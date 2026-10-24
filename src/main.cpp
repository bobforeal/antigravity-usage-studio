// ============================================================================
// Antigravity Usage Studio — Native C++20 / Direct2D / DirectWrite EntryPoint
// ============================================================================

#include "studio_render_part1.hpp"
#include "studio_render_part2.hpp"
#include "studio_render_part3.hpp"
#include "studio_render_part4.hpp"
#include "studio_actions.hpp"

static DesktopUsageStudioApp* g_App = nullptr;

LRESULT CALLBACK StudioWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_NCCALCSIZE: {
            if (wParam == TRUE) {
                return 0; // Remove standard non-client frame while keeping DWM rounded corners & animations
            }
            break;
        }
        case WM_SIZE: {
            if (g_App && g_App->hwndRt) {
                UINT w = LOWORD(lParam);
                UINT h = HIWORD(lParam);
                if (w > 0 && h > 0) {
                    g_App->hwndRt->Resize(D2D1::SizeU(w, h));
                    g_App->Render();
                }
            }
            return 0;
        }
        case WM_TIMER: {
            if (wParam == 1 && g_App) {
                g_App->StepAnimations();
                g_App->Render();
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd, &ps);
            if (g_App) g_App->Render();
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;

        case WM_NCHITTEST: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &pt);
            RECT rc{};
            GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            // Bottom-right resize grip & edges
            const int border = 6;
            if (pt.x >= w - 18 && pt.y >= h - 18) return HTBOTTOMRIGHT;
            if (pt.x < border && pt.y < border) return HTTOPLEFT;
            if (pt.x >= w - border && pt.y < border) return HTTOPRIGHT;
            if (pt.x < border && pt.y >= h - border) return HTBOTTOMLEFT;
            if (pt.x < border) return HTLEFT;
            if (pt.x >= w - border) return HTRIGHT;
            if (pt.y < border) return HTTOP;
            if (pt.y >= h - border) return HTBOTTOM;

            // Check if cursor is over any interactive button in the top 38px bar
            if (pt.y < 38 && g_App) {
                for (const auto& hr : g_App->hit_regions) {
                    if (pt.x >= hr.rect.left && pt.x <= hr.rect.right &&
                        pt.y >= hr.rect.top && pt.y <= hr.rect.bottom) {
                        return HTCLIENT;
                    }
                }
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_MOUSEMOVE: {
            if (g_App) {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                g_App->last_mouse = {mx, my};
                if ((wParam & MK_LBUTTON) && g_App->is_dragging_pill) {
                    g_App->HandleDragMove(static_cast<float>(mx), static_cast<float>(my));
                }
                std::string found;
                for (int i = static_cast<int>(g_App->hit_regions.size()) - 1; i >= 0; --i) {
                    const auto& hr = g_App->hit_regions[i];
                    if (mx >= hr.rect.left && mx <= hr.rect.right && my >= hr.rect.top && my <= hr.rect.bottom) {
                        found = hr.id;
                        break;
                    }
                }
                g_App->hovered_id = found;
                SetCursor(LoadCursorW(nullptr, (!found.empty() || g_App->is_dragging_pill) ? IDC_HAND : IDC_ARROW));
                if (g_App->is_dragging_pill) {
                    g_App->Render();
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (g_App) {
                float mx = static_cast<float>(GET_X_LPARAM(lParam));
                float my = static_cast<float>(GET_Y_LPARAM(lParam));
                bool hit = g_App->HandleClick(mx, my);
                if (g_App->is_dragging_pill) {
                    SetCapture(hwnd);
                }
                g_App->Render();
                if (!hit) {
                    // Allow dragging the frameless window from any empty background area
                    ReleaseCapture();
                    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                }
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (g_App && g_App->is_dragging_pill) {
                g_App->is_dragging_pill = false;
                g_App->drag_pill_prefix.clear();
                ReleaseCapture();
                g_App->Render();
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            if (g_App) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                float step = (delta > 0) ? -48.0f : 48.0f;
                if (g_App->current_view == "quotas") {
                    g_App->quotas_scroll_target = std::clamp(g_App->quotas_scroll_target + step, 0.0f, 380.0f);
                } else if (g_App->current_view == "sessions") {
                    g_App->sessions_scroll_target = std::clamp(g_App->sessions_scroll_target + step, 0.0f, 900.0f);
                } else if (g_App->current_view.starts_with("cli_")) {
                    auto& cli = g_App->cli_instances[g_App->current_view];
                    cli.scroll_y = std::clamp(cli.scroll_y + step, 0.0f, 600.0f);
                }
                g_App->Render();
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (g_App) {
                bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                if (ctrlDown) {
                    if (wParam == '1') {
                        g_App->SwitchView(g_App->cli_order.empty() ? "cli_1" : g_App->cli_order[0]);
                        g_App->Render();
                        return 0;
                    } else if (wParam == '2') {
                        g_App->SwitchView("usage");
                        g_App->Render();
                        return 0;
                    } else if (wParam == '3') {
                        g_App->SwitchView("orchestrator");
                        g_App->Render();
                        return 0;
                    } else if (wParam == '4') {
                        g_App->SwitchView("quotas");
                        g_App->Render();
                        return 0;
                    } else if (wParam == '5') {
                        g_App->SwitchView("sessions");
                        g_App->Render();
                        return 0;
                    } else if (wParam == 'T' || wParam == 'N') {
                        g_App->SpawnNewCLITab();
                        g_App->Render();
                        return 0;
                    } else if (wParam == 'B') {
                        g_App->show_sidebar = !g_App->show_sidebar;
                        g_App->Render();
                        return 0;
                    } else if (wParam == VK_TAB) {
                        static const char* kCycle[] = {"cli_1", "usage", "orchestrator", "quotas", "sessions"};
                        int cur = g_App->GetViewOrderIndex(g_App->current_view);
                        int next = (cur + 1) % 5;
                        g_App->SwitchView(kCycle[next]);
                        g_App->Render();
                        return 0;
                    }
                }
            }
            break;
        }
        case WM_CHAR: {
            if (g_App) {
                // Ignore control characters triggered by Ctrl+Key shortcuts
                if (wParam >= 32 || wParam == L'\b' || wParam == L'\r' || wParam == L'\n') {
                    g_App->HandleCharInput(static_cast<wchar_t>(wParam));
                    g_App->Render();
                }
            }
            return 0;
        }
        case WM_USER + 100: {
            if (g_App) {
                switch (wParam) {
                    case 1:
                        g_App->SpawnNewCLITab();
                        g_App->SwitchView("cli_1");
                        break;
                    case 2:
                        g_App->SwitchView("orchestrator");
                        break;
                    case 3:
                        g_App->SwitchView("usage");
                        g_App->chart_mode = "cost";
                        break;
                    case 4:
                        g_App->SwitchView("usage");
                        g_App->chart_mode = "heatmap";
                        break;
                    case 5:
                        g_App->SwitchView("quotas");
                        break;
                    case 6:
                        g_App->SwitchView("sessions");
                        break;
                }
                g_App->view_slide_x = 0.0f;
                g_App->chart_anim = 1.0f;
                g_App->quotas_anim = 1.0f;
                g_App->tab_ind_x1 = g_App->tab_ind_target_x1;
                g_App->tab_ind_x2 = g_App->tab_ind_target_x2;
                {
                    std::lock_guard<std::mutex> lk(g_App->data_mutex);
                    g_App->kpi_disp_cost = g_App->dash_data.total_cost;
                    g_App->kpi_disp_reqs = g_App->dash_data.total_requests;
                    g_App->kpi_disp_toks = static_cast<double>(g_App->dash_data.total_tokens);
                }
                g_App->Render();
                g_App->tab_ind_x1 = g_App->tab_ind_target_x1;
                g_App->tab_ind_x2 = g_App->tab_ind_target_x2;
                g_App->tab_pill_x1 = g_App->tab_ind_target_x1 - 4.0f;
                g_App->tab_pill_x2 = g_App->tab_ind_target_x2 + 4.0f;
                g_App->Render();
            }
            return 0;
        }
        case WM_CLOSE: {
            if (g_App) {
                RECT rc{};
                if (GetWindowRect(hwnd, &rc)) {
                    g_App->settings.window_x = rc.left;
                    g_App->settings.window_y = rc.top;
                    g_App->settings.window_w = rc.right - rc.left;
                    g_App->settings.window_h = rc.bottom - rc.top;
                }
                SaveAppSettings(g_App->settings);
            }
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetProcessDPIAware();

    DesktopUsageStudioApp app;
    g_App = &app;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = StudioWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"AntigravityUsageStudioCppClass";
    RegisterClassExW(&wc);

    int winW = std::max(980, app.settings.window_w);
    int winH = std::max(680, app.settings.window_h);
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int winX = (app.settings.window_x >= 0 && app.settings.window_x < scrW - 100) ? app.settings.window_x : (scrW - winW) / 2;
    int winY = (app.settings.window_y >= 0 && app.settings.window_y < scrH - 100) ? app.settings.window_y : (scrH - winH) / 2;

    DWORD exStyle = WS_EX_APPWINDOW | (app.is_pinned ? WS_EX_TOPMOST : 0);
    DWORD style = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_VISIBLE;

    HWND hwnd = CreateWindowExW(
        exStyle,
        wc.lpszClassName,
        L"Antigravity Usage Studio",
        style,
        winX, winY, winW, winH,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd) {
        CoUninitialize();
        return 1;
    }

    // Enable Windows 11 Native DWM Rounded Corners (DWMWCP_ROUND = 2, DWMWA_WINDOW_CORNER_PREFERENCE = 33)
    DWORD cornerPref = 2;
    DwmSetWindowAttribute(hwnd, 33, &cornerPref, sizeof(cornerPref));
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));

    // Extend DWM frame by 1px so frameless WS_NCCALCSIZE window keeps smooth shadow + rounded clip
    MARGINS margins = {0, 0, 0, 1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    if (!app.InitD2D(hwnd)) {
        CoUninitialize();
        return 1;
    }

    std::string cmdArgs = lpCmdLine ? lpCmdLine : "";
    bool isCaptureMode = (cmdArgs.find("--capture-previews") != std::string::npos);

    // Non-blocking startup in normal mode (< 5ms cold boot!), synchronous only when capturing preview PNGs
    app.StartPolling(isCaptureMode);

    if (isCaptureMode) {
        fs::path assetsDir = fs::path(GetAppDirectory()) / "assets";
        app.SpawnNewCLITab();

        app.SwitchView("cli_1");
        app.SaveDirect2DPng((assetsDir / "preview-cli.png").string(), 1140, 780);

        app.SwitchView("orchestrator");
        // Seed live active folder-transfer packets and running state so preview-orchestrator.png showcases the inter-worker bus!
        app.workers[0].is_running = true;
        app.workers[0].anim_phase = 0.45f;
        app.workers[0].status_text = "\xE2\xA0\x99 Running (high)";
        app.workers[0].status_color = 0xF59E0B;
        app.workers[0].logs = {
            "\xE2\x97\x8F Dispatching Worker #1 [Gemini 4 Argon (High) \xC2\xB7 effort: high]...",
            "\xF0\x9F\x93\x81 Transferred src/core folder to Worker #2 & Worker #3 (18.4k tokens)"
        };
        app.workers[1].is_running = true;
        app.workers[1].anim_phase = 0.72f;
        app.workers[1].status_text = "\xE2\xA0\xB9 Running (medium)";
        app.workers[1].status_color = 0xF59E0B;
        app.workers[1].logs = {
            "\xE2\x97\x8F Dispatching Worker #2 [Gemini 3.8 Flash (Medium) \xC2\xB7 effort: medium]...",
            "\xF0\x9F\x93\x81 Receiving src/core & streaming Direct2D UI layout to Worker #4..."
        };
        app.workers[2].progress_fill = 1.0f;
        app.workers[2].status_text = "\xE2\x9C\x93 Completed";
        app.workers[2].status_color = 0x38C77B;
        app.transfer_packets = {
            {-1, 0, 0.38f, 1.0f, "\xF0\x9F\x93\x81", "plan/spec",   0xD97757},
            { 0, 1, 0.54f, 1.0f, "\xF0\x9F\x93\x81", "src/core",    0x60A5FA},
            { 1, 3, 0.66f, 1.0f, "\xF0\x9F\x93\x81", "assets/ui",   0xC084FC},
            { 0, 2, 0.48f, 1.0f, "\xF0\x9F\x93\xA6", "tests/pass",  0x38C77B}
        };
        app.SaveDirect2DPng((assetsDir / "preview-orchestrator.png").string(), 1140, 780);

        app.SwitchView("usage");
        app.chart_mode = "cost";
        {
            std::lock_guard<std::mutex> lk(app.data_mutex);
            app.kpi_disp_cost = app.dash_data.total_cost;
            app.kpi_disp_reqs = app.dash_data.total_requests;
            app.kpi_disp_toks = static_cast<double>(app.dash_data.total_tokens);
        }
        app.SaveDirect2DPng((assetsDir / "preview-dashboard.png").string(), 1140, 780);

        app.chart_mode = "heatmap";
        app.SaveDirect2DPng((assetsDir / "preview-heatmap.png").string(), 1140, 780);

        app.SwitchView("quotas");
        app.SaveDirect2DPng((assetsDir / "preview-quotas.png").string(), 1140, 780);

        DestroyWindow(hwnd);
        g_App = nullptr;
        CoUninitialize();
        return 0;
    }

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // High-Precision ~120fps / 60fps Hardware Animation Timer (8ms interval)
    SetTimer(hwnd, 1, 8, nullptr);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_App = nullptr;
    CoUninitialize();
    return 0;
}
