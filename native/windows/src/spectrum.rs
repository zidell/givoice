// Hann FFT, disjoint bands and fixed -60..0 dBFS scale match Linux/macOS.
pub struct Spectrum {
    size: usize, position: usize, filled: usize, pending: usize,
    rate: f64, window_power: f64, decay: f64,
    samples: Vec<f64>, window: Vec<f64>, real: Vec<f64>, imag: Vec<f64>,
    cosine: Vec<f64>, sine: Vec<f64>, power: [f64; 5],
}
impl Spectrum {
    pub fn new(rate: f64) -> Self {
        let mut size = 256;
        while (size as f64) < rate * 0.032 && size < 2048 { size *= 2; }
        let mut s = Self {
            size, position: 0, filled: 0, pending: 0, rate, window_power: 0.0,
            decay: (-(size as f64 / 2.0) / (rate * 0.03)).exp(),
            samples: vec![0.0; size], window: vec![0.0; size],
            real: vec![0.0; size], imag: vec![0.0; size],
            cosine: vec![0.0; size / 2], sine: vec![0.0; size / 2], power: [0.0; 5],
        };
        for i in 0..size {
            s.window[i] = 0.5 - 0.5 * (2.0 * std::f64::consts::PI * i as f64 / size as f64).cos();
            s.window_power += s.window[i] * s.window[i];
        }
        for i in 0..size / 2 {
            let angle = -2.0 * std::f64::consts::PI * i as f64 / size as f64;
            s.cosine[i] = angle.cos(); s.sine[i] = angle.sin();
        }
        s
    }
    pub fn sample(&mut self, sample: f64) {
        self.samples[self.position] = sample;
        self.position = (self.position + 1) % self.size;
        self.filled = (self.filled + 1).min(self.size);
        self.pending += 1;
        if self.filled == self.size && self.pending >= self.size / 2 {
            self.pending = 0;
            self.transform();
        }
    }
    fn transform(&mut self) {
        let n = self.size;
        for i in 0..n {
            self.real[i] = self.samples[(self.position + i) % n] * self.window[i];
            self.imag[i] = 0.0;
        }
        let mut j = 0;
        for i in 1..n {
            let mut bit = n >> 1;
            while j & bit != 0 { j ^= bit; bit >>= 1; }
            j ^= bit;
            if i < j { self.real.swap(i, j); }
        }
        let mut width = 2;
        while width <= n {
            for start in (0..n).step_by(width) {
                for j in 0..width / 2 {
                    let a = start + j; let b = a + width / 2; let k = j * n / width;
                    let re = self.cosine[k] * self.real[b] - self.sine[k] * self.imag[b];
                    let im = self.cosine[k] * self.imag[b] + self.sine[k] * self.real[b];
                    self.real[b] = self.real[a] - re; self.imag[b] = self.imag[a] - im;
                    self.real[a] += re; self.imag[a] += im;
                }
            }
            width *= 2;
        }
        let edges = [80.0, 250.0, 500.0, 1000.0, 2000.0, 8000.0];
        let mut power = [0.0; 5];
        for k in 1..n / 2 {
            let hz = k as f64 * self.rate / n as f64;
            for band in 0..5 {
                if hz >= edges[band] && hz < edges[band + 1] {
                    power[band] += 2.0 * (self.real[k].powi(2) + self.imag[k].powi(2)) /
                        (n as f64 * self.window_power);
                    break;
                }
            }
        }
        for i in 0..5 {
            self.power[i] = self.decay * self.power[i] + (1.0 - self.decay) * power[i];
        }
    }
    pub fn levels(&self) -> [f32; 5] {
        self.power.map(|p| ((10.0 * p.max(1e-12).log10() + 60.0) / 60.0).clamp(0.0, 1.0) as f32)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn tone_at_rate(hz: f64, amplitude: f64, rate: f64) -> [f32; 5] {
        let mut s = Spectrum::new(rate);
        for n in 0..rate as usize {
            s.sample(amplitude * (2.0 * std::f64::consts::PI * hz * n as f64 / rate).sin());
        }
        s.levels()
    }
    fn tone(hz: f64, amplitude: f64) -> [f32; 5] { tone_at_rate(hz, amplitude, 16000.0) }
    #[test]
    fn silence_and_frequency_separation() {
        assert_eq!(tone(0.0, 0.0), [0.0; 5]);
        for rate in [16000.0, 44100.0, 48000.0] {
            for (band, hz) in [141.0, 354.0, 707.0, 1414.0, 3960.0].iter().enumerate() {
                let levels = tone_at_rate(*hz, 0.1, rate);
                for i in 0..5 {
                    if i != band { assert!(levels[i] < 0.1); }
                    else { assert!((levels[i] - (60.0 + 10.0 * 0.005_f32.log10()) / 60.0).abs() < 0.01); }
                }
            }
        }
        assert!(tone(707.0, 0.1)[2] > tone(707.0, 0.01)[2] + 0.3);
    }
}
