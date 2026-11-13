// FnxTest UI - Custom ImGui control panel (Win32 + DX11)
// Read-only configuration/diagnostics/identity-inventory UI. No hooks, no injection, no spoofing performed.
// "Undetected" / anti-cheat blocking claims are not made.

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwmapi.lib")

static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct LogEntry { std::string time; std::string msg; };

static std::string now_str() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return buf;
}

static void add_log(std::vector<LogEntry>& log, const std::string& msg) {
    log.push_back({now_str(), msg});
    if (log.size() > 500) log.erase(log.begin(), log.begin() + (log.size() - 500));
}

static void ApplyFnxTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 5.0f;
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(8, 6);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* c = style.Colors;
    ImVec4 bg       = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);
    ImVec4 bg2      = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);
    ImVec4 panel    = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);
    ImVec4 border   = ImVec4(0.20f, 0.22f, 0.28f, 1.00f);
    ImVec4 text     = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
    ImVec4 textDim  = ImVec4(0.58f, 0.62f, 0.70f, 1.00f);
    ImVec4 accent   = ImVec4(0.22f, 0.55f, 0.95f, 1.00f); // blue accent
    ImVec4 accentH  = ImVec4(0.30f, 0.62f, 1.00f, 1.00f);
    ImVec4 accentA  = ImVec4(0.18f, 0.46f, 0.82f, 1.00f);
    ImVec4 good     = ImVec4(0.20f, 0.78f, 0.45f, 1.00f);
    ImVec4 warn     = ImVec4(0.95f, 0.72f, 0.20f, 1.00f);
    ImVec4 bad      = ImVec4(0.92f, 0.28f, 0.30f, 1.00f);

    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = textDim;
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = bg2;
    c[ImGuiCol_PopupBg] = panel;
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.18f, 0.22f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.23f, 0.28f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.26f, 0.32f, 1.0f);
    c[ImGuiCol_TitleBg] = bg2;
    c[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.14f, 0.18f, 1.0f);
    c[ImGuiCol_MenuBarBg] = panel;
    c[ImGuiCol_ScrollbarBg] = bg2;
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.28f, 0.32f, 0.40f, 1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.40f, 0.50f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accentH;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accentH;
    c[ImGuiCol_Button] = ImVec4(0.18f, 0.22f, 0.30f, 1.0f);
    c[ImGuiCol_ButtonHovered] = accent;
    c[ImGuiCol_ButtonActive] = accentA;
    c[ImGuiCol_Header] = ImVec4(0.18f, 0.30f, 0.50f, 0.7f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.40f, 0.65f, 0.8f);
    c[ImGuiCol_HeaderActive] = accent;
    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_Tab] = ImVec4(0.14f, 0.17f, 0.22f, 1.0f);
    c[ImGuiCol_TabHovered] = accent;
    c[ImGuiCol_TabActive] = ImVec4(0.18f, 0.32f, 0.52f, 1.0f);
    c[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.14f, 0.18f, 1.0f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.22f, 0.32f, 1.0f);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.16f, 0.18f, 0.24f, 1.0f);
    c[ImGuiCol_TableBorderStrong] = border;
    c[ImGuiCol_TableBorderLight] = ImVec4(0.16f, 0.18f, 0.22f, 1.0f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.22f, 0.45f, 0.80f, 0.35f);
    c[ImGuiCol_NavHighlight] = accent;
    (void)good; (void)warn; (void)bad;
}

