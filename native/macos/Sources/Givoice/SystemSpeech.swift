import Foundation
import Speech
import AVFoundation

/// Apple Speech on macOS 13+. Availability is not proof of downloaded assets.
final class SystemSpeech {
    private var generation = UUID()
    private var segment = UUID()
    private var task: SFSpeechRecognitionTask?
    private var activeRecognizer: SFSpeechRecognizer?
    private var timeout: DispatchWorkItem?
    private var chunks: [URL] = []

    static func recognizer(language: String) -> SFSpeechRecognizer? {
        let normalized = language.replacingOccurrences(of: "_", with: "-").lowercased()
        let locales = SFSpeechRecognizer.supportedLocales().sorted { $0.identifier < $1.identifier }
        let candidates = locales.filter {
            let code = $0.identifier.replacingOccurrences(of: "_", with: "-").lowercased()
            return code == normalized || (!normalized.contains("-") && code.split(separator: "-").first.map(String.init) == normalized)
        }
        let preferences = Locale.preferredLanguages + [Locale.current.identifier]
        let locale = preferences.compactMap { preferred in
            candidates.first { $0.identifier.replacingOccurrences(of: "_", with: "-").lowercased() == preferred.replacingOccurrences(of: "_", with: "-").lowercased() }
        }.first ?? candidates.first
        return locale.flatMap { SFSpeechRecognizer(locale: $0) }
    }

    static func problem(language: String, checkMicrophone: Bool = false) -> String? {
        guard let recognizer = recognizer(language: language) else {
            return "Apple 음성 인식이 '\(language)' 언어를 지원하지 않습니다. 다른 언어 또는 다른 엔진을 선택해 주세요."
        }
        switch SFSpeechRecognizer.authorizationStatus() {
        case .notDetermined: return "음성 인식 권한을 허용해야 합니다. ‘설정 안내’를 눌러 권한을 요청해 주세요."
        case .denied: return "음성 인식 권한이 꺼져 있습니다. 시스템 설정 → 개인정보 보호 및 보안 → 음성 인식에서 Givoice를 허용해 주세요."
        case .restricted: return "이 기기에서는 음성 인식 사용이 제한되어 있습니다. 기기 관리 정책을 확인하거나 다른 엔진을 선택해 주세요."
        case .authorized: break
        @unknown default: return "음성 인식 권한 상태를 확인할 수 없습니다. 시스템 설정에서 권한을 확인해 주세요."
        }
        if checkMicrophone {
            switch AVCaptureDevice.authorizationStatus(for: .audio) {
            case .notDetermined: return "마이크 권한을 허용해야 합니다. ‘설정 안내’를 눌러 권한을 요청해 주세요."
            case .denied: return "마이크 권한이 꺼져 있습니다. 시스템 설정 → 개인정보 보호 및 보안 → 마이크에서 Givoice를 허용해 주세요."
            case .restricted: return "이 기기에서는 마이크 사용이 제한되어 있습니다. 기기 관리 정책을 확인해 주세요."
            case .authorized: break
            @unknown default: return "시스템 설정에서 마이크 권한을 확인해 주세요."
            }
        }
        guard recognizer.isAvailable else {
            return "Apple 음성 인식을 현재 사용할 수 없습니다. 시스템 설정 → 키보드 → 받아쓰기의 활성화·언어 추가·다운로드 상태와 네트워크 연결을 확인한 뒤 다시 확인해 주세요. 이 상태만으로 언어 모델 미설치를 확정할 수는 없습니다."
        }
        return nil
    }

    static func status(language: String) -> String {
        if let problem = problem(language: language, checkMicrophone: true) { return problem }
        guard let recognizer = recognizer(language: language) else { return "지원하지 않는 언어" }
        return recognizer.supportsOnDeviceRecognition
            ? "사용 가능 · 기기 내 인식 지원 (필요시 Apple 서버 처리)"
            : "사용 가능 · 인터넷 연결 필요 · Apple 서버 처리"
    }

