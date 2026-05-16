#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>

// =====================================================
// THE NETWORK BRIDGE (Blueprint)
// =====================================================

class SupabaseBridge {
private:
    std::wstring host;
    std::wstring key;

    // Internal helper to handle raw WinHTTP POST/PATCH requests
    std::string SendHttpRequest(const std::wstring& method, const std::wstring& endpoint, const std::string& json_payload);

public:
    // Constructor
    SupabaseBridge(std::wstring db_host, std::wstring db_key);

    // Core Connectivity
    bool PingDatabase();
    
    // Telemetry & Enforcement
    std::wstring RegisterWorkstation();
    void SendHeartbeat(const std::wstring& workstation_id);
    void FireAlert(const std::wstring& workstation_id, const std::string& severity, const std::string& reason);
};
