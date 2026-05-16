#pragma once

#include <windows.h>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

// =====================================================
// THE WARDEN (Blueprint)
// =====================================================

class SentinelStrike {
private:
    std::atomic<bool> system_frozen;
    int timeout_sec;
    std::mutex _lock;
    std::thread suppressor_thread;

    // Static hooks required by the Win32 API
    static HHOOK hKeyboardHook;
    static HHOOK hMouseHook;
    static std::atomic<bool> is_hooked;

    // The raw memory callbacks that intercept kernel interrupts
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    // The internal execution loop for the message pump
    void SuppressorLogic();

public:
    // Constructor & Destructor
    SentinelStrike(int timeout = 300);
    ~SentinelStrike();

    // The God-Tier Enforcement Methods
    bool Scalpel(std::string target_name);
    void EngageFreeze(int duration = 0);
    void DisengageFreeze();
};