struct HwidField { const char* label; char observed[256]; char profile[256]; const char* note; };

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"FnxTestUI", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"FnxTest Control Panel", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 820, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ApplyFnxTheme();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // State
    std::vector<LogEntry> logs;
    add_log(logs, "UI started. Read-only control panel (no hooks/injection/spoofing).");
    add_log(logs, "Use hwid_enum.exe and dev_enum.exe for BEFORE/AFTER evidence snapshots.");

    char processName[128] = "FortniteClient-Win64-Shipping.exe";
    char pidBuf[32] = "0";
    char configPath[260] = "config\\fortnite.example.ini";
    char driverPath[128] = "\\\\.\\FnxTestDrv";
    bool useDriver = false;

    // HWID inventory fields (observed filled by paste from probe; profile user-defined for testing)
    HwidField hwidFields[] = {
        {"Computer Name", "", "", "GetComputerName* (user-mode). Observer may differ."},
        {"Machine GUID", "", "", "HKLM\\SOFTWARE\\Microsoft\\Cryptography\\MachineGuid"},
        {"System UUID (SMBIOS)", "", "", "GetSystemFirmwareTable RSMB Type 1"},
        {"System Serial (SMBIOS)", "", "", "SMBIOS Type 1; firmware-dependent"},
        {"Baseboard Serial", "", "", "SMBIOS Type 2; firmware-dependent"},
        {"Disk Serial (PhysicalDrive0)", "", "", "IOCTL_STORAGE_QUERY_PROPERTY; access/format varies"},
        {"Volume Serial (C:)", "", "", "GetVolumeInformation"},
        {"Primary MAC", "", "", "GetAdaptersAddresses; adapters may change"},
        {"CPU Brand/ID", "", "", "CPUID; not a unique per-unit serial in general"},
        {"Mouse InstanceId", "", "", "SetupDi/CM; per device"},
        {"Mouse HardwareID", "", "", "SPDRP_HARDWAREID"},
        {"Mouse RawInput DevicePath", "", "", "RIDI_DEVICENAME (process/session-specific observations possible)"},
    };
    const int hwidCount = (int)(sizeof(hwidFields) / sizeof(hwidFields[0]));

    char patternEditor[4096] =
        "; Patterns must come from evidence for the exact build under test.\n"
        "[process]\nname=FortniteClient-Win64-Shipping.exe\n\n"
        "[pattern.GWorld]\nmodule=FortniteClient-Win64-Shipping.exe\npattern=SUPPLY_FOR_YOUR_BUILD\n"
        "rip_relative=0\ndisp_offset=0\nadd_offset=0\n";

    bool showDemo = false;
    bool running = true;
    ImVec4 clear_color = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);

    while (running) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Main window filling viewport
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                     ImGuiWindowFlags_MenuBar;
        ImGui::Begin("##Host", nullptr, hostFlags);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("ImGui Demo (debug)", nullptr, &showDemo);
                ImGui::EndMenu();
            }
            ImGui::TextDisabled("  FnxTest Panel  |  diagnostics + identity inventory  |  no evasion claims");
            ImGui::EndMenuBar();
        }

        if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Overview")) {
                ImGui::TextColored(ImVec4(0.30f,0.62f,1.0f,1.0f), "FnxTest Control Panel");
                ImGui::Separator();
                ImGui::TextWrapped("Engineering/test workspace: external diagnostics, user-supplied pattern resolution, HWID/mouse identity discovery, and optional KMDF diagnostic driver backend.");
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.95f,0.72f,0.20f,1.0f), "Evidence & claim boundaries (read carefully):");
                ImGui::BulletText("No Fortnite offsets/signatures/pointer chains/structures are included or fabricated. Patterns must come from evidence for the exact build and are validated/reported (MISSING/MISS/INVALID/etc.).");
                ImGui::BulletText("No 'undetected' claim and no anti-cheat evasion/bypass implemented. 'Block EAC from reading all HWID serials' cannot be guaranteed: observer and observation paths are target/environment-specific and must be identified and validated per-path.");
                ImGui::BulletText("HWID/serials are observer-dependent (user-mode APIs, drivers, kernel, firmware, app telemetry). Discovery tools here cover common OS-exposed paths only.");
                ImGui::BulletText("Any virtualization/spoofing must be authorized, narrowly targeted to an evidenced path, reversible, and verified BEFORE/AFTER. Kernel/driver work requires WDK, proper signing/policy, and controlled environment.");
                ImGui::Spacing();
                if (ImGui::Button("Log: self-test command")) add_log(logs, "Run: fn_external_test.exe --self-test (offline, expects SELF_TEST: PASS)");
                ImGui::SameLine();
                if (ImGui::Button("Log: hwid probe command")) add_log(logs, "Run: hwid-tool\\hwid_enum.exe > hwid_before.txt (evidence snapshot)");
                ImGui::SameLine();
                if (ImGui::Button("Log: mouse probe command")) add_log(logs, "Run: identity-tool\\dev_enum.exe > mouse_before.txt (evidence snapshot)");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Diagnostics")) {
                ImGui::Text("External tool (read-only) command builder");
                ImGui::Separator();
                ImGui::InputText("Process name", processName, sizeof(processName));
                ImGui::InputText("PID (0 = use name)", pidBuf, sizeof(pidBuf));
                ImGui::InputText("Config path", configPath, sizeof(configPath));
                ImGui::Checkbox("Use driver backend (KMDF diagnostic, if loaded)", &useDriver);
                if (useDriver) ImGui::InputText("Driver device", driverPath, sizeof(driverPath));
                ImGui::Spacing();

                std::string cmd = "fn_external_test.exe --diagnose ";
                if (std::string(pidBuf) != "0" && pidBuf[0] != '\0') { cmd += "--pid "; cmd += pidBuf; }
                else { cmd += "--process \""; cmd += processName; cmd += "\""; }
                if (configPath[0]) { cmd += " --config \""; cmd += configPath; cmd += "\""; }
                if (useDriver && driverPath[0]) { cmd += " --driver "; cmd += driverPath; }

                ImGui::TextWrapped("Command preview:");
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f,0.11f,0.14f,1.0f));
                static char cmdBuf[512];
