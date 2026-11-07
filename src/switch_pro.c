#include "switch_pro.h"
#include <string.h>
#include <math.h>

static void pack_stick(uint8_t *p, unsigned x, unsigned y)
{
    p[0] = (uint8_t)x; p[1] = (uint8_t)((x >> 8) | (y << 4));
    p[2] = (uint8_t)(y >> 4);
}
/* Preserve exact centre for the 8-bit DualSense axes. */
static unsigned axis(uint8_t x)
{
    return x < 128 ? (unsigned)x * 16 : 2048 + ((unsigned)x - 128) * 2047 / 127;
}
static int16_t le16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static void put16(uint8_t *p, int v)
{
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    p[0] = (uint8_t)v; p[1] = (uint8_t)((uint16_t)v >> 8);
}
void switch_pro_disconnect(struct switch_pro *s)
{
    memset(s->ds, 0, sizeof(s->ds));
    memset(s->ds, 128, 4); s->ds[7] = 8;
    memset(s->bias, 0, sizeof(s->bias));
    memset(s->motor, 0, sizeof(s->motor));
    for (unsigned i=0;i<6;i++) s->scale[i] = i < 3 ? (13371.0f / 936 / 16) : .5f;
    s->rumble_ms = 0;
}
void switch_pro_init(struct switch_pro *s, const uint8_t mac[6])
{
    memset(s, 0, sizeof(*s));
    memcpy(s->mac, mac, 6);
    switch_pro_disconnect(s);
    s->mode = 0x3f;
    s->gyro_range = 3;
}
void switch_pro_input(struct switch_pro *s, const uint8_t ds[63]) { memcpy(s->ds, ds, 63); }
void switch_pro_calibration(struct switch_pro *s, const uint8_t *data, unsigned len)
{
    if (len < 34) return;
    int speed = le16(data + 18) + le16(data + 20);
    for (unsigned i=0;i<3;i++) {
        int range = le16(data + 6 + i*4) - le16(data + 8 + i*4);
        float scale = range ? (float)speed / range * (13371.0f / 936) : 0;
        int bias = le16(data + i*2);
        if (scale > .4f && scale < 1.4f && bias >= -1024 && bias <= 1024) {
            s->bias[i] = (float)bias; s->scale[i] = scale;
        }
        int plus = le16(data + 22 + i*4), minus = le16(data + 24 + i*4);
        range = plus - minus;
        scale = range ? 8192.0f / range : 0;
        if (scale > .25f && scale < .75f) {
            s->bias[3+i] = (plus + minus) / 2.0f; s->scale[3+i] = scale;
        }
    }
}

void switch_pro_simple_report(const struct switch_pro *s, uint8_t out[64])
{
    const uint8_t *d = s->ds;
    memset(out, 0, 64); out[0] = 0x3f;
    /* Simple-mode button order consumed by SDL and generic HID drivers:
     * B,A,Y,X,L,R,ZL,ZR; minus,plus,L3,R3,home,capture. */
    out[1] = ((d[7] >> 5) & 1) | ((d[7] >> 5) & 2) |
             ((d[7] >> 2) & 4) | ((d[7] >> 4) & 8) |
             ((d[8] & 1) << 4) | ((d[8] & 2) << 4) |
             ((d[8] & 4) << 4) | ((d[8] & 8) << 4);
    out[2] = ((d[8] >> 4) & 15) | ((d[9] & 3) << 4);
    out[3] = (d[7] & 15) <= 7 ? d[7] & 15 : 8;
    for (unsigned i = 0; i < 4; ++i) {
        unsigned x = d[i];
        unsigned v = x <= 128 ? x * 256 : 32768 + (x-128)*32767/127;
        out[4+i*2] = (uint8_t)v; out[5+i*2] = (uint8_t)(v >> 8);
    }
}

