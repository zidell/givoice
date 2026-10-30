#pragma once
#include <math.h>
#include <string.h>

// Five fixed frequency bands; RMS in dBFS mapped from -60..0 dB to 0..1.
// No automatic gain: louder input always produces a larger reading.
typedef struct {
    double b[5], a1[5], a2[5], z1[5], z2[5], power[5], decay;
} Spectrum;
static void spectrum_init(Spectrum *s, double rate) {
    memset(s, 0, sizeof(*s));
    const double edges[] = {80, 250, 500, 1000, 2000, 8000};
    s->decay = exp(-1.0 / (rate * 0.03));
    for (int i = 0; i < 5; i++) {
        double high = fmin(edges[i + 1], rate * 0.49);
        double low = fmin(edges[i], high * 0.5);
        double center = sqrt(low * high), q = center / (high - low);
        double w = 2 * 3.141592653589793 * center / rate;
        double alpha = sin(w) / (2 * q), a0 = 1 + alpha;
        s->b[i] = alpha / a0;
        s->a1[i] = -2 * cos(w) / a0;
        s->a2[i] = (1 - alpha) / a0;
    }
}
static void spectrum_sample(Spectrum *s, double sample) {
    for (int i = 0; i < 5; i++) {
        double y = s->b[i] * sample + s->z1[i];
        s->z1[i] = -s->a1[i] * y + s->z2[i];
        s->z2[i] = -s->b[i] * sample - s->a2[i] * y;
        s->power[i] = s->decay * s->power[i] + (1 - s->decay) * y * y;
    }
}
static void spectrum_levels(const Spectrum *s, double levels[5]) {
    for (int i = 0; i < 5; i++)
        levels[i] = fmax(0, fmin(1, (10 * log10(fmax(s->power[i], 1e-12)) + 60) / 60));
}
