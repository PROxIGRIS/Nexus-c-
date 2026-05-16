#include "warden.h"
#include <tlhelp32.h>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <chrono>

// =====================================================
// LINKER DEFINITIONS
// We must allocate memory for the static variables here, 
// otherwise the compiler will throw a LNK2001 fatal error.
// =====================================================
HHOOK SentinelStrike::hKeyboardHook = NULL;
HHOOK SentinelStrike::hMouseHook = NULL;
std::atomic<bool> SentinelStrike::is_hooked{false};

// Constructor
SentinelStrike::SentinelStrike(int timeout) : timeout_sec(timeout), system_frozen(false) {}

// Destructor ensures hooks are violently removed if the agent crashes
SentinelStrike::~SentinelStrike() {
    DisengageFreeze();
    if (suppressor_thread.joinable()) {
        suppressor_thread.join();
    }
}

// -----------------------------------------------------
// THE KERNEL INTERCEPTS
// -----------------------------------------------------
LRESULT CALLBACK SentinelStrike::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // If nCode >= 0, Windows is handing us the keystroke. 
    // Returning 1 means "swallow the key, do not pass it to the active window."
    if (nCode >= 0 && is_hooked) {
        return 1; 
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK SentinelStrike::MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && is_hooked) {
        return 1; 
    }
    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

// -----------------------------------------------------
// THE MESSAGE PUMP (The Heart of the Trap)
// -----------------------------------------------------
void SentinelStrike::SuppressorLogic() {
    // 1. Inject the traps into the Windows subsystem
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, NULL, 0);
    is_hooked = true;

    auto start_time = std::chrono::steady_clock::now();
    MSG msg;

    // 2. The Pump: Windows requires hooked threads to actively process messages.
    // If we just sleep() here, the OS assumes we crashed and deletes our hooks.
    while (system_frozen) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        
        if (elapsed > timeout_sec) {
            std::cout << "[strike] Failsafe timeout reached. Forcing OS unhook.\n";
            system_frozen = false;
            break;
        }

        // Catch and dispatch OS messages to keep the thread alive in the kernel's eyes
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 3. The Cure: Rip the hooks out
    UnhookWindowsHookEx(hKeyboardHook);
    UnhookWindowsHookEx(hMouseHook);
    is_hooked = false;
}

// -----------------------------------------------------
// THE ENFORCEMENT COMMANDS
// -----------------------------------------------------
void SentinelStrike::EngageFreeze(int duration) {
    std::lock_guard<std::mutex> lock(_lock);
    if (!system_frozen) {
        system_frozen = true;
        
        if (suppressor_thread.joinable()) {
            suppressor_thread.join();
        }
        
        // Spin up the Message Pump in an isolated thread
        suppressor_thread = std::thread(&SentinelStrike::SuppressorLogic, this);
        std::cout << "[strike] Hardware Suppression Engaged.\n";

        // Optional timed release
        if (duration > 0) {
            std::thread([this, duration]() {
                std::this_thread::sleep_for(std::chrono::seconds(duration));
                DisengageFreeze();
            }).detach();
        }
    }
}

void SentinelStrike::DisengageFreeze() {
    std::lock_guard<std::mutex> lock(_lock);
    if (system_frozen) {
        system_frozen = false; // Flips the boolean, breaking the pump loop
        std::cout << "[strike] Hardware Suppression Disengaged.\n";
    }
}

// -----------------------------------------------------
// THE SCALPEL (Raw Memory Process Enumeration)
// -----------------------------------------------------
bool SentinelStrike::Scalpel(std::string target_name) {
    if (target_name.empty()) return false;
    
    // Normalize target name to lowercase for matching
    std::transform(target_name.begin(), target_name.end(), target_name.begin(),
        [](unsigned char c){ return std::tolower(c); });

    bool killed = false;
    
    // 1. Take a snapshot of all active processes in RAM
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);

        // 2. Iterate through the memory pointers
        if (Process32First(hSnap, &pe)) {
            do {
                std::string exe_name(pe.szExeFile);
                std::transform(exe_name.begin(), exe_name.end(), exe_name.begin(),
                    [](unsigned char c){ return std::tolower(c); });

                // 3. If the names match, acquire a termination handle and execute
                if (exe_name == target_name) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProcess != NULL) {
                        TerminateProcess(hProcess, 9); // 9 = SIGKILL equivalent
                        CloseHandle(hProcess);
                        std::cout << "[strike] Scalpel strike successful: " << target_name << "\n";
                        killed = true;
                    }
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap); // Prevent memory leak
    }
    
    if (!killed) {
        std::cerr << "[strike] Scalpel failure: Process not found or access denied.\n";
    }
    return killed;
}
