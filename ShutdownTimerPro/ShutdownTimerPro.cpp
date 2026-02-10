#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <shellapi.h>
#include "ShutdownTimerPro.h"
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// 常量定义
#define ID_TIMER_UPDATE 1001
#define ID_TIMER_COUNTDOWN 1002

// 控件ID
#define IDC_MINUTES_EDIT 2001
#define IDC_HOUR_EDIT 2002
#define IDC_MINUTE_EDIT 2003
#define IDC_TIME_EDIT 2004
#define IDC_COUNTDOWN_EDIT 2005
#define IDC_CURRENT_TIME 2006
#define IDC_STATUS_TEXT 2007
#define IDC_PROGRESS_BAR 2008

#define IDC_SHUTDOWN_MINUTES_BTN 3001
#define IDC_SHUTDOWN_TIME_BTN 3002
#define IDC_SHUTDOWN_NOW_BTN 3003
#define IDC_RESTART_BTN 3004
#define IDC_HIBERNATE_BTN 3005
#define IDC_CANCEL_BTN 3006
#define IDC_COUNTDOWN_BTN 3007
#define IDC_HELP_BTN 3008

// 菜单ID
#define IDM_FILE_EXIT 4001
#define IDM_VIEW_ALWAYSONTOP 4002
#define IDM_HELP_ABOUT 4003

// 全局变量
static HWND g_hMainWnd;
static HWND g_hStatusBar;
static HWND g_hCurrentTimeLabel;
static HWND g_hStatusLabel;
static HWND g_hProgressBar;
static bool g_bAdminPrivileges = false;
static bool g_bAlwaysOnTop = false;
static bool g_bCountdownActive = false;
static int g_iCountdownSeconds = 0;
static time_t g_shutdownTime = 0;
static HICON g_hAppIcon;

// 函数声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void CreateControls(HWND hwnd);
void UpdateCurrentTime();
void UpdateStatus(const std::wstring& status);
void ShutdownComputer(int seconds);
void CancelShutdownTask();
void RestartComputer();
void HibernateComputer();
void ShowHelpDialog(HWND hwnd);
void ShowAboutDialog(HWND hwnd);
void CheckAdminPrivileges();
bool IsAdmin();
std::wstring FormatTime(int seconds);
std::wstring GetCurrentTimeString();
void StartCountdown(int seconds);
void StopCountdown();
void UpdateProgressBar(int totalSeconds, int remainingSeconds);
void ToggleAlwaysOnTop(HWND hwnd);
void SetButtonEnabled(HWND hwnd, int id, bool enabled);

