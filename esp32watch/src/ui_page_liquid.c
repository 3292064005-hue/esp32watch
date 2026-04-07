#include "ui_internal.h"
#include "display.h"
#include "platform_api.h"
#include <math.h>
#include <string.h>

/*
 * Liquid page rewritten around a compact FLIP-style pipeline inspired by
 * cccAboy/esp32-led-matrix. The internal visible domain is 32x16 so the OLED
 * can fill the full 128x64 display with square rendered cells.
 */

#define FLIP_VISIBLE_W               32
#define FLIP_VISIBLE_H               16
#define FLIP_PADDING                 1
#define FLIP_SIM_W                   (FLIP_VISIBLE_W + (FLIP_PADDING * 2))
#define FLIP_SIM_H                   (FLIP_VISIBLE_H + (FLIP_PADDING * 2))
#define FLIP_SIM_CELLS              (FLIP_SIM_W * FLIP_SIM_H)

#define LIQUID_BLOCK_SIZE            4
#define LIQUID_RENDER_W             (FLIP_VISIBLE_W * LIQUID_BLOCK_SIZE)
#define LIQUID_RENDER_H             (FLIP_VISIBLE_H * LIQUID_BLOCK_SIZE)
#define LIQUID_RENDER_X               0
#define LIQUID_RENDER_Y               0

#define LIQUID_TANK_W                (((float)(FLIP_SIM_W - 1)) / ((float)(FLIP_SIM_H - 1)))
#define LIQUID_TANK_H                1.0f
#define LIQUID_FILL_RATIO            0.60f
#define LIQUID_FILL_WIDTH_RATIO      0.80f

#define FLIP_MAX_PARTICLES          320
#define FLIP_PGRID_W                 24
#define FLIP_PGRID_H                 24
#define FLIP_PGRID_CELLS            (FLIP_PGRID_W * FLIP_PGRID_H)

#define FLIP_GRAVITY_SCALE           9.81f
#define FLIP_FLIP_RATIO              0.90f
#define FLIP_PUSH_ITERS              2
#define FLIP_PRESSURE_ITERS         20
#define FLIP_OVER_RELAXATION         1.90f
#define FLIP_DENSITY_CLAMP           1.20f
#define FLIP_GAMMA                   0.60f
#define FLIP_MAX_DT                  0.050f
#define FLIP_MIN_DT                  0.012f
#define FLIP_TARGET_SUBSTEP          0.016f
#define FLIP_MAX_SUBSTEPS             3

#define LIQUID_LED_MAX_F            20.0f

#define FLIP_AIR_CELL                0
#define FLIP_FLUID_CELL              1
#define FLIP_SOLID_CELL              2

#define FLIP_GRID_INDEX(x, y)       ((x) * FLIP_SIM_H + (y))

typedef struct {
    float u[FLIP_SIM_CELLS];
    float v[FLIP_SIM_CELLS];
    float du[FLIP_SIM_CELLS];
    float dv[FLIP_SIM_CELLS];
    float prev_u[FLIP_SIM_CELLS];
    float prev_v[FLIP_SIM_CELLS];
    float pressure[FLIP_SIM_CELLS];
    float solid[FLIP_SIM_CELLS];
    int32_t cell_type[FLIP_SIM_CELLS];

    float particle_pos[FLIP_MAX_PARTICLES * 2];
    float particle_vel[FLIP_MAX_PARTICLES * 2];
    float particle_density[FLIP_SIM_CELLS];
    float particle_rest_density;

    int32_t num_cell_particles[FLIP_PGRID_CELLS];
    int32_t first_cell_particle[FLIP_PGRID_CELLS + 1];
    int32_t cell_particle_ids[FLIP_MAX_PARTICLES];

    float led_grid[FLIP_VISIBLE_W * FLIP_VISIBLE_H];

    float h;
    float inv_spacing;
    float particle_radius;
    float particle_inv_spacing;
    uint16_t num_particles;
    uint32_t last_ms;
    bool frame_ready;
    bool initialized;
} LiquidPageState;

static LiquidPageState g_liquid;
static bool g_gamma_ready;
static uint8_t g_gamma_lut[256];

