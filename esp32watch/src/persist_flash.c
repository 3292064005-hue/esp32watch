#include "persist_flash.h"
#include "app_config.h"
#include "board_storage_map.h"
#include "common/storage_codec.h"
#include "persist_flash_internal.h"
#include "common/crc16.h"
#include <string.h>

#if APP_STORAGE_USE_FLASH
#include "platform_api.h"

_Static_assert((sizeof(FlashStorageRecord) % 2U) == 0U, "FlashStorageRecord must be halfword aligned in size");
_Static_assert(sizeof(FlashStorageRecord) <= FLASH_STORAGE_PAGE_SIZE, "FlashStorageRecord must fit in one flash page");

static FlashStoragePayload g_shadow;
static uint32_t g_active_address;
static uint32_t g_active_sequence;
static bool g_initialized;
static uint16_t g_stored_crc;
static uint16_t g_calculated_crc;

typedef struct {
    PersistFlashCommitPhase phase;
    uint32_t target_address;
    uint32_t previous_address;
    FlashStorageRecord record;
    FlashStoragePayload next_payload;
} PersistFlashCommitContext;

static PersistFlashCommitContext g_commit_ctx;

static uint32_t other_page(uint32_t address)
{
    return address == FLASH_STORAGE_PAGE0_ADDRESS ? FLASH_STORAGE_PAGE1_ADDRESS : FLASH_STORAGE_PAGE0_ADDRESS;
}

static bool flash_unlock(void)
{
    return platform_flash_unlock() == HAL_OK;
}

static void flash_lock(void)
{
    platform_flash_lock();
}

static bool flash_erase_page(uint32_t address)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = address;
    erase.NbPages = 1U;
    return platform_flash_erase_pages(&erase, &page_error) == HAL_OK;
}

static bool flash_program_halfwords(uint32_t address, const uint16_t *data, uint32_t halfwords)
{
    for (uint32_t i = 0; i < halfwords; ++i) {
        if (platform_flash_program_halfword(FLASH_TYPEPROGRAM_HALFWORD, address + i * 2U, data[i]) != HAL_OK) {
            return false;
        }
    }
    return true;
}

static bool flash_program_halfword(uint32_t address, uint16_t value)
{
    return platform_flash_program_halfword(FLASH_TYPEPROGRAM_HALFWORD, address, value) == HAL_OK;
}

static bool persist_flash_finalize_receive(uint32_t address)
{
    bool ok;

    if (!flash_unlock()) {
        return false;
    }
    ok = flash_program_halfword(address, FLASH_RECORD_STATE_VALID);
    flash_lock();
    return ok;
}

static void persist_flash_recover_journal(FlashPageInfo *selected, const FlashPageInfo *peer)
{
    if (selected == NULL) {
        return;
    }

    if (selected->receive) {
        if (persist_flash_finalize_receive(selected->address)) {
            selected->receive = false;
            selected->valid = true;
        }
    }

    if (peer != NULL && peer->receive && !selected->receive && selected->sequence >= peer->sequence) {
        if (flash_unlock()) {
            (void)flash_erase_page(peer->address);
            flash_lock();
        }
    }
}

static void read_page_info(uint32_t address, FlashPageInfo *info)
{
    const FlashStorageRecord *rec = (const FlashStorageRecord *)address;
    memset(info, 0, sizeof(*info));
    info->address = address;
    (void)persist_flash_validate_record(rec, info);
}

static void adopt_page(const FlashPageInfo *info)
{
    g_shadow = info->payload;
    g_active_address = info->address;
    g_active_sequence = info->sequence;
    g_initialized = true;
    g_stored_crc = info->stored_crc;
    g_calculated_crc = info->calc_crc;
}

static void persist_flash_reset_commit_context(void)
{
    memset(&g_commit_ctx, 0, sizeof(g_commit_ctx));
    g_commit_ctx.phase = PERSIST_FLASH_COMMIT_IDLE;
}

static bool persist_flash_finalize_commit(const FlashPageInfo *info)
{
    if (info == NULL || !info->valid) {
        return false;
    }

    adopt_page(info);
    persist_flash_reset_commit_context();
    return true;
}