void switch_pro_report(const struct switch_pro *s, uint32_t ms, uint8_t out[64])
{
    const uint8_t *d = s->ds;
    memset(out, 0, 64); out[0] = 0x30; out[1] = (uint8_t)ms;
    unsigned battery = d[52] & 15;
    if (battery > 10) battery = 10;
    out[2] = (uint8_t)((battery * 4 / 10 * 2) << 4) | 1;
    if ((d[52] >> 4) == 1) out[2] |= 0x10;
    /* Physical positions: circle=A, cross=B, triangle=X, square=Y. */
    out[3] = ((d[7] >> 4) & 1) | ((d[7] >> 6) & 2) |
             ((d[7] >> 3) & 4) | ((d[7] >> 3) & 8) |
             ((d[8] & 2) ? 0x40 : 0) | ((d[8] & 8) ? 0x80 : 0);
    out[4] = ((d[8] >> 4) & 3) | ((d[8] >> 5) & 4) |
             ((d[8] >> 3) & 8) | ((d[9] & 1) ? 0x10 : 0) |
             ((d[9] & 2) ? 0x20 : 0);
    unsigned hat = d[7] & 15;
    if (hat <= 7) {
        if (hat >= 3 && hat <= 5) out[5] |= 1;
        if (hat == 7 || hat <= 1) out[5] |= 2;
        if (hat >= 1 && hat <= 3) out[5] |= 4;
        if (hat >= 5) out[5] |= 8;
    }
    if (d[8] & 1) out[5] |= 0x40;
    if (d[8] & 4) out[5] |= 0x80;
    pack_stick(out + 6, axis(d[0]), d[1] ? 4096 - axis(d[1]) : 4095);
    pack_stick(out + 9, axis(d[2]), d[3] ? 4096 - axis(d[3]) : 4095);
    out[12] = 0x80;
    if (s->imu) {
        /* DS nominal: 8192 counts/g, 16 counts/degree/s. Pro: 4096/g,
         * 13371/936 counts/degree/s. Use factory calibration when available.
         * Axes rotate into Pro's coordinate frame.
         * Three repeated samples are explicit: no invented interpolation. */
        static const unsigned source[3] = {2, 0, 1};
        static const int sign[3] = {-1, -1, 1};
        static const float accel_factor[4] = {1, 2, 4, .5f};
        /* SDL maps Pro (x,y,z) to PS (-y,z,-x). Apply its inverse to
         * both sensors: Pro = (-PS.z,-PS.x,PS.y), after PS calibration. */
        for (unsigned i = 0; i < 3; ++i) {
            uint8_t *p = out + 13 + i * 12;
            for (unsigned axis = 0; axis < 3; ++axis) {
                unsigned j = source[axis];
                float acc = (le16(d+21+j*2)-s->bias[3+j])*s->scale[3+j];
                float gyro = (le16(d+15+j*2)-s->bias[j])*s->scale[j];
                put16(p+axis*2, (int)(sign[axis]*acc*accel_factor[s->accel_range]));
                put16(p+6+axis*2, (int)(sign[axis]*gyro*(1u << (3-s->gyro_range))));
            }
        }
    }
}

/* Synthetic factory calibration matches the report units above. User SPI is
 * erased so hosts choose factory data. SPI is read-only; NFC is unsupported. */
static uint8_t spi(uint32_t a)
{
    static const uint8_t sticks[18] = {
        0xff,0xf7,0x7f, 0x00,0x08,0x80, 0x00,0x08,0x80,
        0x00,0x08,0x80, 0x00,0x08,0x80, 0xff,0xf7,0x7f
    };
    static const uint8_t sensor[24] = {
        0,0,0,0,0,0, 0,0x40,0,0x40,0,0x40,
        0,0,0,0,0,0, 0x3b,0x34,0x3b,0x34,0x3b,0x34
    };
    if (a >= 0x6020 && a < 0x6038) return sensor[a - 0x6020];
    if (a >= 0x603d && a < 0x604f) return sticks[a - 0x603d];
    if (a >= 0x6050 && a < 0x605c) return 0x30;
    /* stick dead zone in the parameter blocks (12-bit packed). */
    if ((a >= 0x6086 && a < 0x6098) || (a >= 0x6098 && a < 0x60aa)) {
        unsigned o = (a - 0x6086) % 18;
        return o == 3 ? 0xae : o == 4 ? 0x40 : o == 5 ? 0xe1 : 0;
    }
    return 0xff;
}

