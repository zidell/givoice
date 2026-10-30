#include "../src/spectrum.h"
#include <assert.h>
#include <stdio.h>
static void tone(double hz, double amplitude, double levels[5]) {
    Spectrum s;
    spectrum_init(&s, 16000);
    for (int n = 0; n < 16000; n++)
        spectrum_sample(&s, amplitude * sin(2 * 3.141592653589793 * hz * n / 16000));
    spectrum_levels(&s, levels);
}
int main(void) {
    double levels[5], quiet[5], loud[5];
    tone(0, 0, levels);
    for (int i = 0; i < 5; i++) assert(levels[i] == 0);
    const double centers[] = {141, 354, 707, 1414, 3960};
    for (int band = 0; band < 5; band++) {
        tone(centers[band], 0.1, levels);
        for (int i = 0; i < 5; i++) {
            if (i != band) assert(levels[band] > levels[i]);
            assert(levels[i] < 0.8);
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
    puts("PASS: silence, five-band tone separation, 20dB dynamic range, release to silence");
}
