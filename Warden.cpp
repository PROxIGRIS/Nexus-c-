#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cctype>

// =====================================================
// THE WARDEN (C++ Native Win32 Enforcement Layer)
// =====================================================

class SentinelStrike {
private:
    std::atomic<bool> system_frozen{false};
    int timeout_sec;
    std::mutex _lock;
    std::thread suppressor_thread;

    // Win32 Hooks require static callbacks (they don't understand C++ class instances)
    static HHOOK hKeyboardHook;
    static HHOOK hMouseHook;
    static std::atomic<bool> is_hooked;

    // The Memory Callback: This intercepts the keystroke BEFORE Windows processes it
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode >= 0 && is_hooked) {
            return 1; // 1 = Swallow the input. The OS never sees it.
        }
        return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
    }

    // The Memory Callback: Intercepts all mouse movement and clicks
    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode >= 0 && is_hooked) {
            return 1; // Swallow the input.
        }
        return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
    }

    // The internal logic that actually sets the traps in the OS
    void SuppressorLogic() {
        // 1. Inject the hooks into the Windows subsystem
        hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
        hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, NULL, 0);
        is_hooked = true;

        auto start_time = std::chrono::steady_clock::now();
        MSG msg;

        // 2. The Message Pump: Required by Windows to keep hooks alive
        while (system_frozen) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            
            if (elapsed > timeout_sec) {
                std::cout << "[strike] Failsafe timeout reached. Forcing OS unhook.\n";
                system_frozen = false;
                break;
            }

            // Process Windows messages so the thread doesn't lock up the CPU
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // 3. The Cure: Manually rip the hooks out of the kernel
        UnhookWindowsHookEx(hKeyboardHook);
        UnhookWindowsHookEx(hMouseHook);
        is_hooked = false;
    }

public:
    SentinelStrike(int timeout = 300) : timeout_sec(timeout) {}

    ~SentinelStrike() {
        DisengageFreeze();
        if (suppressor_thread.joinable()) {
            suppressor_thread.join();
        }
    }

    // Surgically terminate a specific process by scraping Windows memory
    bool Scalpel(std::string target_name) {
        if (target_name.empty()) return false;
        
        // Convert target to lowercase
        std::transform(target_name.begin(), target_name.end(), target_name.begin(),
            [](unsigned char c){ return std::tolower(c); });

        bool killed = false;
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe;
            pe.dwSize = sizeof(PROCESSENTRY32);

            if (Process32First(hSnap, &pe)) {
                do {
                    std::string exe_name(pe.szExeFile);
                    std::transform(exe_name.begin(), exe_name.end(), exe_name.begin(),
                        [](unsigned char c){ return std::tolower(c); });

                    if (exe_name == target_name) {
                        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                        if (hProcess != NULL) {
                            TerminateProcess(hProcess, 9);
                            CloseHandle(hProcess);
                            std::cout << "[strike] Scalpel strike successful: " << target_name << "\n";
                            killed = true;
                        }
                    }
                } while (Process32Next(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }
        if (!killed) {
            std::cerr << "[strike] Scalpel failure: Process not found or access denied.\n";
        }
        return killed;
    }

    // Locks hardware inputs
    void EngageFreeze(int duration = 0) {
        std::lock_guard<std::mutex> lock(_lock);
        if (!system_frozen) {
            system_frozen = true;
            
            if (suppressor_thread.joinable()) {
                suppressor_thread.join();
            }
            
            suppressor_thread = std::thread(&SentinelStrike::SuppressorLogic, this);
            std::cout << "[strike] Hardware Suppression Engaged.\n";

            if (duration > 0) {
                std::thread([this, duration]() {
                    std::this_thread::sleep_for(std::chrono::seconds(duration));
                    DisengageFreeze();
                }).detach();
            }
        }
    }

    // Restores user control
    void DisengageFreeze() {
        std::lock_guard<std::mutex> lock(_lock);
        if (system_frozen) {
            system_frozen = false; // Breaks the while loop in SuppressorLogic
            std::cout << "[strike] Hardware Suppression Disengaged.\n";
        }
    }
};

// Initialize static members
HHOOK SentinelStrike::hKeyboardHook = NULL;
HHOOK SentinelStrike::hMouseHook = NULL;
std::atomic<bool> SentinelStrike::is_hooked{false};
