#include "network.h"
#include <iostream>
#include <vector>

// Link the Windows HTTP library
#pragma comment(lib, "winhttp.lib")

SupabaseBridge::SupabaseBridge(std::wstring db_host, std::wstring db_key) 
    : host(db_host), key(db_key) {}

// -----------------------------------------------------
// CORE HTTP ENGINE (The Win32 Wrapper)
// -----------------------------------------------------
std::string SupabaseBridge::SendHttpRequest(const std::wstring& method, const std::wstring& endpoint, const std::string& json_payload) {
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    std::string response = "";

    hSession = WinHttpOpen(L"Nexus Sentinel Native/6.3.5", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession) {
        hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    }
    if (hConnect) {
        hRequest = WinHttpOpenRequest(hConnect, method.c_str(), endpoint.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    }

    if (hRequest) {
        // Construct Authorization and Content-Type Headers
        std::wstring headers = L"apikey: " + key + L"\r\n"
                               L"Authorization: Bearer " + key + L"\r\n"
                               L"Content-Type: application/json\r\n"
                               L"Prefer: return=representation\r\n"; // Supabase flag to return inserted row

        // Send the payload
        BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), -1, 
                                           (LPVOID)json_payload.c_str(), json_payload.length(), 
                                           json_payload.length(), 0);

        if (bResults) {
            bResults = WinHttpReceiveResponse(hRequest, NULL);
        }

        // Read the server response directly from the network buffer
        if (bResults) {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;
            do {
                dwSize = 0;
                WinHttpQueryDataAvailable(hRequest, &dwSize);
                if (dwSize == 0) break;

                std::vector<char> buffer(dwSize + 1, 0);
                if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
                    response.append(buffer.begin(), buffer.begin() + dwDownloaded);
                }
            } while (dwSize > 0);
        }
    }

    // MANDATORY MEMORY TEARDOWN
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    return response;
}

// -----------------------------------------------------
// SUPABASE API METHODS
// -----------------------------------------------------

bool SupabaseBridge::PingDatabase() {
    std::wcout << L"[network] Initiating WinHTTP Handshake with Mothership...\n";
    // Send a lightweight GET request to verify the JWT
    std::string res = SendHttpRequest(L"GET", L"/rest/v1/workstations?select=id&limit=1", "");
    return !res.empty();
}

std::wstring SupabaseBridge::RegisterWorkstation() {
    // In Python, you dynamically fetched the hostname. 
    // Here we hardcode for the C++ proof-of-concept.
    std::string hardware_uuid = "NATIVE-CPP-UUID-001";
    std::string name = "LAB-PC-01";

    // Manually formatting a JSON string (Brittle, but requires zero dependencies)
    std::string payload = "{\"hardware_uuid\":\"" + hardware_uuid + "\",\"name\":\"" + name + "\",\"status\":\"online\"}";

    // Fire POST request to Supabase REST API
    std::string response = SendHttpRequest(L"POST", L"/rest/v1/workstations", payload);
    
    std::cout << "[network] Registration payload deployed.\n";
    
    // In a full build, you would parse the JSON response here to extract the generated 'id'.
    // For this architecture, we return a mock wide-string ID.
    return L"uuid-1234-5678"; 
}

void SupabaseBridge::SendHeartbeat(const std::wstring& workstation_id) {
    // Note: C++ requires converting the wide string (std::wstring) to a narrow string (std::string) for the URL endpoint
    std::string wid(workstation_id.begin(), workstation_id.end());
    std::wstring endpoint = L"/rest/v1/workstations?id=eq." + workstation_id;
    
    std::string payload = "{\"status\":\"online\"}";
    
    // PATCH request updates the row
    SendHttpRequest(L"PATCH", endpoint, payload);
}

void SupabaseBridge::FireAlert(const std::wstring& workstation_id, const std::string& severity, const std::string& reason) {
    std::string wid(workstation_id.begin(), workstation_id.end());
    
    // Manual JSON construction replicating your Python _build_alert_payload logic
    std::string payload = "{\"workstation_id\":\"" + wid + "\",\"severity\":\"" + severity + "\",\"process_name\":\"" + reason + "\",\"is_backlogged\":false}";

    SendHttpRequest(L"POST", L"/rest/v1/alerts", payload);
    std::cout << "[!!!] ALERT [" << severity << "] Fired via WinHTTP Native Stream.\n";
}
