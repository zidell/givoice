import AVFoundation

// Analysis only: recording and its file format remain owned by AVAudioRecorder.
final class SpectrumMeter {
    private let engine = AVAudioEngine()
    private let lock = NSLock()
    private var b = [Double](repeating: 0, count: 5)
    private var a1 = [Double](repeating: 0, count: 5)
    private var a2 = [Double](repeating: 0, count: 5)
    private var z1 = [Double](repeating: 0, count: 5)
    private var z2 = [Double](repeating: 0, count: 5)
    private var power = [Double](repeating: 0, count: 5)
    private var decay: Double = 0
    private var installed = false

    func start() throws {
        let input = engine.inputNode
        let format = input.outputFormat(forBus: 0)
        let rate = format.sampleRate
        guard rate > 0, format.channelCount > 0 else {
            throw NSError(domain: "Givoice", code: 2,
                          userInfo: [NSLocalizedDescriptionKey: "마이크 주파수 분석을 시작하지 못했습니다."])
        }
        let edges: [Double] = [80, 250, 500, 1000, 2000, 8000]
        decay = exp(-1 / (rate * 0.03))
        for i in 0..<5 {
            let high = min(edges[i + 1], rate * 0.49)
            let low = min(edges[i], high * 0.5)
            let center = sqrt(low * high), q = center / (high - low)
            let w = 2 * Double.pi * center / rate
            let alpha = sin(w) / (2 * q)
            b[i] = alpha / (1 + alpha)
            a1[i] = -2 * cos(w) / (1 + alpha)
            a2[i] = (1 - alpha) / (1 + alpha)
        }
        input.installTap(onBus: 0, bufferSize: 1024, format: format) { [weak self] buffer, _ in
            self?.analyze(buffer)
        }
        installed = true
        do { try engine.start() } catch { stop(); throw error }
    }

    private func analyze(_ buffer: AVAudioPCMBuffer) {
        guard let channels = buffer.floatChannelData else { return }
        lock.lock()
        defer { lock.unlock() }
        for frame in 0..<Int(buffer.frameLength) {
            var sample: Double = 0
            for channel in 0..<Int(buffer.format.channelCount) {
                sample += Double(channels[channel][frame])
            }
            sample /= Double(buffer.format.channelCount)
            for i in 0..<5 {
                let y = b[i] * sample + z1[i]
                z1[i] = -a1[i] * y + z2[i]
                z2[i] = -b[i] * sample - a2[i] * y
                power[i] = decay * power[i] + (1 - decay) * y * y
            }
        }
    }

    func levels() -> [CGFloat] {
        lock.lock()
        defer { lock.unlock() }
        // Fixed -60..0 dBFS scale, shared with Linux and Windows; no gain boost.
        return power.map { CGFloat(max(0, min(1, (10 * log10(max($0, 1e-12)) + 60) / 60))) }
    }

    func stop() {
        engine.stop()
        if installed {
            engine.inputNode.removeTap(onBus: 0)
            installed = false
        }
    }

    deinit { stop() }
}
