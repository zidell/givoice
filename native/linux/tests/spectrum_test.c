#include "../src/spectrum.h"
#include <assert.h>
#include <stdio.h>
static void tone_at_rate(double hz, double amplitude, double rate, double levels[5]) {
    Spectrum s;
    spectrum_init(&s, rate);
    for (int n = 0; n < rate; n++)
        spectrum_sample(&s, amplitude * sin(2 * 3.141592653589793 * hz * n / rate));
    spectrum_levels(&s, levels);
}
static void tone(double hz, double amplitude, double levels[5]) {
    tone_at_rate(hz, amplitude, 16000, levels);
}
int main(void) {
    double levels[5], quiet[5], loud[5];
    tone(0, 0, levels);
    for (int i = 0; i < 5; i++) assert(levels[i] == 0);
    const double centers[] = {141, 354, 707, 1414, 3960};
    const double rates[] = {16000, 44100, 48000};
    for (int r = 0; r < 3; r++)
        for (int band = 0; band < 5; band++) {
            tone_at_rate(centers[band], 0.1, rates[r], levels);
            for (int i = 0; i < 5; i++) {
                // A pure tone must not visibly raise unrelated frequency bars.
                if (i != band) assert(levels[i] < 0.1);
                else assert(fabs(levels[i] - (60 + 10 * log10(0.005)) / 60) < 0.01);
            }
        }
    tone(707, 0.01, quiet);
    tone(707, 0.1, loud);
    assert(loud[2] > quiet[2] + 0.3);
    assert(loud[2] < 1);
    Spectrum s;
    spectrum_init(&s, 16000);
    for (int n = 0; n < 16000; n++) spectrum_sample(&s, 0.2 * sin(n * 0.3));
    for (int n = 0; n < 16000; n++) spectrum_sample(&s, 0);
    spectrum_levels(&s, levels);
    for (int i = 0; i < 5; i++) assert(levels[i] == 0);
    puts("PASS: silence, FFT band isolation at 16/44.1/48kHz, calibrated RMS, 20dB dynamic range, release");
}
