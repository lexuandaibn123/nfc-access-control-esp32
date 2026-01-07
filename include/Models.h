#ifndef MODELS_H
#define MODELS_H

#include <Arduino.h>

// ============================================
// Cấu trúc dữ liệu thẻ
// ============================================
struct Credential {
    String format;      // "jwt"
    String alg;         // Thuật toán mã hóa ("EdDSA" hoặc "ES256")
    String raw;         // Chuỗi JWT đầy đủ
    String exp;         // Thời gian hết hạn
};

struct CardData {
    String card_id;     // ID thẻ trong hệ thống (backend cấp)
    String card_uid;    // UID phần cứng của thẻ (từ chip NFC)
    Credential credential;
    bool has_card_id;
    bool has_credential;
};

// ============================================
// Cấu hình thiết bị
// ============================================
struct OfflineWhitelistItem {
    String card_id;
    String user_id;
    String valid_until;
};

struct JwtVerificationConfig {
    String alg;             // Thuật toán ("EdDSA" hoặc "ES256")
    String public_key_pem;  // Khóa công khai (Public Key) dạng PEM
    String kid;             // Key ID (tùy chọn)
};

struct OfflineModeConfig {
    bool enabled;
    String cache_expire_at;
};

struct DeviceConfig {
    String device_id;
    int relay_open_ms;
    OfflineModeConfig offline_mode;
    JwtVerificationConfig jwt_verification;
    OfflineWhitelistItem whitelist[50];  // Lưu tối đa 50 người khi mất mạng
    int whitelist_count;
};

// ============================================
// Các mẫu dữ liệu gửi nhận API (Request/Response)
// ============================================
struct AccessCheckRequest {
    String device_id;
    String door_id;
    String card_id;
    String card_uid;
    String credential_raw;  // Chuỗi JWT (để trống nếu thẻ trắng)
    String timestamp;
};

struct UserInfo {
    String user_id;     // Mã người dùng
    String name;        // Tên
};

struct PolicyInfo {
    String access_level;
    String valid_until;
};

struct AccessCheckResponse {
    String result;          // "ALLOW" (cho vào) hoặc "DENY" (cấm)
    String reason;          // Lý do
    int relay_open_ms;
    UserInfo user;
    PolicyInfo policy;
    Credential credential;  // Credential mới từ backend (nếu có)
    bool has_user;
    bool has_policy;
    bool has_credential;
};

struct CardCreateRequest {
    String device_id;
    String card_uid;
};

struct CardCreateResponse {
    String card_id;
    String card_uid;
    String user_id;
    String status;
    bool enroll_mode;       // Chế độ đăng ký thẻ mới
};

// ============================================
// Nhật ký hoạt động (Log)
// ============================================
struct LogEntry {
    String ts;          // Thời gian (ISO 8601)
    String door_id;
    String card_id;
    String card_uid;
    String decision;    // Kết quả ("ALLOW" / "DENY")
    String reason;      // Lý do
};

// ============================================
// Trạng thái thiết bị (Heartbeat)
// ============================================
struct DeviceStatus {
    int uptime_sec;     // Thời gian đã chạy (giây)
    int rssi;           // Cường độ sóng Wifi
    String fw_version;  // Phiên bản phần mềm
    String last_access_ts;
};

// ============================================
// Lệnh điều khiển cửa (Từ xa)
// ============================================
struct DoorCommand {
    String action;          // Hành động: "unlock" (mở), "lock" (đóng)...
    String timestamp;       // Thời gian tạo lệnh
    String requestedBy;     // Người yêu cầu
};

struct DoorCommandPollResponse {
    bool hasCommand;        // Có lệnh mới không?
    DoorCommand command;
    int waitTime;           // Thời gian chờ server (ms)
};

#endif
