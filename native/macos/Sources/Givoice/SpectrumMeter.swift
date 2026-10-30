import AVFoundation

// Hann FFT with disjoint bands, matching Linux and Windows.
// Analysis only: recording and its file format remain owned by AVAudioRecorder.
final class SpectrumMeter {
    private let engine = AVAudioEngine()
    private let lock = NSLock()
    private var size = 256
    private var position = 0
    private var filled = 0
    private var pending = 0
    private var rate: Double = 0
    private var windowPower: Double = 0
    private var decay: Double = 0
    private var samples: [Double] = []
    private var window: [Double] = []
    private var real: [Double] = []
    private var imag: [Double] = []
    private var cosine: [Double] = []
    private var sine: [Double] = []
    private var power = [Double](repeating: 0, count: 5)
    private var installed = false

    func start() throws {
        let input = engine.inputNode
        let format = input.outputFormat(forBus: 0)
        let rate = format.sampleRate
        guard rate > 0, format.channelCount > 0 else {
            throw NSError(domain: "Givoice", code: 2,
                          userInfo: [NSLocalizedDescriptionKey: "마이크 주파수 분석을 시작하지 못했습니다."])
        }
        self.rate = rate
        while Double(size) < rate * 0.032 && size < 2048 { size *= 2 }
        samples = Array(repeating: 0, count: size)
        window = Array(repeating: 0, count: size)
        real = Array(repeating: 0, count: size)
        imag = Array(repeating: 0, count: size)
        cosine = Array(repeating: 0, count: size / 2)
        sine = Array(repeating: 0, count: size / 2)
        decay = exp(-(Double(size) / 2) / (rate * 0.03))
        for i in 0..<size {
            window[i] = 0.5 - 0.5 * cos(2 * Double.pi * Double(i) / Double(size))
            windowPower += window[i] * window[i]
        }
        for i in 0..<(size / 2) {
            let angle = -2 * Double.pi * Double(i) / Double(size)
            cosine[i] = cos(angle)
            sine[i] = sin(angle)
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
            samples[position] = sample
            position = (position + 1) % size
            filled = min(filled + 1, size)
            pending += 1
            if filled == size && pending >= size / 2 {
                pending = 0
                transform()
            }
        }
    }

    private func transform() {
        let n = size
        for i in 0..<n {
            real[i] = samples[(position + i) % n] * window[i]
            imag[i] = 0
        }
        var j = 0
        for i in 1..<n {
            var bit = n >> 1
            while (j & bit) != 0 { j ^= bit; bit >>= 1 }
            j ^= bit
            if i < j { real.swapAt(i, j) }
        }
        var width = 2
        while width <= n {
            for start in stride(from: 0, to: n, by: width) {
                for j in 0..<(width / 2) {
                    let a = start + j, b = a + width / 2, k = j * n / width
                    let re = cosine[k] * real[b] - sine[k] * imag[b]
                    let im = cosine[k] * imag[b] + sine[k] * real[b]
                    real[b] = real[a] - re
                    imag[b] = imag[a] - im
                    real[a] += re
                    imag[a] += im
                }
            }
            width *= 2
        }
        let edges: [Double] = [80, 250, 500, 1000, 2000, 8000]
        var next = [Double](repeating: 0, count: 5)
        for k in 1..<(n / 2) {
            let hz = Double(k) * rate / Double(n)
            for band in 0..<5 where hz >= edges[band] && hz < edges[band + 1] {
                next[band] += 2 * (real[k] * real[k] + imag[k] * imag[k]) / (Double(n) * windowPower)
                break
            }
        }
        for i in 0..<5 {
            power[i] = decay * power[i] + (1 - decay) * next[i]
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