static const uint8_t g_bayer4x4[4][4] = {
    {0U,  8U,  2U, 10U},
    {12U, 4U, 14U,  6U},
    {3U, 11U,  1U,  9U},
    {15U, 7U, 13U,  5U},
};

static float clampf_local(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int clamp_i_local(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float hash01_local(uint32_t value)
{
    value ^= value >> 17;
    value *= 0xed5ad4bbU;
    value ^= value >> 11;
    value *= 0xac4c1b51U;
    value ^= value >> 15;
    value *= 0x31848babU;
    value ^= value >> 14;
    return (float)(value & 0xFFFFU) / 65535.0f;
}

static void liquid_gamma_init_once(void)
{
    int i;

    if (g_gamma_ready) {
        return;
    }

    for (i = 0; i < 256; ++i) {
        float x = (float)i / 255.0f;
        g_gamma_lut[i] = (uint8_t)(powf(x, FLIP_GAMMA) * 255.0f + 0.5f);
    }
    g_gamma_ready = true;
}

static void liquid_reset_state(void)
{
    const float h = LIQUID_TANK_H / (float)(FLIP_SIM_H - 1);
    const float particle_radius = 0.35f * h;
    const float dx = particle_radius * 2.0f;
    const float dy = dx * 0.8660254f;
    const int num_x = (int)floorf((LIQUID_FILL_WIDTH_RATIO * LIQUID_TANK_W - 2.0f * h - 2.0f * particle_radius) / dx);
    const int num_y = (int)floorf((LIQUID_FILL_RATIO * LIQUID_TANK_H - 2.0f * h - 2.0f * particle_radius) / dy);
    int particle_index = 0;
    int i;
    int j;

    memset(&g_liquid, 0, sizeof(g_liquid));
    liquid_gamma_init_once();

    g_liquid.h = h;
    g_liquid.inv_spacing = 1.0f / h;
    g_liquid.particle_radius = particle_radius;
    g_liquid.particle_inv_spacing = 1.0f / (2.2f * particle_radius);

    for (i = 0; i < FLIP_SIM_W; ++i) {
        for (j = 0; j < FLIP_SIM_H; ++j) {
            float solid = 1.0f;
            if (i == 0 || i == (FLIP_SIM_W - 1) || j == 0 || j == (FLIP_SIM_H - 1)) {
                solid = 0.0f;
            }
            g_liquid.solid[FLIP_GRID_INDEX(i, j)] = solid;
        }
    }

    for (i = 0; i < num_x && particle_index < FLIP_MAX_PARTICLES; ++i) {
        for (j = 0; j < num_y && particle_index < FLIP_MAX_PARTICLES; ++j) {
            float jitter_x = (hash01_local((uint32_t)(particle_index * 37U + 3U)) - 0.5f) * 0.08f * h;
            float jitter_y = (hash01_local((uint32_t)(particle_index * 53U + 9U)) - 0.5f) * 0.08f * h;
            float px = h + particle_radius + dx * (float)i + ((j & 1) ? particle_radius : 0.0f);
            float py = h + particle_radius + dy * (float)j;

            g_liquid.particle_pos[particle_index * 2] = px + jitter_x;
            g_liquid.particle_pos[particle_index * 2 + 1] = py + jitter_y;
            g_liquid.particle_vel[particle_index * 2] = 0.0f;
            g_liquid.particle_vel[particle_index * 2 + 1] = 0.0f;
            particle_index++;
        }
    }

    g_liquid.num_particles = (uint16_t)particle_index;
    g_liquid.particle_rest_density = 0.0f;
    g_liquid.last_ms = 0U;
    g_liquid.frame_ready = false;
    g_liquid.initialized = true;
}

static void liquid_compute_gravity(const UiSystemSnapshot *snap, float *gx, float *gy)
{
    float tilt_x = 0.0f;
    float tilt_y = 0.0f;
    float gyro_x = 0.0f;
    float gyro_y = 0.0f;
    float len;

    if (gx == NULL || gy == NULL) {
        return;
    }

    if (snap != NULL && snap->sensor.online) {
        tilt_x = clampf_local((float)snap->sensor.roll_deg / 42.0f, -1.0f, 1.0f);
        tilt_y = clampf_local((float)snap->sensor.pitch_deg / 42.0f, -1.0f, 1.0f);
        gyro_x = clampf_local((float)snap->sensor.gx / 18000.0f, -0.25f, 0.25f);
        gyro_y = clampf_local((float)snap->sensor.gy / 18000.0f, -0.25f, 0.25f);
    }

    *gx = tilt_x + gyro_x * 0.55f;
    *gy = 1.0f - tilt_y + gyro_y * 0.55f;

    len = sqrtf((*gx * *gx) + (*gy * *gy));
    if (len > 0.0001f) {
        *gx /= len;
        *gy /= len;
    } else {
        *gx = 0.0f;
        *gy = 1.0f;
    }

    *gx = clampf_local(*gx, -1.0f, 1.0f);
    *gy = clampf_local(*gy, -1.2f, 1.2f);
}

static void liquid_integrate_particles(float dt, float gx, float gy)
{
    uint16_t i;

    for (i = 0U; i < g_liquid.num_particles; ++i) {
        g_liquid.particle_vel[i * 2] += gx * FLIP_GRAVITY_SCALE * dt;
        g_liquid.particle_vel[i * 2 + 1] += gy * FLIP_GRAVITY_SCALE * dt;
        g_liquid.particle_pos[i * 2] += g_liquid.particle_vel[i * 2] * dt;
        g_liquid.particle_pos[i * 2 + 1] += g_liquid.particle_vel[i * 2 + 1] * dt;
    }
}

static void liquid_push_particles_apart(void)
{
    const float min_dist = 2.0f * g_liquid.particle_radius;
    const float min_dist_sq = min_dist * min_dist;
    int i;
    int iter;

    memset(g_liquid.num_cell_particles, 0, sizeof(g_liquid.num_cell_particles));

    for (i = 0; i < g_liquid.num_particles; ++i) {
        float x = g_liquid.particle_pos[i * 2];
        float y = g_liquid.particle_pos[i * 2 + 1];
        int xi = clamp_i_local((int)(x * g_liquid.particle_inv_spacing), 0, FLIP_PGRID_W - 1);
        int yi = clamp_i_local((int)(y * g_liquid.particle_inv_spacing), 0, FLIP_PGRID_H - 1);
        int cell = xi * FLIP_PGRID_H + yi;
        g_liquid.num_cell_particles[cell]++;
    }

    {
        int first = 0;
        for (i = 0; i < FLIP_PGRID_CELLS; ++i) {
            first += g_liquid.num_cell_particles[i];
            g_liquid.first_cell_particle[i] = first;
        }
        g_liquid.first_cell_particle[FLIP_PGRID_CELLS] = first;
    }

    for (i = 0; i < g_liquid.num_particles; ++i) {
        float x = g_liquid.particle_pos[i * 2];
        float y = g_liquid.particle_pos[i * 2 + 1];
        int xi = clamp_i_local((int)(x * g_liquid.particle_inv_spacing), 0, FLIP_PGRID_W - 1);
        int yi = clamp_i_local((int)(y * g_liquid.particle_inv_spacing), 0, FLIP_PGRID_H - 1);
        int cell = xi * FLIP_PGRID_H + yi;
        g_liquid.first_cell_particle[cell]--;
        g_liquid.cell_particle_ids[g_liquid.first_cell_particle[cell]] = i;
    }

    for (iter = 0; iter < FLIP_PUSH_ITERS; ++iter) {
        for (i = 0; i < g_liquid.num_particles; ++i) {
            float px = g_liquid.particle_pos[i * 2];
            float py = g_liquid.particle_pos[i * 2 + 1];
            int pxi = clamp_i_local((int)(px * g_liquid.particle_inv_spacing), 0, FLIP_PGRID_W - 1);
            int pyi = clamp_i_local((int)(py * g_liquid.particle_inv_spacing), 0, FLIP_PGRID_H - 1);
            int x0 = (pxi > 0) ? (pxi - 1) : 0;
            int y0 = (pyi > 0) ? (pyi - 1) : 0;
            int x1 = (pxi + 1 < FLIP_PGRID_W) ? (pxi + 1) : (FLIP_PGRID_W - 1);
            int y1 = (pyi + 1 < FLIP_PGRID_H) ? (pyi + 1) : (FLIP_PGRID_H - 1);
            int xi;
            int yi;

            for (xi = x0; xi <= x1; ++xi) {
                for (yi = y0; yi <= y1; ++yi) {
                    int cell = xi * FLIP_PGRID_H + yi;
                    int a = g_liquid.first_cell_particle[cell];
                    int b = g_liquid.first_cell_particle[cell + 1];
                    int k;

                    for (k = a; k < b; ++k) {
                        int id = g_liquid.cell_particle_ids[k];

                        if (id == i) {
                            continue;
                        }

                        {
                            float qx = g_liquid.particle_pos[id * 2];
                            float qy = g_liquid.particle_pos[id * 2 + 1];
                            float dx = qx - px;
                            float dy = qy - py;
                            float dist_sq = dx * dx + dy * dy;

                            if (dist_sq > min_dist_sq || dist_sq <= 0.0f) {
                                continue;
                            }

                            {
                                float dist = sqrtf(dist_sq);
                                float scale = (0.5f * (min_dist - dist)) / dist;

                                dx *= scale;
                                dy *= scale;
                                g_liquid.particle_pos[i * 2] -= dx;
                                g_liquid.particle_pos[i * 2 + 1] -= dy;
                                g_liquid.particle_pos[id * 2] += dx;
                                g_liquid.particle_pos[id * 2 + 1] += dy;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void liquid_handle_particle_collisions(void)
{
    const float min_x = g_liquid.h + g_liquid.particle_radius;
    const float max_x = (float)(FLIP_SIM_W - 1) * g_liquid.h - g_liquid.particle_radius;
    const float min_y = g_liquid.h + g_liquid.particle_radius;
    const float max_y = (float)(FLIP_SIM_H - 1) * g_liquid.h - g_liquid.particle_radius;
    uint16_t i;

    for (i = 0U; i < g_liquid.num_particles; ++i) {
        float *x = &g_liquid.particle_pos[i * 2];
        float *y = &g_liquid.particle_pos[i * 2 + 1];
        float *vx = &g_liquid.particle_vel[i * 2];
        float *vy = &g_liquid.particle_vel[i * 2 + 1];

        if (*x < min_x) {
            *x = min_x;
            if (*vx < 0.0f) {
                *vx = -*vx * 0.55f;
            }
            *vy *= 0.98f;
        } else if (*x > max_x) {
            *x = max_x;
            if (*vx > 0.0f) {
                *vx = -*vx * 0.55f;
            }
            *vy *= 0.98f;
        }

        if (*y < min_y) {
            *y = min_y;
            if (*vy < 0.0f) {
                *vy = -*vy * 0.45f;
            }
        } else if (*y > max_y) {
            *y = max_y;
            if (*vy > 0.0f) {
                *vy = -*vy * 0.35f;
            }
            *vx *= 0.96f;
        }
    }
}

static void liquid_transfer_velocities(bool to_grid)
{
    int component;
    int i;

    if (to_grid) {
        memcpy(g_liquid.prev_u, g_liquid.u, sizeof(g_liquid.u));
        memcpy(g_liquid.prev_v, g_liquid.v, sizeof(g_liquid.v));
        memset(g_liquid.du, 0, sizeof(g_liquid.du));
        memset(g_liquid.dv, 0, sizeof(g_liquid.dv));
        memset(g_liquid.u, 0, sizeof(g_liquid.u));
        memset(g_liquid.v, 0, sizeof(g_liquid.v));

        for (i = 0; i < FLIP_SIM_CELLS; ++i) {
            g_liquid.cell_type[i] = (g_liquid.solid[i] == 0.0f) ? FLIP_SOLID_CELL : FLIP_AIR_CELL;
        }

        for (i = 0; i < g_liquid.num_particles; ++i) {
            int xi = clamp_i_local((int)(g_liquid.particle_pos[i * 2] * g_liquid.inv_spacing), 0, FLIP_SIM_W - 1);
            int yi = clamp_i_local((int)(g_liquid.particle_pos[i * 2 + 1] * g_liquid.inv_spacing), 0, FLIP_SIM_H - 1);
            int cell = FLIP_GRID_INDEX(xi, yi);
            if (g_liquid.cell_type[cell] == FLIP_AIR_CELL) {
                g_liquid.cell_type[cell] = FLIP_FLUID_CELL;
            }
        }
    }

    for (component = 0; component < 2; ++component) {
        const float dx = (component == 0) ? 0.0f : (0.5f * g_liquid.h);
        const float dy = (component == 0) ? (0.5f * g_liquid.h) : 0.0f;
        float *field = (component == 0) ? g_liquid.u : g_liquid.v;
        float *prev_field = (component == 0) ? g_liquid.prev_u : g_liquid.prev_v;
        float *weight = (component == 0) ? g_liquid.du : g_liquid.dv;

        for (i = 0; i < g_liquid.num_particles; ++i) {
            float x = clampf_local(g_liquid.particle_pos[i * 2], g_liquid.h, (float)(FLIP_SIM_W - 1) * g_liquid.h);
            float y = clampf_local(g_liquid.particle_pos[i * 2 + 1], g_liquid.h, (float)(FLIP_SIM_H - 1) * g_liquid.h);
            int x0 = clamp_i_local((int)((x - dx) * g_liquid.inv_spacing), 0, FLIP_SIM_W - 2);
            int y0 = clamp_i_local((int)((y - dy) * g_liquid.inv_spacing), 0, FLIP_SIM_H - 2);
            int x1 = x0 + 1;
            int y1 = y0 + 1;
            float tx = clampf_local((x - dx - (float)x0 * g_liquid.h) * g_liquid.inv_spacing, 0.0f, 1.0f);
            float ty = clampf_local((y - dy - (float)y0 * g_liquid.h) * g_liquid.inv_spacing, 0.0f, 1.0f);
            float sx = 1.0f - tx;
            float sy = 1.0f - ty;
            float w0 = sx * sy;
            float w1 = tx * sy;
            float w2 = tx * ty;
            float w3 = sx * ty;
            int idx0 = FLIP_GRID_INDEX(x0, y0);
            int idx1 = FLIP_GRID_INDEX(x1, y0);
            int idx2 = FLIP_GRID_INDEX(x1, y1);
            int idx3 = FLIP_GRID_INDEX(x0, y1);

            if (to_grid) {
                float pv = g_liquid.particle_vel[i * 2 + component];
                field[idx0] += pv * w0;
                field[idx1] += pv * w1;
                field[idx2] += pv * w2;
                field[idx3] += pv * w3;
                weight[idx0] += w0;
                weight[idx1] += w1;
                weight[idx2] += w2;
                weight[idx3] += w3;
            } else {
                int offset = (component == 0) ? FLIP_SIM_H : 1;
                float valid0 = (g_liquid.cell_type[idx0] != FLIP_AIR_CELL ||
                                g_liquid.cell_type[clamp_i_local(idx0 - offset, 0, FLIP_SIM_CELLS - 1)] != FLIP_AIR_CELL) ? 1.0f : 0.0f;
                float valid1 = (g_liquid.cell_type[idx1] != FLIP_AIR_CELL ||
                                g_liquid.cell_type[clamp_i_local(idx1 - offset, 0, FLIP_SIM_CELLS - 1)] != FLIP_AIR_CELL) ? 1.0f : 0.0f;
                float valid2 = (g_liquid.cell_type[idx2] != FLIP_AIR_CELL ||
                                g_liquid.cell_type[clamp_i_local(idx2 - offset, 0, FLIP_SIM_CELLS - 1)] != FLIP_AIR_CELL) ? 1.0f : 0.0f;
                float valid3 = (g_liquid.cell_type[idx3] != FLIP_AIR_CELL ||
                                g_liquid.cell_type[clamp_i_local(idx3 - offset, 0, FLIP_SIM_CELLS - 1)] != FLIP_AIR_CELL) ? 1.0f : 0.0f;
                float sum = valid0 * w0 + valid1 * w1 + valid2 * w2 + valid3 * w3;

                if (sum > 0.0f) {
                    float pic = (valid0 * w0 * field[idx0] +
                                 valid1 * w1 * field[idx1] +
                                 valid2 * w2 * field[idx2] +
                                 valid3 * w3 * field[idx3]) / sum;
                    float corr = (valid0 * w0 * (field[idx0] - prev_field[idx0]) +
                                  valid1 * w1 * (field[idx1] - prev_field[idx1]) +
                                  valid2 * w2 * (field[idx2] - prev_field[idx2]) +
                                  valid3 * w3 * (field[idx3] - prev_field[idx3])) / sum;
                    float flip = g_liquid.particle_vel[i * 2 + component] + corr;

                    g_liquid.particle_vel[i * 2 + component] =
                        (1.0f - FLIP_FLIP_RATIO) * pic + FLIP_FLIP_RATIO * flip;
                }
            }
        }

        if (to_grid) {
            for (i = 0; i < FLIP_SIM_CELLS; ++i) {
                if (weight[i] > 0.0f) {
                    field[i] /= weight[i];
                }
            }

            for (i = 0; i < FLIP_SIM_W; ++i) {
                int j;
                for (j = 0; j < FLIP_SIM_H; ++j) {
                    int idx = FLIP_GRID_INDEX(i, j);
                    bool solid_here = (g_liquid.cell_type[idx] == FLIP_SOLID_CELL);

                    if (solid_here || (i > 0 && g_liquid.cell_type[FLIP_GRID_INDEX(i - 1, j)] == FLIP_SOLID_CELL)) {
                        g_liquid.u[idx] = g_liquid.prev_u[idx];
                    }
                    if (solid_here || (j > 0 && g_liquid.cell_type[FLIP_GRID_INDEX(i, j - 1)] == FLIP_SOLID_CELL)) {
                        g_liquid.v[idx] = g_liquid.prev_v[idx];
                    }
                }
            }
        }
    }
}

static void liquid_update_particle_density(void)
{
    const float h2 = 0.5f * g_liquid.h;
    int i;

    memset(g_liquid.particle_density, 0, sizeof(g_liquid.particle_density));

    for (i = 0; i < g_liquid.num_particles; ++i) {
        float x = clampf_local(g_liquid.particle_pos[i * 2], g_liquid.h, (float)(FLIP_SIM_W - 1) * g_liquid.h);
        float y = clampf_local(g_liquid.particle_pos[i * 2 + 1], g_liquid.h, (float)(FLIP_SIM_H - 1) * g_liquid.h);
        int x0 = clamp_i_local((int)((x - h2) * g_liquid.inv_spacing), 0, FLIP_SIM_W - 2);
        int y0 = clamp_i_local((int)((y - h2) * g_liquid.inv_spacing), 0, FLIP_SIM_H - 2);
        int x1 = x0 + 1;
        int y1 = y0 + 1;
        float tx = clampf_local((x - h2 - (float)x0 * g_liquid.h) * g_liquid.inv_spacing, 0.0f, 1.0f);
        float ty = clampf_local((y - h2 - (float)y0 * g_liquid.h) * g_liquid.inv_spacing, 0.0f, 1.0f);
        float sx = 1.0f - tx;
        float sy = 1.0f - ty;

        g_liquid.particle_density[FLIP_GRID_INDEX(x0, y0)] += sx * sy;
        g_liquid.particle_density[FLIP_GRID_INDEX(x1, y0)] += tx * sy;
        g_liquid.particle_density[FLIP_GRID_INDEX(x1, y1)] += tx * ty;
        g_liquid.particle_density[FLIP_GRID_INDEX(x0, y1)] += sx * ty;
    }
}

static float liquid_calculate_rest_density(void)
{
    float sum = 0.0f;
    int count = 0;
    int i;

    for (i = 0; i < FLIP_SIM_CELLS; ++i) {
        if (g_liquid.cell_type[i] == FLIP_FLUID_CELL) {
            sum += g_liquid.particle_density[i];
            count++;
        }
    }
    return (count > 0) ? (sum / (float)count) : 0.0f;
}

static void liquid_solve_incompressibility(float dt)
{
    const float cp = (1000.0f * g_liquid.h) / dt;
    int iter;

    memset(g_liquid.pressure, 0, sizeof(g_liquid.pressure));
    memcpy(g_liquid.prev_u, g_liquid.u, sizeof(g_liquid.u));
    memcpy(g_liquid.prev_v, g_liquid.v, sizeof(g_liquid.v));

    for (iter = 0; iter < FLIP_PRESSURE_ITERS; ++iter) {
        int i;
        int j;
        for (i = 1; i < FLIP_SIM_W - 1; ++i) {
            for (j = 1; j < FLIP_SIM_H - 1; ++j) {
                int center = FLIP_GRID_INDEX(i, j);
                int left = FLIP_GRID_INDEX(i - 1, j);
                int right = FLIP_GRID_INDEX(i + 1, j);
                int bottom = FLIP_GRID_INDEX(i, j - 1);
                int top = FLIP_GRID_INDEX(i, j + 1);
                float sx0;
                float sx1;
                float sy0;
                float sy1;
                float s_sum;
                float div;
                float p;

                if (g_liquid.cell_type[center] != FLIP_FLUID_CELL) {
                    continue;
                }

                sx0 = g_liquid.solid[left];
                sx1 = g_liquid.solid[right];
                sy0 = g_liquid.solid[bottom];
                sy1 = g_liquid.solid[top];
                s_sum = sx0 + sx1 + sy0 + sy1;
                if (s_sum == 0.0f) {
                    continue;
                }

                div = (g_liquid.u[right] - g_liquid.u[center]) +
                      (g_liquid.v[top] - g_liquid.v[center]);

                if (g_liquid.particle_rest_density > 0.0f) {
                    float compression = g_liquid.particle_density[center] - g_liquid.particle_rest_density;
                    if (compression > 0.0f) {
                        div -= compression;
                    }
                }

                p = (-div / s_sum) * FLIP_OVER_RELAXATION;
                g_liquid.pressure[center] += cp * p;

                g_liquid.u[center] -= sx0 * p;
                g_liquid.u[right] += sx1 * p;
                g_liquid.v[center] -= sy0 * p;
                g_liquid.v[top] += sy1 * p;
            }
        }
    }
}

static void liquid_step_once(float dt, float gx, float gy)
{
    liquid_integrate_particles(dt, gx, gy);
    liquid_push_particles_apart();
    liquid_handle_particle_collisions();
    liquid_transfer_velocities(true);
    liquid_update_particle_density();

    if (g_liquid.particle_rest_density <= 0.0f) {
        g_liquid.particle_rest_density = liquid_calculate_rest_density();
    }

    liquid_solve_incompressibility(dt);
    liquid_transfer_velocities(false);
}

static void liquid_step_simulation(const UiSystemSnapshot *snap)
{
    uint32_t now_ms = platform_time_now_ms();
    float dt;
    float gx;
    float gy;
    int substeps;
    int step;

    if (!g_liquid.initialized) {
        liquid_reset_state();
    }

    if (g_liquid.last_ms == 0U || now_ms <= g_liquid.last_ms) {
        dt = 0.020f;
    } else {
        dt = (float)(now_ms - g_liquid.last_ms) / 1000.0f;
    }
    g_liquid.last_ms = now_ms;

    dt = clampf_local(dt, FLIP_MIN_DT, FLIP_MAX_DT);
    substeps = (int)ceilf(dt / FLIP_TARGET_SUBSTEP);
    substeps = clamp_i_local(substeps, 1, FLIP_MAX_SUBSTEPS);
    dt /= (float)substeps;

    liquid_compute_gravity(snap, &gx, &gy);
    for (step = 0; step < substeps; ++step) {
        liquid_step_once(dt, gx, gy);
    }
}

static void liquid_get_led_grid(void)
{
    const int padding = FLIP_PADDING;
    int x;
    int y;

    for (x = 0; x < FLIP_VISIBLE_W; ++x) {
        for (y = 0; y < FLIP_VISIBLE_H; ++y) {
            int sim_x = x + padding;
            int sim_y = y + padding;
            int idx = FLIP_GRID_INDEX(sim_x, sim_y);
            float density = g_liquid.particle_density[idx];
            float b;
            uint8_t lut_in;

            if (g_liquid.particle_rest_density > 0.0f) {
                density /= g_liquid.particle_rest_density;
            }

            b = clampf_local(density / FLIP_DENSITY_CLAMP, 0.0f, 1.0f);
            lut_in = (uint8_t)(b * 255.0f + 0.5f);
            g_liquid.led_grid[x * FLIP_VISIBLE_H + y] =
                (float)g_gamma_lut[lut_in] * (LIQUID_LED_MAX_F / 255.0f);
        }
    }
}

static void liquid_prime_cached_frame(void)
{
    int i;

    memset(g_liquid.particle_density, 0, sizeof(g_liquid.particle_density));

    for (i = 0; i < FLIP_SIM_CELLS; ++i) {
        g_liquid.cell_type[i] = (g_liquid.solid[i] == 0.0f) ? FLIP_SOLID_CELL : FLIP_AIR_CELL;
    }

    for (i = 0; i < g_liquid.num_particles; ++i) {
        int xi = clamp_i_local((int)(g_liquid.particle_pos[i * 2] * g_liquid.inv_spacing), 0, FLIP_SIM_W - 1);
        int yi = clamp_i_local((int)(g_liquid.particle_pos[i * 2 + 1] * g_liquid.inv_spacing), 0, FLIP_SIM_H - 1);
        int cell = FLIP_GRID_INDEX(xi, yi);

        if (g_liquid.cell_type[cell] == FLIP_AIR_CELL) {
            g_liquid.cell_type[cell] = FLIP_FLUID_CELL;
        }
    }

    liquid_update_particle_density();
    if (g_liquid.particle_rest_density <= 0.0f) {
        g_liquid.particle_rest_density = liquid_calculate_rest_density();
    }
    liquid_get_led_grid();
    g_liquid.frame_ready = true;
}

static void liquid_draw_led_grid(int16_t ox)
{
    int cell_x;
    int cell_y;

    for (cell_x = 0; cell_x < FLIP_VISIBLE_W; ++cell_x) {
        for (cell_y = 0; cell_y < FLIP_VISIBLE_H; ++cell_y) {
            float level20 = g_liquid.led_grid[cell_x * FLIP_VISIBLE_H + cell_y];
            float threshold_level = clampf_local(level20 / LIQUID_LED_MAX_F, 0.0f, 1.0f) * 16.0f;
            int px;
            int py;

            for (py = 0; py < LIQUID_BLOCK_SIZE; ++py) {
                for (px = 0; px < LIQUID_BLOCK_SIZE; ++px) {
                    if (threshold_level > (float)g_bayer4x4[py][px]) {
                        display_draw_pixel(ox + LIQUID_RENDER_X + cell_x * LIQUID_BLOCK_SIZE + px,
                                           LIQUID_RENDER_Y + cell_y * LIQUID_BLOCK_SIZE + py,
                                           true);
                    }
                }
            }
        }
    }
}

void ui_page_liquid_render(PageId page, int16_t ox)
{
    (void)page;

    if (!g_liquid.initialized) {
        liquid_reset_state();
    }

    if (ui_runtime_is_transition_render()) {
        g_liquid.last_ms = platform_time_now_ms();
        if (!g_liquid.frame_ready) {
            liquid_prime_cached_frame();
        }
    } else {
        UiSystemSnapshot snap;

        ui_get_system_snapshot(&snap);
        liquid_step_simulation(&snap);
        liquid_get_led_grid();
        g_liquid.frame_ready = true;
    }

    liquid_draw_led_grid(ox);
}

bool ui_page_liquid_handle(PageId page, const KeyEvent *e, uint32_t now_ms)
{
    (void)page;

    if (e->type != KEY_EVENT_SHORT) {
        return false;
    }

    if (e->id == KEY_ID_BACK) {
        ui_core_go(PAGE_APPS, -1, now_ms);
        return true;
    }

    if (e->id == KEY_ID_OK) {
        liquid_reset_state();
        ui_core_mark_dirty();
        return true;
    }

    if (e->id == KEY_ID_UP || e->id == KEY_ID_DOWN) {
        return true;
    }

    return false;
}