static bool persist_flash_prepare_payload_record(const FlashStoragePayload *next_payload)
{
    if (next_payload == NULL || g_commit_ctx.phase != PERSIST_FLASH_COMMIT_IDLE) {
        return false;
    }

    memset(&g_commit_ctx, 0, sizeof(g_commit_ctx));
    g_commit_ctx.phase = PERSIST_FLASH_COMMIT_ERASE_TARGET;
    g_commit_ctx.target_address = other_page(g_active_address == 0U ? FLASH_STORAGE_PAGE1_ADDRESS : g_active_address);
    g_commit_ctx.previous_address = g_active_address;
    g_commit_ctx.next_payload = *next_payload;

    memset(&g_commit_ctx.record, 0xFF, sizeof(g_commit_ctx.record));
    g_commit_ctx.record.state = FLASH_RECORD_STATE_RX;
    g_commit_ctx.record.version = APP_STORAGE_VERSION;
    g_commit_ctx.record.magic = FLASH_STORAGE_MAGIC;
    g_commit_ctx.record.sequence = g_active_sequence + 1U;
    g_commit_ctx.record.length = sizeof(FlashStoragePayload);
    g_commit_ctx.record.payload = *next_payload;
    g_commit_ctx.record.crc = crc16_buf((const uint8_t *)&g_commit_ctx.record.payload, sizeof(g_commit_ctx.record.payload));
    return true;
}

void persist_flash_init(void)
{
    FlashPageInfo p0, p1;
    FlashPageInfo selected;
    bool has_selected = false;

    memset(&g_shadow, 0, sizeof(g_shadow));
    g_active_address = 0U;
    g_active_sequence = 0U;
    g_initialized = false;
    g_stored_crc = 0U;
    g_calculated_crc = 0U;
    persist_flash_reset_commit_context();

    read_page_info(FLASH_STORAGE_PAGE0_ADDRESS, &p0);
    read_page_info(FLASH_STORAGE_PAGE1_ADDRESS, &p1);

    if (p0.valid && (!p1.valid || p0.sequence >= p1.sequence)) {
        selected = p0;
        has_selected = true;
    } else if (p1.valid && (!p0.valid || p1.sequence >= p0.sequence)) {
        selected = p1;
        has_selected = true;
    } else if (p0.receive && (!p1.receive || p0.sequence >= p1.sequence)) {
        selected = p0;
        has_selected = true;
        persist_flash_recover_journal(&selected, &p1);
    } else if (p1.receive && (!p0.receive || p1.sequence >= p0.sequence)) {
        selected = p1;
        has_selected = true;
        persist_flash_recover_journal(&selected, &p0);
    }

    if (has_selected) {
        adopt_page(&selected);
    }
}

bool persist_flash_is_initialized(void)
{
    return g_initialized;
}

uint8_t persist_flash_get_version(void)
{
    return g_initialized ? APP_STORAGE_VERSION : 0U;
}

uint16_t persist_flash_get_stored_crc(void)
{
    return g_stored_crc;
}

uint16_t persist_flash_get_calculated_crc(void)
{
    return g_calculated_crc;
}

bool persist_flash_commit_all(const SettingsState *settings,
                              const AlarmState *alarms,
                              uint8_t count,
                              const SensorCalibrationData *cal)
{
    if (!persist_flash_begin_commit(settings, alarms, count, cal)) {
        return false;
    }

    for (uint8_t guard = 0U; guard < 8U; ++guard) {
        PersistFlashCommitStepResult result = persist_flash_commit_step();
        if (result == PERSIST_FLASH_COMMIT_STEP_DONE_OK) {
            return true;
        }
        if (result == PERSIST_FLASH_COMMIT_STEP_DONE_FAIL) {
            return false;
        }
    }

    persist_flash_abort_commit();
    return false;
}

bool persist_flash_begin_commit(const SettingsState *settings,
                                const AlarmState *alarms,
                                uint8_t count,
                                const SensorCalibrationData *cal)
{
    AlarmState local_alarms[APP_MAX_ALARMS] = {0};
    SettingsState local_settings;
    SensorCalibrationData local_cal;
    FlashStoragePayload next_payload;

    if (settings == NULL || alarms == NULL || cal == NULL) {
        return false;
    }
    if (count > APP_MAX_ALARMS) {
        count = APP_MAX_ALARMS;
    }

    memset(&local_settings, 0, sizeof(local_settings));
    local_settings = *settings;
    local_cal = *cal;
    for (uint8_t i = 0; i < count; ++i) {
        local_alarms[i] = alarms[i];
    }

    persist_flash_payload_from_runtime(&next_payload, &local_settings, local_alarms, &local_cal);
    return persist_flash_prepare_payload_record(&next_payload);
}

