#pragma once
#include <math.h>
#include <string.h>

#define SPECTRUM_MAX 2048
// Hann-windowed FFT, disjoint frequency bands, fixed -60..0 dBFS scale.
// Roughly 32-46ms windows with 50% overlap; no automatic gain.
typedef struct {
    int size, position, filled, pending;
    double rate, window_power, decay;
    double samples[SPECTRUM_MAX], window[SPECTRUM_MAX];
    double real[SPECTRUM_MAX], imag[SPECTRUM_MAX];
    double cosine[SPECTRUM_MAX / 2], sine[SPECTRUM_MAX / 2], power[5];
} Spectrum;
static void spectrum_init(Spectrum *s, double rate) {
    memset(s, 0, sizeof(*s));
    s->rate = rate;
    s->size = 256;
    while (s->size < rate * 0.032 && s->size < SPECTRUM_MAX)
        s->size *= 2;
    s->decay = exp(-(s->size / 2.0) / (rate * 0.03));
    for (int i = 0; i < s->size; i++) {
        s->window[i] = 0.5 - 0.5 * cos(2 * 3.141592653589793 * i / s->size);
        s->window_power += s->window[i] * s->window[i];
    }
    for (int i = 0; i < s->size / 2; i++) {
        double angle = -2 * 3.141592653589793 * i / s->size;
        s->cosine[i] = cos(angle);
        s->sine[i] = sin(angle);
    }
}
static void spectrum_transform(Spectrum *s) {
    int n = s->size;
    for (int i = 0; i < n; i++) {
        s->real[i] = s->samples[(s->position + i) % n] * s->window[i];
        s->imag[i] = 0;
    }
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            double tmp = s->real[i]; s->real[i] = s->real[j]; s->real[j] = tmp;
        }
    }
    for (int width = 2; width <= n; width *= 2)
        for (int start = 0; start < n; start += width)
            for (int j = 0; j < width / 2; j++) {
                int a = start + j, b = a + width / 2, k = j * n / width;
                double re = s->cosine[k] * s->real[b] - s->sine[k] * s->imag[b];
                double im = s->cosine[k] * s->imag[b] + s->sine[k] * s->real[b];
                s->real[b] = s->real[a] - re; s->imag[b] = s->imag[a] - im;
                s->real[a] += re; s->imag[a] += im;
            }
    const double edges[] = {80, 250, 500, 1000, 2000, 8000};
    double power[5] = {0};
    for (int k = 1; k < n / 2; k++) {
        double hz = k * s->rate / n;
        for (int band = 0; band < 5; band++)
            if (hz >= edges[band] && hz < edges[band + 1]) {
                power[band] += 2 * (s->real[k] * s->real[k] + s->imag[k] * s->imag[k]) /
                               (n * s->window_power);
                break;
            }
    }
    for (int i = 0; i < 5; i++)
        s->power[i] = s->decay * s->power[i] + (1 - s->decay) * power[i];
}
static void spectrum_sample(Spectrum *s, double sample) {
    s->samples[s->position] = sample;
    s->position = (s->position + 1) % s->size;
    if (s->filled < s->size) s->filled++;
    s->pending++;
    if (s->filled == s->size && s->pending >= s->size / 2) {
        s->pending = 0;
        spectrum_transform(s);
    }
}
static void spectrum_levels(const Spectrum *s, double levels[5]) {
    for (int i = 0; i < 5; i++)
        levels[i] = fmax(0, fmin(1, (10 * log10(fmax(s->power[i], 1e-12)) + 60) / 60));
}
