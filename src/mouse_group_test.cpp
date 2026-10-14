#include <windows.h>
#include <shlobj.h>
#include "primitive_execution_engine.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace std;

namespace {
int passed = 0, failed = 0;
int leftDownCount = 0, rightDownCount = 0, middleDownCount = 0;
LRESULT CALLBACK TestWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_LBUTTONDOWN) ++leftDownCount;
    if (msg == WM_RBUTTONDOWN) ++rightDownCount;
    if (msg == WM_MBUTTONDOWN) ++middleDownCount;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcA(hwnd, msg, wp, lp);
}
void check(const string& label, bool ok) {
    cout << "[" << label << "] " << (ok ? "PASS" : "FAIL") << '\n';
    (ok ? passed : failed)++;
}
PrimitiveOperation base(PrimitiveOperationType type, const string& name) {
    PrimitiveOperation p{}; p.type = type; p.name = name; p.resolved = true; return p;
}
PrimitiveOperation moveTo(int x, int y) {
    auto p = base(PrimitiveOperationType::MOUSE_MOVE, "mouse.move");
    p.parameters.x = to_string(x); p.parameters.y = to_string(y); return p;
}
PrimitiveOperation clickAt(int x, int y, const string& button = "left", const string& count = "1") {
    auto p = base(PrimitiveOperationType::MOUSE_CLICK, "mouse.click");
    p.parameters.x = to_string(x); p.parameters.y = to_string(y);
    p.parameters.button = button; p.parameters.clickCount = count; return p;
}
void pumpMessagesFor(int milliseconds) {
    const auto end = chrono::steady_clock::now() + chrono::milliseconds(milliseconds);
    MSG msg{};
    while (chrono::steady_clock::now() < end) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageA(&msg);
        }
        this_thread::sleep_for(chrono::milliseconds(5));
    }
}
}
int main() {
    cout << "=== GROUP 5: MOUSE TEST ===\n";
    PrimitiveExecutionEngine engine;

    WNDCLASSA wc{};
    wc.lpfnWndProc = TestWndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "NovaGroup5MouseTestWindow";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "Nova Group 5 Mouse Test",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 250, 200, 500, 300,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) {
        cerr << "Unable to create mouse test window. Windows error: " << GetLastError() << '\n';
        return 2;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    pumpMessagesFor(150);

    RECT rect{};
    GetWindowRect(hwnd, &rect);
    const int cx = rect.left + (rect.right - rect.left) / 2;
    const int cy = rect.top + (rect.bottom - rect.top) / 2;

    auto r = engine.execute(moveTo(cx, cy));
    POINT cursor{};
    const bool cursorVerified = GetCursorPos(&cursor) && cursor.x == cx && cursor.y == cy;
    check("01_MOUSE_MOVE_SUCCESS", r.status == ExecutionStatus::SUCCESS);
    check("02_CURSOR_POSITION_VERIFIED", cursorVerified);

    leftDownCount = 0;
    r = engine.execute(clickAt(cx, cy, "left"));
    pumpMessagesFor(150);
    check("03_LEFT_CLICK_SUCCESS", r.status == ExecutionStatus::SUCCESS);
    check("04_LEFT_CLICK_REACHED_TEST_WINDOW", leftDownCount == 1);

    rightDownCount = 0;
    r = engine.execute(clickAt(cx, cy, "right"));
    pumpMessagesFor(150);
    check("05_RIGHT_CLICK_SUCCESS", r.status == ExecutionStatus::SUCCESS);
    check("06_RIGHT_CLICK_REACHED_TEST_WINDOW", rightDownCount == 1);

    middleDownCount = 0;
    r = engine.execute(clickAt(cx, cy, "middle"));
    pumpMessagesFor(150);
    check("07_MIDDLE_CLICK_SUCCESS", r.status == ExecutionStatus::SUCCESS);
    check("08_MIDDLE_CLICK_REACHED_TEST_WINDOW", middleDownCount == 1);

    leftDownCount = 0;
    r = engine.execute(clickAt(cx, cy, "left", "2"));
    pumpMessagesFor(200);
    check("09_DOUBLE_CLICK_SUCCESS", r.status == ExecutionStatus::SUCCESS);
    check("10_DOUBLE_CLICK_DELIVERED_TWICE", leftDownCount == 2);

    r = engine.execute(moveTo(-99999, -99999));
    check("11_OUT_OF_BOUNDS_MOVE_REJECTED", r.status == ExecutionStatus::FAILED);

    auto badX = moveTo(cx, cy);
    badX.parameters.x = "12px";
    r = engine.execute(badX);
    check("12_NON_INTEGER_COORDINATE_REJECTED", r.status == ExecutionStatus::NOT_EXECUTED);

    r = engine.execute(clickAt(cx, cy, "laser"));
    check("13_UNKNOWN_BUTTON_REJECTED", r.status == ExecutionStatus::NOT_EXECUTED);

    r = engine.execute(clickAt(cx, cy, "left", "3"));
    check("14_INVALID_CLICK_COUNT_REJECTED", r.status == ExecutionStatus::NOT_EXECUTED);

    auto unresolved = moveTo(cx, cy);
    unresolved.resolved = false;
    r = engine.execute(unresolved);
    check("15_UNRESOLVED_MOUSE_MOVE_REJECTED", r.status == ExecutionStatus::NOT_EXECUTED);

    DestroyWindow(hwnd);
    pumpMessagesFor(30);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);

    cout << "\nGROUP 5 MOUSE TEST\nPassed: " << passed << "\nFailed: " << failed
         << "\n" << (failed == 0 ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return failed == 0 ? 0 : 1;
}