PersistFlashCommitStepResult persist_flash_commit_step(void)
{
    FlashPageInfo info;
    bool ok;

    switch (g_commit_ctx.phase) {
        case PERSIST_FLASH_COMMIT_ERASE_TARGET:
            if (!flash_unlock()) {
                g_commit_ctx.phase = PERSIST_FLASH_COMMIT_DONE_FAIL;
                return PERSIST_FLASH_COMMIT_STEP_DONE_FAIL;
            }
            ok = flash_erase_page(g_commit_ctx.target_address);
            flash_lock();
            g_commit_ctx.phase = ok ? PERSIST_FLASH_COMMIT_PROGRAM_RECORD : PERSIST_FLASH_COMMIT_DONE_FAIL;
            return ok ? PERSIST_FLASH_COMMIT_STEP_IN_PROGRESS : PERSIST_FLASH_COMMIT_STEP_DONE_FAIL;
        case PERSIST_FLASH_COMMIT_PROGRAM_RECORD:
            if (!flash_unlock()) {
                g_commit_ctx.phase = PERSIST_FLASH_COMMIT_DONE_FAIL;
                return PERSIST_FLASH_COMMIT_STEP_DONE_FAIL;
            }
            ok = flash_program_halfwords(g_commit_ctx.target_address,
                                         (const uint16_t *)&g_commit_ctx.record,
                                         (uint32_t)(sizeof(g_commit_ctx.record) / 2U));
            flash_lock();
            g_commit_ctx.phase = ok ? PERSIST_FLASH_COMMIT_MARK_VALID : PERSIST_FLASH_COMMIT_DONE_FAIL;
            return ok ? PERSIST_FLASH_COMMIT_STEP_IN_PROGRESS : PERSIST_FLASH_COMMIT_STEP_DONE_FAIL;
        case PERSIST_FLASH_COMMIT_MARK_VALID:
            if (!flash_unlock()) {
                g_commit_ctx.phase = PERSIST_FLASH_COMMIT_DONE_FAIL;
                return PERSIST_FLASH_COMMIT_STEP_DONE_FAIL;
            }
            ok = flash_program_halfword(g_commit_ctx.target_address, FLASH_RECORD_STATE_VALID);
            flash_lock();
            if (!ok) {
                g_commit_ctx.phase = PERSIST_FLASH_COMMIT_DONE_FAIL;
                return PERSIST_FLASH_COMMIT_STEP_DONE_FAIL;
            }
            g_commit_ctx.phase = (g_commit_ctx.previous_address != 0U) ? PERSIST_FLASH_COMMIT_ERASE_PREVIOUS : PERSIST_FLASH_COMMIT_VERIFY;
            return PERSIST_FLASH_COMMIT_STEP_IN_PROGRESS;
        case PERSIST_FLASH_COMMIT_ERASE_PREVIOUS:
            if (!flash_unlock()) {
                g_commit_ctx.phase = PERSIST_FLASH_COMMIT_DONE_FAIL;
                return PERSIST_FLASH_COMMIT_STEP_DONE_FAIL;
            }
            ok = flash_erase_page(g_commit_ctx.previous_address);
            flash_lock();
            g_commit_ctx.phase = ok ? PERSIST_FLASH_COMMIT_VERIFY : PERSIST_FLASH_COMMIT_DONE_FAIL;
            return ok ? PERSIST_FLASH_COMMIT_STEP_IN_PROGRESS : PERSIST_FLASH_COMMIT_STEP_DONE_FAIL;
        case PERSIST_FLASH_COMMIT_VERIFY:
            read_page_info(g_commit_ctx.target_address, &info);
            if (!info.valid || info.sequence != g_commit_ctx.record.sequence) {
                g_commit_ctx.phase = PERSIST_FLASH_COMMIT_DONE_FAIL;
                return PERSIST_FLASH_COMMIT_STEP_DONE_FAIL;
            }
            return persist_flash_finalize_commit(&info) ?
                   PERSIST_FLASH_COMMIT_STEP_DONE_OK :
                   PERSIST_FLASH_COMMIT_STEP_DONE_FAIL;
        case PERSIST_FLASH_COMMIT_DONE_OK:
            persist_flash_reset_commit_context();
            return PERSIST_FLASH_COMMIT_STEP_DONE_OK;
        case PERSIST_FLASH_COMMIT_DONE_FAIL:
            persist_flash_reset_commit_context();
            return PERSIST_FLASH_COMMIT_STEP_DONE_FAIL;
        default:
            return PERSIST_FLASH_COMMIT_STEP_DONE_FAIL;
    }
}

PersistFlashCommitPhase persist_flash_commit_phase(void)
{
    return g_commit_ctx.phase;
}

const char *persist_flash_commit_phase_name(PersistFlashCommitPhase phase)
{
    switch (phase) {
        case PERSIST_FLASH_COMMIT_ERASE_TARGET: return "ERASE";
        case PERSIST_FLASH_COMMIT_PROGRAM_RECORD: return "PROGRAM";
        case PERSIST_FLASH_COMMIT_MARK_VALID: return "MARK";
        case PERSIST_FLASH_COMMIT_ERASE_PREVIOUS: return "ERASE_OLD";
        case PERSIST_FLASH_COMMIT_VERIFY: return "VERIFY";
        case PERSIST_FLASH_COMMIT_DONE_OK: return "DONE";
        case PERSIST_FLASH_COMMIT_DONE_FAIL: return "FAIL";
        default: return "IDLE";
    }
}