#ifdef _MSC_VER
                strncpy_s(cmdBuf, cmd.c_str(), sizeof(cmdBuf)-1);
#else
                std::strncpy(cmdBuf, cmd.c_str(), sizeof(cmdBuf)-1); cmdBuf[sizeof(cmdBuf)-1]=0;
#endif
                ImGui::InputTextMultiline("##cmd", cmdBuf, sizeof(cmdBuf), ImVec2(-FLT_MIN, 60), ImGuiInputTextFlags_ReadOnly);
                ImGui::PopStyleColor();
                if (ImGui::Button("Copy/log command")) add_log(logs, std::string("CMD: ") + cmdBuf);
                ImGui::SameLine();
                if (ImGui::Button("Log self-test")) add_log(logs, "CMD: fn_external_test.exe --self-test");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.58f,0.62f,0.70f,1.0f), "Notes: self-test is fully offline. Pattern resolution requires user-supplied, build-specific signatures (no fabrication). Driver backend is diagnostic/read-only; no evasion claims.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("HWID & Serials")) {
                ImGui::TextWrapped("Inventory view for common OS-exposed identifiers (populate 'Observed' from hwid_enum.exe output). 'Profile' is a user-defined test value for authorized, reversible virtualization planning per evidenced path. This UI does not modify the system.");
                ImGui::Spacing();
                if (ImGui::Button("Clear observed")) { for(int i=0;i<hwidCount;++i) hwidFields[i].observed[0]=0; add_log(logs, "Cleared observed HWID fields."); }
                ImGui::SameLine();
                if (ImGui::Button("Clear profiles")) { for(int i=0;i<hwidCount;++i) hwidFields[i].profile[0]=0; add_log(logs, "Cleared profile fields."); }

                if (ImGui::BeginTable("hwidtbl", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Identifier", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                    ImGui::TableSetupColumn("Observed (from probe)", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Test Profile Value", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Source / Note", ImGuiTableColumnFlags_WidthFixed, 320.0f);
                    ImGui::TableHeadersRow();
                    for (int i = 0; i < hwidCount; ++i) {
                        ImGui::TableNextRow();
                        ImGui::PushID(i);
                        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(hwidFields[i].label);
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputText("##obs", hwidFields[i].observed, sizeof(hwidFields[i].observed));
                        ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputText("##prof", hwidFields[i].profile, sizeof(hwidFields[i].profile));
                        ImGui::TableSetColumnIndex(3); ImGui::TextDisabled("%s", hwidFields[i].note);
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.95f,0.72f,0.20f,1.0f), "Important: 'All serials' is observer-dependent. To block/virtualize for a specific target (e.g. EAC), you must identify its actual observation paths (user/kernel/driver/firmware/IOCTL/registry/API/telemetry), implement narrow reversible interception per path, and verify BEFORE/AFTER on that same path. Undetectability cannot be guaranteed.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Mouse / Device IDs")) {
                ImGui::TextWrapped("Mouse/HID identifiers (populate from identity-tool\\dev_enum.exe). Consumers may use different paths (SetupDi, CM, Raw Input RIDI_DEVICENAME, HID attributes, registry, driver IOCTLs).");
                ImGui::Separator();
                static char mouseObserved[1024] = "";
                static char mousePlan[1024] =
                    "1) Run dev_enum.exe > mouse_before.txt\n"
                    "2) Identify which value the consumer reads and via which API/path (trace/instrument)\n"
                    "3) Implement narrow, reversible virtualization at that path only (per-process user-mode hook preferred for tests when sufficient; driver-backed only if required)\n"
                    "4) Verify on the same path (mouse_after.txt + consumer-side check) and provide restore\n"
                    "5) No undetectability claim";
                ImGui::Text("Observed snapshot (paste relevant lines):");
                ImGui::InputTextMultiline("##mouseobs", mouseObserved, sizeof(mouseObserved), ImVec2(-FLT_MIN, 140));
                ImGui::Text("Virtualization plan (evidence-driven):");
                ImGui::InputTextMultiline("##mouseplan", mousePlan, sizeof(mousePlan), ImVec2(-FLT_MIN, 160));
                if (ImGui::Button("Log mouse probe")) add_log(logs, "Run: identity-tool\\dev_enum.exe for SetupDi/HID/Raw Input identifiers.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Driver")) {
                ImGui::Text("KMDF Diagnostic Driver Backend (optional)");
                ImGui::Separator();
                ImGui::InputText("Device path", driverPath, sizeof(driverPath));
                ImGui::TextWrapped("Shared interface: IOCTL_FNX_GET_VERSION, IOCTL_FNX_READ_MEMORY (METHOD_BUFFERED). Max read 4096 bytes/request. Uses documented MmCopyVirtualMemory with input validation; returns NTSTATUS diagnostics in response.");
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.95f,0.72f,0.20f,1.0f), "No anti-cheat evasion/undetectability claimed. Loading kernel drivers requires appropriate build (WDK/VS), signing/test-signing policy, elevation, and environment-specific steps. Test in controlled environments.");
                ImGui::Spacing();
                if (ImGui::Button("Log driver verify command")) { std::string c = "fn_external_test.exe --diagnose --process Notepad.exe --driver "; c += driverPath; add_log(logs, c); }
                ImGui::SameLine();
                if (ImGui::Button("Log driver docs")) add_log(logs, "See driver/README.md for WDK build, INF, install/load/unload, and failure diagnostics.");
                ImGui::BulletText("Expected on success: 'Driver backend: CONNECTED (version 1.0, ...)'");
                ImGui::BulletText("Failure: device missing (not loaded), access denied, IOCTL mismatch (rebuild from same shared.h), or signing/policy/start errors.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Profiles / Config")) {
                ImGui::TextWrapped("Pattern/config editor (INI). Placeholders (SUPPLY_FOR_YOUR_BUILD) are intentionally not valid signatures. No offsets/signatures are fabricated.");
                ImGui::InputTextMultiline("##cfg", patternEditor, sizeof(patternEditor), ImVec2(-FLT_MIN, 360), ImGuiInputTextFlags_AllowTabInput);
                if (ImGui::Button("Validate (basic)")) {
                    std::string s(patternEditor);
                    bool hasPlaceholder = (s.find("SUPPLY_FOR_YOUR_BUILD") != std::string::npos);
                    bool hasPattern = (s.find("[pattern.") != std::string::npos);
                    if (hasPlaceholder) add_log(logs, "Config: contains SUPPLY_FOR_YOUR_BUILD placeholders -> will report MISSING_USER_PATTERN until replaced with evidence-based patterns.");
                    else if (hasPattern) add_log(logs, "Config: no obvious placeholders detected (syntax/validity still depends on content and target build).");
                    else add_log(logs, "Config: no [pattern.*] sections found.");
                }
                ImGui::SameLine();
                if (ImGui::Button("Log config reminder")) add_log(logs, "Supply per-build patterns from evidence (disasm/signature tools). RIP-relative: set rip_relative/disp_offset/add_offset correctly.");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        // Log panel (bottom)
        ImGui::Separator();
        ImGui::Text("Log");
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) logs.clear();
        ImGui::BeginChild("logchild", ImVec2(0, 140), true, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& e : logs) {
            ImGui::TextDisabled("[%s]", e.time.c_str()); ImGui::SameLine();
            ImGui::TextUnformatted(e.msg.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        ImGui::End();

        if (showDemo) ImGui::ShowDemoWindow(&showDemo);

        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

// --- D3D11 boilerplate ---
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0; sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60; sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1; sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;
    CreateRenderTarget();
    return true;
}
void CleanupDeviceD3D() { CleanupRenderTarget(); if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain=nullptr; } if (g_pd3dDeviceContext){g_pd3dDeviceContext->Release(); g_pd3dDeviceContext=nullptr;} if (g_pd3dDevice){g_pd3dDevice->Release(); g_pd3dDevice=nullptr;} }
void CreateRenderTarget() { ID3D11Texture2D* pBackBuffer; g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)); if(pBackBuffer){ g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView); pBackBuffer->Release(); } }
void CleanupRenderTarget() { if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; } }

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
