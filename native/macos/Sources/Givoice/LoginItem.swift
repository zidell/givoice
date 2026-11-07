import Foundation

/// 로그인 시 자동 실행. 설정 파일에 따로 두지 않고 사용자 LaunchAgent 파일 자체를 상태로 쓴다.
/// 데몬·KeepAlive·셸 없이 로그인할 때 앱을 한 번 띄우기만 한다.
enum LoginItem {
    static let url = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent("Library/LaunchAgents/com.videostew.givoice.login.plist")

    /// 개발용 LaunchAgent처럼 다른 감시자가 이 프로세스를 띄우면 그쪽이 로그인 실행을 맡는다.
    /// 여기서 또 등록하면 로그인할 때 앱이 두 개 뜬다.
    static var managedBySupervisor: Bool {
        ProcessInfo.processInfo.environment["GIVOICE_KEEPALIVE"] == "1"
    }

    static var isEnabled: Bool { FileManager.default.fileExists(atPath: url.path) }

    static func set(_ enabled: Bool) throws {
        guard enabled else {
            if isEnabled { try FileManager.default.removeItem(at: url) }
            return
        }
        guard let executable = Bundle.main.executablePath else {
            throw NSError(domain: "Givoice", code: 1,
                          userInfo: [NSLocalizedDescriptionKey: "실행 파일 경로를 찾을 수 없습니다"])
        }
        let agent: [String: Any] = [
            "Label": "com.videostew.givoice.login",
            "ProgramArguments": [executable],
            "RunAtLoad": true,
            "LimitLoadToSessionType": "Aqua",
            "AssociatedBundleIdentifiers": [Bundle.main.bundleIdentifier ?? "com.videostew.givoice"],
        ]
        let data = try PropertyListSerialization.data(fromPropertyList: agent, format: .xml, options: 0)
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(),
                                                withIntermediateDirectories: true)
        try data.write(to: url, options: .atomic)
    }
}
