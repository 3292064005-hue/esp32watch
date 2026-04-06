#include "display.h"
#include "esp32_port_config.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <string.h>

static const uint16_t kDisplayWidth = 128U;
static const uint16_t kDisplayHeight = 64U;
static const size_t kDisplayBufferSize = (size_t)kDisplayWidth * (size_t)kDisplayHeight / 8U;
static const uint8_t kDisplayAddr7bit = 0x3CU;

static Adafruit_SSD1306 g_display(kDisplayWidth, kDisplayHeight, &Wire, -1);
static bool g_display_ready = false;

extern "C" {

static void display_backend_begin_wire(void)
{
#if (ESP32_I2C_SDA_GPIO >= 0) && (ESP32_I2C_SCL_GPIO >= 0)
    Wire.begin(ESP32_I2C_SDA_GPIO, ESP32_I2C_SCL_GPIO);
#else
    Wire.begin();
#endif
}

bool display_backend_init(void)
{
    display_backend_begin_wire();
    g_display_ready = g_display.begin(SSD1306_SWITCHCAPVCC, kDisplayAddr7bit, false, false);
    if (!g_display_ready) {
        return false;
    }

    g_display.clearDisplay();
    g_display.display();
    g_display.setTextWrap(false);
    g_display.setTextColor(SSD1306_WHITE);
    g_display.setRotation(0);
    return true;
}

void display_backend_set_contrast(uint8_t value)
{
    if (!g_display_ready) {
        return;
    }
    g_display.ssd1306_command(SSD1306_SETCONTRAST);
    g_display.ssd1306_command(value);
}

void display_backend_set_sleep(bool sleep)
{
    if (!g_display_ready) {
        return;
    }
    g_display.ssd1306_command(sleep ? SSD1306_DISPLAYOFF : SSD1306_DISPLAYON);
}

bool display_backend_flush(const uint8_t *buffer)
{
    if (!g_display_ready || buffer == NULL) {
        return false;
    }

    // Our framebuffer is already in SSD1306 page layout, so copy it directly.
    memcpy(g_display.getBuffer(), buffer, kDisplayBufferSize);
    g_display.display();
    return true;
}

} // extern "C"
