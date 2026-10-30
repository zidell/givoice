// Fixed bands and -60..0 dBFS scale match Linux and macOS.
pub struct Spectrum {
    b: [f64; 5], a1: [f64; 5], a2: [f64; 5],
    z1: [f64; 5], z2: [f64; 5], power: [f64; 5], decay: f64,
}
impl Spectrum {
    pub fn new(rate: f64) -> Self {
        let mut s = Self { b: [0.0; 5], a1: [0.0; 5], a2: [0.0; 5],
            z1: [0.0; 5], z2: [0.0; 5], power: [0.0; 5], decay: (-1.0 / (rate * 0.03)).exp() };
        let edges: [f64; 6] = [80.0, 250.0, 500.0, 1000.0, 2000.0, 8000.0];
        for i in 0..5 {
            let high = edges[i + 1].min(rate * 0.49);
            let low = edges[i].min(high * 0.5);
            let center = (low * high).sqrt();
            let q = center / (high - low);
            let w = 2.0 * std::f64::consts::PI * center / rate;
            let alpha = w.sin() / (2.0 * q);
            s.b[i] = alpha / (1.0 + alpha);
            s.a1[i] = -2.0 * w.cos() / (1.0 + alpha);
            s.a2[i] = (1.0 - alpha) / (1.0 + alpha);
        }
        s
    }
    pub fn sample(&mut self, sample: f64) {
        for i in 0..5 {
            let y = self.b[i] * sample + self.z1[i];
            self.z1[i] = -self.a1[i] * y + self.z2[i];
            self.z2[i] = -self.b[i] * sample - self.a2[i] * y;
            self.power[i] = self.decay * self.power[i] + (1.0 - self.decay) * y * y;
        }
    }
    pub fn levels(&self) -> [f32; 5] {
        self.power.map(|p| ((10.0 * p.max(1e-12).log10() + 60.0) / 60.0).clamp(0.0, 1.0) as f32)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn tone(hz: f64, amplitude: f64) -> [f32; 5] {
        let mut s = Spectrum::new(16000.0);
        for n in 0..16000 {
            s.sample(amplitude * (2.0 * std::f64::consts::PI * hz * n as f64 / 16000.0).sin());
        }
        s.levels()
    }
    #[test]
    fn silence_and_frequency_separation() {
        assert_eq!(tone(0.0, 0.0), [0.0; 5]);
        for (band, hz) in [141.0, 354.0, 707.0, 1414.0, 3960.0].iter().enumerate() {
            let levels = tone(*hz, 0.1);
            for i in 0..5 {
                if i != band { assert!(levels[band] > levels[i]); }
                assert!(levels[i] < 0.8);
            }
        }
        assert!(tone(707.0, 0.1)[2] > tone(707.0, 0.01)[2] + 0.3);
    }
}