void persist_flash_abort_commit(void)
{
    persist_flash_reset_commit_context();
}

void persist_flash_save_settings(const SettingsState *settings)
{
    SensorCalibrationData cal;
    AlarmState alarms[APP_MAX_ALARMS] = {0};
    persist_flash_load_sensor_calibration(&cal);
    persist_flash_load_alarms(alarms, APP_MAX_ALARMS);
    (void)persist_flash_commit_all(settings, alarms, APP_MAX_ALARMS, &cal);
}

void persist_flash_load_settings(SettingsState *settings)
{
    if (settings == NULL) return;
    persist_flash_seed_default_settings(settings);
    if (!g_initialized) return;
    persist_flash_payload_to_settings(&g_shadow, settings);
}

void persist_flash_save_alarms(const AlarmState *alarms, uint8_t count)
{
    SensorCalibrationData cal;
    SettingsState settings;
    AlarmState local[APP_MAX_ALARMS] = {0};
    if (count > APP_MAX_ALARMS) count = APP_MAX_ALARMS;
    persist_flash_load_sensor_calibration(&cal);
    memset(&settings, 0, sizeof(settings));
    persist_flash_load_settings(&settings);
    for (uint8_t i = 0; i < count; ++i) {
        local[i] = alarms[i];
    }
    (void)persist_flash_commit_all(&settings, local, APP_MAX_ALARMS, &cal);
}

void persist_flash_load_alarms(AlarmState *alarms, uint8_t count)
{
    if (alarms == NULL || count == 0U) return;
    if (count > APP_MAX_ALARMS) count = APP_MAX_ALARMS;
    for (uint8_t i = 0; i < count; ++i) {
        persist_flash_seed_default_alarm(&alarms[i], i);
    }
    if (!g_initialized) return;
    persist_flash_payload_to_alarms(&g_shadow, alarms, count);
}

void persist_flash_save_sensor_calibration(const SensorCalibrationData *cal)
{
    SettingsState settings;
    AlarmState alarms[APP_MAX_ALARMS] = {0};
    memset(&settings, 0, sizeof(settings));
    persist_flash_load_settings(&settings);
    persist_flash_load_alarms(alarms, APP_MAX_ALARMS);
    (void)persist_flash_commit_all(&settings, alarms, APP_MAX_ALARMS, cal);
}

void persist_flash_load_sensor_calibration(SensorCalibrationData *cal)
{
    if (cal == NULL) return;
    memset(cal, 0, sizeof(*cal));
    if (!g_initialized) return;
    persist_flash_payload_to_calibration(&g_shadow, cal);
}

uint32_t persist_flash_get_wear_count(void)
{
    return g_active_sequence;
}
#else
void persist_flash_init(void) {}
bool persist_flash_is_initialized(void) { return false; }
uint8_t persist_flash_get_version(void) { return 0U; }
uint16_t persist_flash_get_stored_crc(void) { return 0U; }
uint16_t persist_flash_get_calculated_crc(void) { return 0U; }
bool persist_flash_commit_all(const SettingsState *settings,
                              const AlarmState *alarms,
                              uint8_t count,
                              const SensorCalibrationData *cal)
{
    (void)settings;
    (void)alarms;
    (void)count;
    (void)cal;
    return false;
}
bool persist_flash_begin_commit(const SettingsState *settings,
                                const AlarmState *alarms,
                                uint8_t count,
                                const SensorCalibrationData *cal)
{
    (void)settings;
    (void)alarms;
    (void)count;
    (void)cal;
    return false;
}
PersistFlashCommitStepResult persist_flash_commit_step(void) { return PERSIST_FLASH_COMMIT_STEP_DONE_FAIL; }
PersistFlashCommitPhase persist_flash_commit_phase(void) { return PERSIST_FLASH_COMMIT_IDLE; }
const char *persist_flash_commit_phase_name(PersistFlashCommitPhase phase) { (void)phase; return "IDLE"; }
void persist_flash_abort_commit(void) {}
void persist_flash_save_settings(const SettingsState *settings) { (void)settings; }
void persist_flash_load_settings(SettingsState *settings) { (void)settings; }
void persist_flash_save_alarms(const AlarmState *alarms, uint8_t count) { (void)alarms; (void)count; }
void persist_flash_load_alarms(AlarmState *alarms, uint8_t count) { (void)alarms; (void)count; }
void persist_flash_save_sensor_calibration(const SensorCalibrationData *cal) { (void)cal; }
void persist_flash_load_sensor_calibration(SensorCalibrationData *cal) { memset(cal, 0, sizeof(*cal)); }
uint32_t persist_flash_get_wear_count(void) { return 0U; }
#endif