// 程序入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR szCmdLine, int iCmdShow) {
    // 初始化公共控件
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    // 检查管理员权限
    CheckAdminPrivileges();

    // 加载应用程序图标

    g_hAppIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SHUTDOWNTIMERPRO));
    if (!g_hAppIcon) {
        g_hAppIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    // 注册窗口类
    WNDCLASSEX wc;
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = g_hAppIcon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"ShutdownTimerGUI";
    wc.hIconSm = g_hAppIcon;

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"窗口注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // 创建主窗口
    g_hMainWnd = CreateWindowEx(
        0,
        L"ShutdownTimerGUI",
        L"Shutdown Timer Pro - 定时关机管理",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT,
        550, 450,
        NULL, NULL, hInstance, NULL);

    if (!g_hMainWnd) {
        MessageBox(NULL, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // 显示窗口
    ShowWindow(g_hMainWnd, iCmdShow);
    UpdateWindow(g_hMainWnd);

    // 启动时间更新定时器
    SetTimer(g_hMainWnd, ID_TIMER_UPDATE, 1000, NULL);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateControls(hwnd);
        UpdateCurrentTime();
        UpdateStatus(L"就绪 - 请选择操作");
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        switch (id) {
        case IDC_SHUTDOWN_MINUTES_BTN: {
            // 按分钟关机
            HWND hEdit = GetDlgItem(hwnd, IDC_MINUTES_EDIT);
            wchar_t buffer[32];
            GetWindowText(hEdit, buffer, 32);

            int minutes = _wtoi(buffer);
            if (minutes <= 0) {
                MessageBox(hwnd, L"请输入有效的分钟数（大于0）", L"输入错误", MB_ICONWARNING);
                return 0;
            }

            int seconds = minutes * 60;
            ShutdownComputer(seconds);
            UpdateStatus(L"已设置定时关机：" + std::to_wstring(minutes) + L"分钟后");
            break;
        }

        case IDC_SHUTDOWN_TIME_BTN: {
            // 按时间关机
            HWND hHourEdit = GetDlgItem(hwnd, IDC_HOUR_EDIT);
            HWND hMinuteEdit = GetDlgItem(hwnd, IDC_MINUTE_EDIT);

            wchar_t hourBuffer[32];
            wchar_t minuteBuffer[32];
            GetWindowText(hHourEdit, hourBuffer, 32);
            GetWindowText(hMinuteEdit, minuteBuffer, 32);

            int hour = _wtoi(hourBuffer);
            int minute = _wtoi(minuteBuffer);

            if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
                MessageBox(hwnd, L"请输入有效的时间（小时：0-23，分钟：0-59）", L"输入错误", MB_ICONWARNING);
                return 0;
            }

            // 计算目标时间
            time_t now = time(0);
            tm currentTime;
            localtime_s(&currentTime, &now);

            tm targetTime = currentTime;
            targetTime.tm_hour = hour;
            targetTime.tm_min = minute;
            targetTime.tm_sec = 0;

            time_t targetTimestamp = mktime(&targetTime);

            // 如果目标时间已经过去，则设置为明天
            if (targetTimestamp <= now) {
                targetTimestamp += 24 * 60 * 60;
            }

            int seconds = static_cast<int>(difftime(targetTimestamp, now));

            if (seconds <= 0) {
                MessageBox(hwnd, L"时间计算错误", L"错误", MB_ICONERROR);
                return 0;
            }

            g_shutdownTime = targetTimestamp;
            ShutdownComputer(seconds);

            wchar_t timeStr[64];
            swprintf_s(timeStr, L"%02d:%02d", hour, minute);
            UpdateStatus(L"已设置定时关机，目标时间：" + std::wstring(timeStr));
            break;
        }

        case IDC_SHUTDOWN_NOW_BTN: {
            // 立即关机
            int result = MessageBox(hwnd,
                L"系统将在30秒后立即关机！\n\n请保存所有工作。\n\n确定要继续吗？",
                L"立即关机确认",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

            if (result == IDYES) {
                ShutdownComputer(30);
                UpdateStatus(L"立即关机已设置，30秒后关机");
            }
            break;
        }

        case IDC_RESTART_BTN: {
            // 重启计算机
            int result = MessageBox(hwnd,
                L"系统将在30秒后重启！\n\n请保存所有工作。\n\n确定要继续吗？",
                L"重启确认",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

            if (result == IDYES) {
                RestartComputer();
                UpdateStatus(L"重启已设置，30秒后重启");
            }
            break;
        }

        case IDC_HIBERNATE_BTN: {
            // 休眠计算机
            int result = MessageBox(hwnd,
                L"系统将进入休眠状态。\n\n确定要继续吗？",
                L"休眠确认",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

            if (result == IDYES) {
                HibernateComputer();
                UpdateStatus(L"系统正在进入休眠状态...");
            }
            break;
        }

        case IDC_CANCEL_BTN: {
            // 取消关机
            CancelShutdownTask();
            UpdateStatus(L"已取消所有关机任务");
            break;
        }

        case IDC_COUNTDOWN_BTN: {
            // 查看倒计时
            HWND hEdit = GetDlgItem(hwnd, IDC_COUNTDOWN_EDIT);
            wchar_t buffer[32];
            GetWindowText(hEdit, buffer, 32);

            int seconds = _wtoi(buffer);
            if (seconds <= 0) {
                MessageBox(hwnd, L"请输入有效的秒数（大于0）", L"输入错误", MB_ICONWARNING);
                return 0;
            }

            StartCountdown(seconds);
            UpdateStatus(L"倒计时已开始：" + std::to_wstring(seconds) + L"秒");
            break;
        }

        case IDC_HELP_BTN: {
            // 帮助
            ShowHelpDialog(hwnd);
            break;
        }

        case IDM_FILE_EXIT:
            DestroyWindow(hwnd);
            break;

        case IDM_VIEW_ALWAYSONTOP:
            ToggleAlwaysOnTop(hwnd);
            break;

        case IDM_HELP_ABOUT:
            ShowAboutDialog(hwnd);
            break;
        }
        return 0;
    }

    case WM_TIMER: {
        if (wParam == ID_TIMER_UPDATE) {
            // 更新时间显示
            UpdateCurrentTime();

            // 如果有关机任务，更新倒计时显示
            if (g_shutdownTime > 0) {
                time_t now = time(0);
                int remainingSeconds = static_cast<int>(difftime(g_shutdownTime, now));

                if (remainingSeconds > 0) {
                    UpdateStatus(L"关机倒计时: " + FormatTime(remainingSeconds));
                    UpdateProgressBar(g_iCountdownSeconds, remainingSeconds);
                }
                else {
                    g_shutdownTime = 0;
                    UpdateStatus(L"关机时间已到，系统即将关机...");
                }
            }
        }
        else if (wParam == ID_TIMER_COUNTDOWN) {
            // 倒计时更新
            if (g_bCountdownActive && g_iCountdownSeconds > 0) {
                g_iCountdownSeconds--;

                if (g_iCountdownSeconds <= 0) {
                    KillTimer(hwnd, ID_TIMER_COUNTDOWN);
                    g_bCountdownActive = false;
                    UpdateStatus(L"倒计时结束");
                    UpdateProgressBar(0, 0);

                    HWND hProgressBar = GetDlgItem(hwnd, IDC_PROGRESS_BAR);
                    SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

                    MessageBox(hwnd, L"倒计时结束！", L"倒计时", MB_ICONINFORMATION);
                }
                else {
                    UpdateStatus(L"倒计时: " + FormatTime(g_iCountdownSeconds));
                    UpdateProgressBar(g_iCountdownSeconds, g_iCountdownSeconds);
                }
            }
        }
        return 0;
    }

    case WM_CLOSE:
        if (g_bCountdownActive) {
            KillTimer(hwnd, ID_TIMER_COUNTDOWN);
        }
        KillTimer(hwnd, ID_TIMER_UPDATE);
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

// 创建控件
void CreateControls(HWND hwnd) {
    // 创建菜单
    HMENU hMenuBar = CreateMenu();
    HMENU hFileMenu = CreateMenu();
    HMENU hViewMenu = CreateMenu();
    HMENU hHelpMenu = CreateMenu();

    AppendMenu(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"退出(&X)");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"文件(&F)");

    AppendMenu(hViewMenu, MF_STRING, IDM_VIEW_ALWAYSONTOP, L"置顶显示(&T)");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hViewMenu, L"视图(&V)");

    AppendMenu(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, L"关于(&A)...");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hHelpMenu, L"帮助(&H)");

    SetMenu(hwnd, hMenuBar);

    // 设置字体
    HFONT hFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"微软雅黑");

    // 当前时间显示
    g_hCurrentTimeLabel = CreateWindowEx(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 10, 480, 30,
        hwnd, (HMENU)IDC_CURRENT_TIME,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_hCurrentTimeLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 状态显示
    g_hStatusLabel = CreateWindowEx(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        25, 50, 480, 30,
        hwnd, (HMENU)IDC_STATUS_TEXT,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_hStatusLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 进度条
    g_hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, L"",
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        27, 85, 480, 20,
        hwnd, (HMENU)IDC_PROGRESS_BAR,
        GetModuleHandle(NULL), NULL);
    SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(g_hProgressBar, PBM_SETSTEP, 1, 0);

    int yPos = 120;
    int controlHeight = 28;

    // 按分钟关机
    CreateWindowEx(0, L"STATIC", L"按分钟关机:",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        35, yPos, 100, controlHeight,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    HWND hMinutesEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"30",
        WS_CHILD | WS_VISIBLE | ES_NUMBER,
        145, yPos, 60, controlHeight,
        hwnd, (HMENU)IDC_MINUTES_EDIT,
        GetModuleHandle(NULL), NULL);
    SendMessage(hMinutesEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    CreateWindowEx(0, L"STATIC", L"分钟",
        WS_CHILD | WS_VISIBLE,
        185 + 25, yPos, 40, controlHeight,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    HWND hMinutesBtn = CreateWindowEx(0, L"BUTTON", L"设置",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        230 + 25, yPos, 100, controlHeight,
        hwnd, (HMENU)IDC_SHUTDOWN_MINUTES_BTN,
        GetModuleHandle(NULL), NULL);
    SendMessage(hMinutesBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    yPos += 40;

    // 按时间关机
    CreateWindowEx(0, L"STATIC", L"按时间关机:",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        10 + 25, yPos, 100, controlHeight,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    HWND hHourEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"22",
        WS_CHILD | WS_VISIBLE | ES_NUMBER,
        120 + 25, yPos, 40, controlHeight,
        hwnd, (HMENU)IDC_HOUR_EDIT,
        GetModuleHandle(NULL), NULL);
    SendMessage(hHourEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    CreateWindowEx(0, L"STATIC", L":",
        WS_CHILD | WS_VISIBLE,
        165 + 25, yPos, 10, controlHeight,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    HWND hMinuteEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"00",
        WS_CHILD | WS_VISIBLE | ES_NUMBER,
        180 + 25, yPos, 40, controlHeight,
        hwnd, (HMENU)IDC_MINUTE_EDIT,
        GetModuleHandle(NULL), NULL);
    SendMessage(hMinuteEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    CreateWindowEx(0, L"STATIC", L"(24小时制)",
        WS_CHILD | WS_VISIBLE,
        225 + 25, yPos, 70, controlHeight,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    HWND hTimeBtn = CreateWindowEx(0, L"BUTTON", L"设置",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        300 + 25, yPos, 100, controlHeight,
        hwnd, (HMENU)IDC_SHUTDOWN_TIME_BTN,
        GetModuleHandle(NULL), NULL);
    SendMessage(hTimeBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    yPos += 40;

    // 倒计时功能
    CreateWindowEx(0, L"STATIC", L"倒计时测试:",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        10 + 25, yPos, 100, controlHeight,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    HWND hCountdownEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"60",
        WS_CHILD | WS_VISIBLE | ES_NUMBER,
        120 + 25, yPos, 60, controlHeight,
        hwnd, (HMENU)IDC_COUNTDOWN_EDIT,
        GetModuleHandle(NULL), NULL);
    SendMessage(hCountdownEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    CreateWindowEx(0, L"STATIC", L"秒",
        WS_CHILD | WS_VISIBLE,
        185 + 25, yPos, 30, controlHeight,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    HWND hCountdownBtn = CreateWindowEx(0, L"BUTTON", L"开始倒计时",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        220 + 25, yPos, 120, controlHeight,
        hwnd, (HMENU)IDC_COUNTDOWN_BTN,
        GetModuleHandle(NULL), NULL);
    SendMessage(hCountdownBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    yPos += 50;

    // 操作按钮行1
    int buttonWidth = 110;
    int buttonSpacing = 10;
    int xPos = 6 + 25;

    HWND hShutdownNowBtn = CreateWindowEx(0, L"BUTTON", L"💻 立即关机",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        xPos, yPos, buttonWidth, controlHeight + 5,
        hwnd, (HMENU)IDC_SHUTDOWN_NOW_BTN,
        GetModuleHandle(NULL), NULL);
    SendMessage(hShutdownNowBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    xPos += buttonWidth + buttonSpacing;

    HWND hRestartBtn = CreateWindowEx(0, L"BUTTON", L"🔄 重启",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        xPos, yPos, buttonWidth, controlHeight + 5,
        hwnd, (HMENU)IDC_RESTART_BTN,
        GetModuleHandle(NULL), NULL);
    SendMessage(hRestartBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    xPos += buttonWidth + buttonSpacing;

    HWND hHibernateBtn = CreateWindowEx(0, L"BUTTON", L"💤 休眠",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        xPos, yPos, buttonWidth, controlHeight + 5,
        hwnd, (HMENU)IDC_HIBERNATE_BTN,
        GetModuleHandle(NULL), NULL);
    SendMessage(hHibernateBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    xPos += buttonWidth + buttonSpacing;

    HWND hCancelBtn = CreateWindowEx(0, L"BUTTON", L"❌ 取消关机",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        xPos, yPos, buttonWidth, controlHeight + 5,
        hwnd, (HMENU)IDC_CANCEL_BTN,
        GetModuleHandle(NULL), NULL);
    SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    yPos += 50;

    // 帮助按钮
    HWND hHelpBtn = CreateWindowEx(0, L"BUTTON", L"❓ 使用帮助",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        2 + 25, yPos, 480, controlHeight + 5,
        hwnd, (HMENU)IDC_HELP_BTN,
        GetModuleHandle(NULL), NULL);
    SendMessage(hHelpBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 如果没有管理员权限，禁用关机相关按钮
    if (!g_bAdminPrivileges) {
        SetButtonEnabled(hwnd, IDC_SHUTDOWN_MINUTES_BTN, false);
        SetButtonEnabled(hwnd, IDC_SHUTDOWN_TIME_BTN, false);
        SetButtonEnabled(hwnd, IDC_SHUTDOWN_NOW_BTN, false);
        SetButtonEnabled(hwnd, IDC_RESTART_BTN, false);
        SetButtonEnabled(hwnd, IDC_HIBERNATE_BTN, false);
        SetButtonEnabled(hwnd, IDC_CANCEL_BTN, false);

        UpdateStatus(L"警告：程序可能没有管理员权限！部分功能受限。");
    }
}

// 更新当前时间
void UpdateCurrentTime() {
    std::wstring timeStr = GetCurrentTimeString();
    SetWindowText(g_hCurrentTimeLabel, timeStr.c_str());
}

// 获取当前时间字符串
std::wstring GetCurrentTimeString() {
    time_t now = time(0);
    tm localTime;
    localtime_s(&localTime, &now);

    wchar_t buffer[64];
    int weekday = localTime.tm_wday;
    if (weekday == 0) weekday = 7; // 星期天转为7

    swprintf_s(buffer, L"📅 当前时间: %04d-%02d-%02d 星期%d %02d:%02d:%02d",
        localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
        weekday,
        localTime.tm_hour, localTime.tm_min, localTime.tm_sec);

    return std::wstring(buffer);
}

// 更新状态文本
void UpdateStatus(const std::wstring& status) {
    SetWindowText(g_hStatusLabel, status.c_str());
}

// 关机函数
void ShutdownComputer(int seconds) {
    wchar_t command[256];
    swprintf_s(command, L"shutdown -s -t %d", seconds);

    int result = _wsystem(command);

    if (result == 0) {
        g_iCountdownSeconds = seconds;
        UpdateStatus(L"✅ 定时关机已设置: " + FormatTime(seconds));
    }
    else {
        UpdateStatus(L"❌ 设置关机失败！请以管理员身份运行。");
    }
}

// 取消关机
void CancelShutdownTask() {
    int result = _wsystem(L"shutdown -a");

    if (result == 0) {
        g_shutdownTime = 0;
        UpdateStatus(L"✅ 定时关机已取消！");
        UpdateProgressBar(0, 0);
    }
    else {
        UpdateStatus(L"⚠️ 没有发现活动的关机任务。");
    }
}

// 重启计算机
void RestartComputer() {
    int result = _wsystem(L"shutdown -r -t 30");

    if (result != 0) {
        UpdateStatus(L"❌ 重启失败！请以管理员身份运行。");
    }
}

// 休眠计算机
void HibernateComputer() {
    int result = _wsystem(L"shutdown -h");

    if (result != 0) {
        UpdateStatus(L"❌ 休眠失败！请以管理员身份运行。");
    }
}

// 显示帮助对话框
void ShowHelpDialog(HWND hwnd) {
    std::wstring helpText = L"Shutdown Timer Pro 使用帮助\n\n"
        L"📋 功能说明：\n"
        L"• 按分钟设置：输入分钟后自动关机\n"
        L"• 按时间设置：指定具体时间自动关机\n"
        L"• 立即关机：30秒后立即关机\n"
        L"• 重启计算机：30秒后重启\n"
        L"• 休眠计算机：立即进入休眠\n"
        L"• 取消关机：取消所有定时任务\n"
        L"• 倒计时测试：测试倒计时功能\n\n"
        L"🔑 管理员权限：\n"
        L"• 程序需要管理员权限才能执行关机操作\n"
        L"• 请右键程序，选择'以管理员身份运行'\n\n"
        L"🚫 取消关机的方法：\n"
        L"1. 在程序中点击'取消关机'按钮\n"
        L"2. 按Win+R，输入 shutdown -a\n"
        L"3. 在CMD中运行 shutdown -a\n\n"
        L"© 2025 https://github.com/rtmmtr2";

    MessageBox(hwnd, helpText.c_str(), L"使用帮助", MB_ICONINFORMATION);
}

// 显示关于对话框
void ShowAboutDialog(HWND hwnd) {
    std::wstring aboutText = L"Shutdown Timer Pro\n"
        L"版本: 2.0 \n"
        L"作者: rtmmtr2\n\n"
        L"功能：\n"
        L"• 智能定时关机管理\n"
        L"• 多种关机方式\n"
        L"• 实时倒计时显示\n"
        L"• 进度条可视化\n\n"
        L"GitHub: https://github.com/rtmmtr2\n"
        L"© 2025 版权所有";

    MessageBox(hwnd, aboutText.c_str(), L"关于 Shutdown Timer Pro", MB_ICONINFORMATION);
}

// 检查管理员权限
void CheckAdminPrivileges() {
    g_bAdminPrivileges = IsAdmin();
}

// 判断是否为管理员
bool IsAdmin() {
    BOOL isAdmin = FALSE;
    HANDLE hToken = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD dwSize;

        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
            isAdmin = elevation.TokenIsElevated;
        }

        CloseHandle(hToken);
    }

    return isAdmin != FALSE;
}

// 格式化时间显示
std::wstring FormatTime(int seconds) {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    wchar_t buffer[64];

    if (hours > 0) {
        swprintf_s(buffer, L"%d小时%d分%d秒", hours, minutes, secs);
    }
    else if (minutes > 0) {
        swprintf_s(buffer, L"%d分%d秒", minutes, secs);
    }
    else {
        swprintf_s(buffer, L"%d秒", secs);
    }

    return std::wstring(buffer);
}

// 开始倒计时
void StartCountdown(int seconds) {
    if (g_bCountdownActive) {
        KillTimer(g_hMainWnd, ID_TIMER_COUNTDOWN);
    }

    g_bCountdownActive = true;
    g_iCountdownSeconds = seconds;

    SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, seconds));
    SendMessage(g_hProgressBar, PBM_SETPOS, seconds, 0);

    SetTimer(g_hMainWnd, ID_TIMER_COUNTDOWN, 1000, NULL);
}

// 停止倒计时
void StopCountdown() {
    if (g_bCountdownActive) {
        KillTimer(g_hMainWnd, ID_TIMER_COUNTDOWN);
        g_bCountdownActive = false;
    }
}

// 更新进度条
void UpdateProgressBar(int totalSeconds, int remainingSeconds) {
    if (totalSeconds <= 0) return;

    int progress = (int)((double)remainingSeconds / totalSeconds * 100);
    SendMessage(g_hProgressBar, PBM_SETPOS, progress, 0);
}

// 切换置顶显示
void ToggleAlwaysOnTop(HWND hwnd) {
    g_bAlwaysOnTop = !g_bAlwaysOnTop;

    if (g_bAlwaysOnTop) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        UpdateStatus(L"✅ 窗口已置顶显示");
    }
    else {
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        UpdateStatus(L"🔽 窗口已取消置顶");
    }
}

// 设置按钮启用状态
void SetButtonEnabled(HWND hwnd, int id, bool enabled) {
    HWND hButton = GetDlgItem(hwnd, id);
    EnableWindow(hButton, enabled);
}