    static func requestPermissions(completion: @escaping () -> Void) {
        func microphone() {
            if AVCaptureDevice.authorizationStatus(for: .audio) == .notDetermined {
                AVCaptureDevice.requestAccess(for: .audio) { _ in DispatchQueue.main.async(execute: completion) }
            } else { DispatchQueue.main.async(execute: completion) }
        }
        if SFSpeechRecognizer.authorizationStatus() == .notDetermined {
            SFSpeechRecognizer.requestAuthorization { authorization in
                if authorization == .authorized { microphone() }
                else { DispatchQueue.main.async(execute: completion) }
            }
        } else { microphone() }
    }

    func cancel() {
        generation = UUID()
        segment = UUID()
        timeout?.cancel()
        timeout = nil
        task?.cancel()
        task = nil
        activeRecognizer = nil
        chunks.forEach { try? FileManager.default.removeItem(at: $0) }
        chunks = []
    }

    func transcribe(audioURL: URL, settings: Settings, completion: @escaping (Result<String, Error>) -> Void) {
        cancel()
        if let problem = Self.problem(language: settings.language) {
            completion(.failure(Self.error(problem)))
            return
        }
        let current = generation
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            // SFSpeechRecognizer requests have an approximately one-minute limit.
            let result = Result { try splitWAV(audioURL, seconds: 50) }
            DispatchQueue.main.async { [weak self] in
                guard let self, self.generation == current else {
                    if case .success(let urls) = result { urls.forEach { try? FileManager.default.removeItem(at: $0) } }
                    return
                }
                switch result {
                case .failure(let error): completion(.failure(error))
                case .success(let urls):
                    self.chunks = urls
                    self.next(settings: settings, completed: [], completion: completion)
                }
            }
        }
    }

    private func next(settings: Settings, completed: [String], completion: @escaping (Result<String, Error>) -> Void) {
        guard let url = chunks.first else {
            finish(.success(completed.filter { !$0.isEmpty }.joined(separator: " ")), completion: completion)
            return
        }
        guard let recognizer = Self.recognizer(language: settings.language), recognizer.isAvailable else {
            finish(.failure(Self.error(Self.status(language: settings.language))), completion: completion)
            return
        }
        let request = SFSpeechURLRecognitionRequest(url: url)
        activeRecognizer = recognizer
        request.shouldReportPartialResults = false
        request.taskHint = .dictation
        request.contextualStrings = Array(settings.keyterms.prefix(100))
        request.addsPunctuation = true
        let current = UUID()
        segment = current
        let expiry = DispatchWorkItem { [weak self] in
            guard let self, self.segment == current else { return }
            self.finish(.failure(Self.error("Apple 음성 인식 응답 시간이 초과되었습니다. 네트워크와 받아쓰기 언어 다운로드 상태를 확인해 주세요.")), completion: completion)
        }
        timeout = expiry
        DispatchQueue.main.asyncAfter(deadline: .now() + 90, execute: expiry)
        task = recognizer.recognitionTask(with: request) { [weak self] result, error in
            DispatchQueue.main.async { [weak self] in
                guard let self, self.segment == current else { return }
                if let result, result.isFinal {
                    self.segment = UUID()
                    self.timeout?.cancel()
                    self.timeout = nil
                    self.task = nil
                    self.chunks.removeFirst()
                    try? FileManager.default.removeItem(at: url)
                    self.next(settings: settings, completed: completed + [result.bestTranscription.formattedString], completion: completion)
                } else if let error {
                    self.finish(.failure(Self.error("Apple 음성 인식 실패: \(error.localizedDescription)\n시스템 설정 → 키보드 → 받아쓰기에서 언어 다운로드 상태를 확인하고, 음성 인식 권한과 네트워크도 확인해 주세요. 녹음 원본은 로그 폴더에 보존됩니다.")), completion: completion)
                }
            }
        }
    }

    private func finish(_ result: Result<String, Error>, completion: (Result<String, Error>) -> Void) {
        cancel()
        completion(result)
    }

    private static func error(_ message: String) -> Error {
        NSError(domain: "Givoice.SystemSpeech", code: 1, userInfo: [NSLocalizedDescriptionKey: message])
    }
}