static float amplitude(unsigned n)
{
    static const float small[16] = {0,.007843f,.011823f,.014061f,.016720f,
        .019885f,.023648f,.028123f,.033442f,.039771f,.047296f,.056246f,
        .066886f,.079542f,.094592f,.112491f};
    if (n < 16) return small[n];
    return n < 32 ? exp2f(n / 16.0f) / 17.0f : exp2f(n / 32.0f) / 8.7f;
}
static void rumble(struct switch_pro *s, const uint8_t *p, uint32_t ms)
{
    s->rumble_ms = ms;
    for (unsigned ch = 0; ch < 2; ++ch, p += 4) {
        struct switch_motor *m = &s->motor[ch];
        unsigned hf = p[0] | (p[1] & 1) << 8;
        unsigned la = p[3] & 0x7f;
        unsigned li = la >= 0x40 ? (la - 0x40) * 2 + (p[2] >> 7) : 0;
        m->hz[0] = 10 * exp2f((hf / 4.0f + 96) / 32);
        m->hz[1] = 10 * exp2f(((p[2] & 0x7f) + 64) / 32.0f);
        m->amp[0] = amplitude(p[1] >> 1);
        m->amp[1] = amplitude(li);
    }
}
unsigned switch_pro_output(struct switch_pro *s, const uint8_t *p, unsigned len,
                           uint32_t ms, uint8_t reply[64])
{
    if (!p || len < 2) return 0;
    memset(reply, 0, 64);
    if (p[0] == 0x80) {
        reply[0] = 0x81; reply[1] = p[1];
        if (p[1] == 1) {
            reply[2] = 0; reply[3] = 3;
            for (unsigned i = 0; i < 6; ++i) reply[4+i] = s->mac[5-i];
        } else if (p[1] == 4 || p[1] == 5) {
            /* Disable/enable the USB idle timeout, NOT input report mode.
             * Ryujinx's NpadHdRumble sends 80 05 after SDL selected 0x30.
             * Downgrading here silently removes its sensor stream while
             * ordinary buttons continue working through the simple report.
             * This USB bridge does not emulate the controller idle watchdog. */
            return 0;
        }
        else if (p[1] != 2 && p[1] != 3) return 0;
        if(p[1]==2) s->usb_stream=true;
        return 64;
    }
    if ((p[0] != 1 && p[0] != 0x10) || len < 10) return 0;
    rumble(s, p + 2, ms);
    if (p[0] == 0x10 || len < 11) return 0;
    switch_pro_report(s, ms, reply); memset(reply + 13, 0, 51);
    reply[0] = 0x21; reply[13] = 0x80; reply[14] = p[10];
    switch (p[10]) {
    case 0x00: reply[15] = 3; break;
    case 0x02:
        reply[13] = 0x82; reply[15] = 3; reply[16] = 0x48;
        reply[17] = 3; reply[18] = 2;
        memcpy(reply + 19, s->mac, 6); reply[25] = 1; reply[26] = 1; break;
    case 0x03:
        if (len < 12 || (p[11] != 0x30 && p[11] != 0x3f)) reply[13] = 0;
        else { s->mode = p[11];s->usb_stream=true; }
        break;
    case 0x04: reply[13] = 0x83; break;
    case 0x08: case 0x22: case 0x38: break;
    case 0x10: {
        if (len < 16 || p[15] > 29) { reply[13] = 0; break; }
        uint32_t a = (uint32_t)p[11] | (uint32_t)p[12]<<8 |
                     (uint32_t)p[13]<<16 | (uint32_t)p[14]<<24;
        reply[13] = 0x90; memcpy(reply + 15, p + 11, 5);
        for (unsigned i = 0; i < p[15]; ++i) reply[20+i] = spi(a+i);
        break;
    }
    case 0x30: if (len >= 12) s->player_leds = p[11]; else reply[13] = 0; break;
    case 0x31: reply[13] = 0xb0; reply[15] = s->player_leds; break;
    case 0x40:
        if (len < 12 || p[11] > 1) { reply[13] = 0; break; }
        if (!s->imu && p[11]) { s->gyro_range = 3; s->accel_range = 0; }
        s->imu = p[11] != 0;
        break;
    case 0x41:
        if (len < 15 || p[11] > 3 || p[12] > 3 || p[13] > 1 || p[14] > 1) {
            reply[13] = 0;
        } else { s->gyro_range = p[11]; s->accel_range = p[12]; }
        break;
    case 0x48:
        if (len >= 12) s->vibration = p[11] != 0; else reply[13] = 0;
        break;
    default: reply[13] = 0; break;
    }
    return 64;
}
void switch_pro_render(struct switch_pro *s, uint32_t ms, float gain,
                       int8_t *stereo, unsigned frames)
{
    const float tau = 6.28318530718f;
    bool active = s->vibration && (uint32_t)(ms - s->rumble_ms) < 250;
    for (unsigned i = 0; i < frames; ++i) {
        for (unsigned ch = 0; ch < 2; ++ch) {
            struct switch_motor *m = &s->motor[ch];
            float v = 0;
            for (unsigned band = 0; band < 2; ++band) {
                v += sinf(m->phase[band]) * m->amp[band];
                m->phase[band] += tau * m->hz[band] / 3000;
                if (m->phase[band] >= tau) m->phase[band] -= tau;
            }
            /* Shared headroom for two bands; clamp after user gain. */
            int n = active ? (int)(v * 63 * gain) : 0;
            if (n > 127) n = 127;
            if (n < -127) n = -127;
            *stereo++ = (int8_t)n;
        }
    }
}
