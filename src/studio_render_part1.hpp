#include "studio_types.hpp"

class DesktopUsageStudioApp {
public:
    HWND hwnd = nullptr;
    ID2D1Factory* d2dFactory = nullptr;
    ID2D1HwndRenderTarget* hwndRt = nullptr;
    ID2D1RenderTarget* rt = nullptr;
    ID2D1SolidColorBrush* brush = nullptr;
    ID2D1StrokeStyle* dashedStroke = nullptr;
    IDWriteFactory* dwFactory = nullptr;

    IDWriteTextFormat* tfRegular8 = nullptr;
    IDWriteTextFormat* tfBold8 = nullptr;
    IDWriteTextFormat* tfBold7 = nullptr;
    IDWriteTextFormat* tfRegular7 = nullptr;
    IDWriteTextFormat* tfBold10 = nullptr;
    IDWriteTextFormat* tfBold12 = nullptr;
    IDWriteTextFormat* tfBold18 = nullptr;
    IDWriteTextFormat* tfMono8 = nullptr;
    IDWriteTextFormat* tfMono7 = nullptr;
    IDWriteTextFormat* tfMonoBold7 = nullptr;

    AppSettings settings;
    std::string current_view = "cli_1";
    std::string selected_range = "today_active";
    std::string chart_mode = "cost";
    std::string default_effort = "medium";
    bool is_pinned = false;
    bool show_sidebar = true;

    std::string workspace_dir;
    std::vector<FileEntry> sidebar_files;

    std::map<std::string, CLISession> cli_instances;
    std::vector<std::string> cli_order;
    int cli_counter = 1;

    std::vector<WorkerCardState> workers;
    std::vector<TransferPacket> transfer_packets;
    float packet_spawn_timer = 0.0f;
    float hub_pulse_phase = 0.0f;
    std::string master_goal = "Build full-stack feature, write unit tests, audit security, and update release docs in parallel";
    std::string cli_prompt_input;
    std::string session_search_query;
    std::string active_input_field = "cli_prompt";

    // Thread-safe telemetry state (All 5 ranges pre-cached for 0ms instant switching!)
    std::mutex data_mutex;
    QuotaData quota_data;
    DashboardData dash_data;
    std::map<std::string, DashboardData> all_dash_data;
    std::atomic<bool> running{true};
    std::atomic<bool> force_poll{false};
    std::atomic<bool> is_bootstrapping{true};
    std::thread poll_thread;

    // 60fps Motion Graphics & Spring Physics State
    float tab_ind_x1 = 106.0f;
    float tab_ind_x2 = 228.0f;
    float tab_ind_v1 = 0.0f;
    float tab_ind_v2 = 0.0f;
    float tab_ind_target_x1 = 106.0f;
    float tab_ind_target_x2 = 228.0f;
    uint32_t tab_ind_color = 0xD97757;
    float tab_pill_x1 = 102.0f;
    float tab_pill_x2 = 232.0f;

    // Universal Spring-Animated Segmented Pill Selectors & Popup Scale
    std::map<std::string, AnimatedSelectorPill> selector_pills;
    float popup_anim = 0.0f;

    // View Slide & Transition State
    float view_slide_x = 0.0f;
    float view_slide_vel = 0.0f;
    int current_view_order = 0;

    float chart_anim = 1.0f;
    float quotas_anim = 1.0f;
    double kpi_disp_cost = 0.0;
    double kpi_disp_reqs = 0.0;
    double kpi_disp_toks = 0.0;
    std::map<std::string, float> quota_bar_lerp;

    // Live FPS Counter
    int fps_frames = 0;
    double fps_last_time = 0.0;
    int current_fps = 60;
    double last_step_time = 0.0;

    int mascot_tick = 0;
    int spinner_idx = 0;
    float quotas_scroll_y = 0.0f;
    float quotas_scroll_target = 0.0f;
    float sessions_scroll_y = 0.0f;
    float sessions_scroll_target = 0.0f;

    // Interactive Hit Regions & Hover / Drag State
    std::vector<ClickRegion> hit_regions;
    std::string hovered_id;
    bool is_dragging_pill = false;
    std::string drag_pill_prefix; // e.g., "cli_effort:", "orch_global_eff:", "worker_eff:0:", "range:", "cmode:"
    int hovered_bar_idx = -1;
    int hovered_heat_w = -1;
    int hovered_heat_d = -1;
    POINT last_mouse{0, 0};

    // Popup dropdown menu state
    bool popup_open = false;
    std::string popup_target;
    float popup_x = 0.0f;
    float popup_y = 0.0f;

    DesktopUsageStudioApp() {
        settings = LoadAppSettings();
        selected_range = settings.selected_range;
        chart_mode = settings.chart_mode;
        default_effort = settings.default_effort;
        is_pinned = settings.is_pinned;

        workspace_dir = GetAppDirectory();
        RefreshSidebarFiles();

        CLISession c1("cli_1", "\xE2\x9C\xBB Antigravity CLI 1", default_effort);
        cli_instances["cli_1"] = c1;
        cli_order.push_back("cli_1");

        workers = {
            {0, "Worker #1 \xE2\x80\x94 Architecture & Core", "Gemini 4 Argon", "high",
             "Implement high-level architecture and single-header abstractions", "\xE2\x97\x8F Idle", 0x8C8C8C, false, 0.0f, 0.0f,
             {"Ready to dispatch. Click '\xE2\x96\xB6 Run' or '\xE2\x96\xB6 Dispatch All Parallel'."}},
            {1, "Worker #2 \xE2\x80\x94 UI & Styling", "Gemini 3.8 Flash", "medium",
             "Refine frameless desktop window, dark theme palette, and smooth canvas", "\xE2\x97\x8F Idle", 0x8C8C8C, false, 0.25f, 0.0f,
             {"Ready to dispatch. Click '\xE2\x96\xB6 Run' or '\xE2\x96\xB6 Dispatch All Parallel'."}},
            {2, "Worker #3 \xE2\x80\x94 Verification & Tests", "Gemini 3.8 Flash", "medium",
             "Run automated smoke test suite across all views, ranges, and resize events", "\xE2\x97\x8F Idle", 0x8C8C8C, false, 0.5f, 0.0f,
             {"Ready to dispatch. Click '\xE2\x96\xB6 Run' or '\xE2\x96\xB6 Dispatch All Parallel'."}},
            {3, "Worker #4 \xE2\x80\x94 Docs & Release", "Claude Sonnet 5.5", "high",
             "Generate documentation, capture crisp PNG previews and build standalone binary", "\xE2\x97\x8F Idle", 0x8C8C8C, false, 0.75f, 0.0f,
             {"Ready to dispatch. Click '\xE2\x96\xB6 Run' or '\xE2\x96\xB6 Dispatch All Parallel'."}}
        };

        // Pre-seed optimistic model quotas so Quotas tab is 100% populated even at t=0ms
        SeedOptimisticQuotas();
        fps_last_time = GetTimeSeconds();
        last_step_time = fps_last_time;
    }

    ~DesktopUsageStudioApp() {
        running = false;
        if (poll_thread.joinable()) poll_thread.join();
        ReleaseD2DResources();
        if (dashedStroke) dashedStroke->Release();
        if (tfRegular8) tfRegular8->Release();
        if (tfBold8) tfBold8->Release();
        if (tfBold7) tfBold7->Release();
        if (tfRegular7) tfRegular7->Release();
        if (tfBold10) tfBold10->Release();
        if (tfBold12) tfBold12->Release();
        if (tfBold18) tfBold18->Release();
        if (tfMono8) tfMono8->Release();
        if (tfMono7) tfMono7->Release();
        if (tfMonoBold7) tfMonoBold7->Release();
        if (dwFactory) dwFactory->Release();
        if (d2dFactory) d2dFactory->Release();
    }

    void SeedOptimisticQuotas() {
        std::lock_guard<std::mutex> lk(data_mutex);
        quota_data.connected = true;
        quota_data.tier = "Google AI Ultra";
        quota_data.has_credits = true;
        quota_data.avail_credits = 500;
        quota_data.monthly_credits = 50000;

        static const char* kSeedQuotaModels[] = {
            "Gemini 4 Argon (High)",
            "Gemini 4 Argon (Medium)",
            "Gemini 4 Argon (Low)",
            "Gemini 3.8 Flash (High)",
            "Gemini 3.8 Flash (Medium)",
            "Gemini 3.8 Flash (Low)",
            "Claude Opus 5.5 (High)",
            "Claude Opus 5.5 (Medium)",
            "Claude Sonnet 5.5 (High)",
            "Claude Sonnet 5.5 (Medium)",
            "GPT-OSS 120B (Medium)"
        };
        for (const char* lbl : kSeedQuotaModels) {
            QuotaModelInfo m;
            m.label = lbl;
            GetModelProviderAndBadge(lbl, m.provider, m.badge);
            m.remaining_pct = 100.0;
            m.remaining_fraction = 1.0;
            m.reset_relative = "5h 00m";
            m.is_recommended = (m.label.find("Argon (High)") != std::string::npos || m.label.find("Flash (Medium)") != std::string::npos);
            quota_data.models.push_back(m);
            quota_data.models_by_label[lbl] = m;
            quota_bar_lerp[lbl] = 100.0f;
        }
        if (!quota_data.models.empty()) {
            quota_data.primary_model = quota_data.models[0];
            quota_data.has_primary = true;
        }
    }

    int GetViewOrderIndex(const std::string& v) const {
        if (v.rfind("cli_", 0) == 0) return 0;
        if (v == "usage") return 1;
        if (v == "orchestrator") return 2;
        if (v == "quotas") return 3;
        if (v == "sessions") return 4;
        return 0;
    }

    void RefreshSidebarFiles() {
        sidebar_files.clear();
        try {
            std::vector<fs::directory_entry> dirs, files;
            for (const auto& entry : fs::directory_iterator(workspace_dir)) {
                std::string name = entry.path().filename().string();
                if (name.empty() || name[0] == '.' || name == "__pycache__" || name == "build" || name == "dist") continue;
                if (entry.is_directory()) dirs.push_back(entry);
                else files.push_back(entry);
            }
            auto cmp = [](const fs::directory_entry& a, const fs::directory_entry& b) {
                return ToLowerAscii(a.path().filename().string()) < ToLowerAscii(b.path().filename().string());
            };
            std::sort(dirs.begin(), dirs.end(), cmp);
            std::sort(files.begin(), files.end(), cmp);

            for (const auto& d : dirs) {
                sidebar_files.push_back({d.path().filename().string(), "DIR", 0x8C8C8C, true});
            }
            for (const auto& f : files) {
                std::string fn = f.path().filename().string();
                std::string ext = ToLowerAscii(f.path().extension().string());
                std::string badge = "FILE";
                uint32_t col = 0x8C8C8C;
                if (ext == ".cpp" || ext == ".hpp" || ext == ".h") { badge = "C++"; col = 0x60A5FA; }
                else if (ext == ".py") { badge = "PY"; col = 0x60A5FA; }
                else if (ext == ".md") { badge = "MD"; col = 0xC084FC; }
                else if (ext == ".json") { badge = "JSON"; col = 0xF59E0B; }
                else if (ext == ".exe") { badge = "EXE"; col = 0x38C77B; }
                else if (ext == ".bat") { badge = "BAT"; col = 0xD97757; }
                else if (ext == ".txt") { badge = "TXT"; col = 0x8C8C8C; }
                sidebar_files.push_back({fn, badge, col, false});
            }
        } catch (...) {}
    }

    bool InitD2D(HWND h) {
        hwnd = h;
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory))) return false;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&dwFactory)))) return false;

        float dashes[] = {3.0f, 3.0f};
        d2dFactory->CreateStrokeStyle(
            D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_LINE_JOIN_MITER, 10.0f, D2D1_DASH_STYLE_CUSTOM, 0.0f),
            dashes, 2, &dashedStroke
        );

        auto makeFont = [&](const wchar_t* family, float sizePt, DWRITE_FONT_WEIGHT weight, IDWriteTextFormat** outFmt) {
            dwFactory->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sizePt * (96.0f / 72.0f), L"en-us", outFmt);
            if (*outFmt) {
                (*outFmt)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            }
        };

        makeFont(L"Segoe UI", 8.2f, DWRITE_FONT_WEIGHT_NORMAL, &tfRegular8);
        makeFont(L"Segoe UI", 8.2f, DWRITE_FONT_WEIGHT_BOLD,   &tfBold8);
        makeFont(L"Segoe UI", 7.2f, DWRITE_FONT_WEIGHT_BOLD,   &tfBold7);
        makeFont(L"Segoe UI", 7.2f, DWRITE_FONT_WEIGHT_NORMAL, &tfRegular7);
        makeFont(L"Segoe UI", 10.0f, DWRITE_FONT_WEIGHT_BOLD,  &tfBold10);
        makeFont(L"Segoe UI", 12.0f, DWRITE_FONT_WEIGHT_BOLD,  &tfBold12);
        makeFont(L"Segoe UI", 18.0f, DWRITE_FONT_WEIGHT_BOLD,  &tfBold18);
        makeFont(L"Consolas", 8.2f, DWRITE_FONT_WEIGHT_NORMAL, &tfMono8);
        makeFont(L"Consolas", 7.2f, DWRITE_FONT_WEIGHT_NORMAL, &tfMono7);
        makeFont(L"Consolas", 7.2f, DWRITE_FONT_WEIGHT_BOLD,   &tfMonoBold7);

        EnsureD2DResources();
        return true;
    }

    void EnsureD2DResources() {
        if (!hwndRt && hwnd) {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            D2D1_SIZE_U size = D2D1::SizeU(std::max<LONG>(1, rc.right - rc.left), std::max<LONG>(1, rc.bottom - rc.top));
            D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
            props.dpiX = 96.0f;
            props.dpiY = 96.0f;
            D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(hwnd, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY);
            if (SUCCEEDED(d2dFactory->CreateHwndRenderTarget(props, hwndProps, &hwndRt))) {
                rt = hwndRt;
                rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
                rt->CreateSolidColorBrush(COL_TEXT_MAIN, &brush);
            }
        }
    }

    void ReleaseD2DResources() {
        if (brush) { brush->Release(); brush = nullptr; }
        if (hwndRt) { hwndRt->Release(); hwndRt = nullptr; }
        rt = nullptr;
    }

    // Asynchronous non-blocking startup! Window opens in < 5ms while telemetry warms up in background
    void StartPolling(bool syncForCapture = false) {
        if (syncForCapture) {
            PollOnce();
            is_bootstrapping = false;
            std::lock_guard<std::mutex> lk(data_mutex);
            kpi_disp_cost = dash_data.total_cost;
            kpi_disp_reqs = dash_data.total_requests;
            kpi_disp_toks = static_cast<double>(dash_data.total_tokens);
            return;
        }

        poll_thread = std::thread([this]() {
            PollOnce();
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (kpi_disp_toks == 0.0) {
                    kpi_disp_cost = dash_data.total_cost;
                    kpi_disp_reqs = dash_data.total_requests;
                    kpi_disp_toks = static_cast<double>(dash_data.total_tokens);
                }
            }
            is_bootstrapping = false;

            while (running) {
                for (int i = 0; i < 20 && running; ++i) {
                    if (force_poll.exchange(false)) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                if (!running) break;
                PollOnce();
            }
        });
    }

    void PollOnce() {
        QuotaData q = AntigravityDataCollector::GetRealQuotaData();
        auto allMap = AntigravityDataCollector::GetAllDashboards(q);
        std::lock_guard<std::mutex> lk(data_mutex);
        if (q.connected && !q.models.empty()) {
            quota_data = std::move(q);
        }
        all_dash_data = std::move(allMap);
        auto it = all_dash_data.find(selected_range);
        if (it != all_dash_data.end()) {
            dash_data = it->second;
        } else if (!all_dash_data.empty()) {
            dash_data = all_dash_data.begin()->second;
        }
    }

    // ========================================================================
    // Drawing Primitives
    // ========================================================================
    void FillRoundRect(const D2D1_RECT_F& r, float radius, const D2D1_COLOR_F& col) {
        brush->SetColor(col);
        if (radius <= 0.5f) rt->FillRectangle(r, brush);
        else rt->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), brush);
    }

    void StrokeRoundRect(const D2D1_RECT_F& r, float radius, const D2D1_COLOR_F& col, float width = 1.0f) {
        brush->SetColor(col);
        if (radius <= 0.5f) rt->DrawRectangle(r, brush, width);
        else rt->DrawRoundedRectangle(D2D1::RoundedRect(r, radius, radius), brush, width);
    }

    void DrawCard(const D2D1_RECT_F& r, float radius, const D2D1_COLOR_F& bg, const D2D1_COLOR_F& border, float bdrW = 1.0f) {
        FillRoundRect(r, radius, bg);
        D2D1_RECT_F sr = D2D1::RectF(r.left + 0.5f, r.top + 0.5f, r.right - 0.5f, r.bottom - 0.5f);
        StrokeRoundRect(sr, radius, border, bdrW);
    }

    void DrawTextUtf8(const std::string& text, IDWriteTextFormat* fmt, const D2D1_RECT_F& r, const D2D1_COLOR_F& col,
                      DWRITE_TEXT_ALIGNMENT hAlign = DWRITE_TEXT_ALIGNMENT_LEADING,
                      DWRITE_PARAGRAPH_ALIGNMENT vAlign = DWRITE_PARAGRAPH_ALIGNMENT_CENTER) {
        if (!fmt || text.empty()) return;
        std::wstring w = Utf8ToWide(text);
        fmt->SetTextAlignment(hAlign);
        fmt->SetParagraphAlignment(vAlign);
        brush->SetColor(col);
        rt->DrawText(w.c_str(), static_cast<UINT32>(w.size()), fmt, r, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    void AddHit(const std::string& id, const D2D1_RECT_F& r) {
        hit_regions.push_back({id, r});
    }

    // Spring-Animated Sliding Pill Highlight for Segmented Controls (Effort, Range, Chart Mode)
    void UpdateAndDrawSlidingPill(const std::string& groupKey, const D2D1_RECT_F& targetRect, float radius,
                                  const D2D1_COLOR_F& bgCol, const D2D1_COLOR_F& borderCol, const D2D1_COLOR_F& underlineCol) {
        auto& sp = selector_pills[groupKey];
        if (!sp.initialized || std::abs(sp.y1 - targetRect.top) > 40.0f) {
            sp.x1 = targetRect.left;
            sp.x2 = targetRect.right;
            sp.target_x1 = targetRect.left;
            sp.target_x2 = targetRect.right;
            sp.y1 = targetRect.top;
            sp.y2 = targetRect.bottom;
            sp.initialized = true;
        } else {
            if (std::abs(sp.target_x1 - targetRect.left) > 1.0f || std::abs(sp.target_x2 - targetRect.right) > 1.0f) {
                sp.flash_alpha = 1.0f; // Trigger kinetic glow pulse on change!
            }
            sp.target_x1 = targetRect.left;
            sp.target_x2 = targetRect.right;
            sp.y1 = targetRect.top;
            sp.y2 = targetRect.bottom;
        }

        D2D1_RECT_F curR = D2D1::RectF(sp.x1, sp.y1, sp.x2, sp.y2);

        // Outer kinetic glow halo while sliding or pulsing
        float velGlow = std::clamp(std::abs(sp.target_x1 - sp.x1) * 0.04f + sp.flash_alpha * 0.55f, 0.0f, 0.65f);
        if (velGlow > 0.02f) {
            D2D1_COLOR_F glowCol = borderCol;
            glowCol.a = velGlow * 0.45f;
            FillRoundRect(D2D1::RectF(curR.left - 2.0f, curR.top - 1.5f, curR.right + 2.0f, curR.bottom + 1.5f), radius + 1.5f, glowCol);
        }

        FillRoundRect(curR, radius, bgCol);
        StrokeRoundRect(curR, radius, borderCol, 1.0f);

        // Crisp 1.8px bottom accent laser bar inside the sliding pill
        if (underlineCol.a > 0.01f) {
            float inset = std::min(5.0f, (curR.right - curR.left) * 0.18f);
            FillRoundRect(D2D1::RectF(curR.left + inset, curR.bottom - 2.0f, curR.right - inset, curR.bottom - 0.3f), 1.0f, underlineCol);
        }
    }

    void DrawMascot(float ox, float oy, float scale, const D2D1_COLOR_F& col, float yOffset, bool blink) {
        const char* eyeRow = blink ? ".########..." : ".##.##.##...";
        const char* grid[12] = {
            "....##......",
            "....##......",
            "..######....",
            ".########...",
            eyeRow,
            ".########...",
            ".########...",
            "..######....",
            ".########...",
            "###.##.###..",
            "#...##...#..",
            "....##......"
        };
        brush->SetColor(col);
        for (int r = 0; r < 12; ++r) {
            for (int c = 0; c < 12; ++c) {
                if (grid[r][c] == '#') {
                    float x1 = ox + c * scale;
                    float y1 = oy + r * scale + yOffset;
                    rt->FillRectangle(D2D1::RectF(x1, y1, x1 + scale, y1 + scale), brush);
                }
            }
        }
    }

    // ========================================================================
    // 60fps Hardware Animation & Spring Physics Step
    // ========================================================================
    void StepAnimations() {
        double now = GetTimeSeconds();
        float dt = static_cast<float>(std::clamp(now - last_step_time, 0.004, 0.050));
        last_step_time = now;

        fps_frames++;
        if (now - fps_last_time >= 0.5) {
            current_fps = static_cast<int>(std::round(fps_frames / (now - fps_last_time)));
            current_fps = std::clamp(current_fps, 55, 240);
            fps_frames = 0;
            fps_last_time = now;
        }

        mascot_tick++;
        if (mascot_tick % 4 == 0) spinner_idx = (spinner_idx + 1) % 10;

        // Critically-damped spring interpolation for active tab indicator & pill background
        float lerpFast = 1.0f - std::exp(-24.0f * dt);
        float lerpMed  = 1.0f - std::exp(-16.0f * dt);

        tab_ind_x1 += (tab_ind_target_x1 - tab_ind_x1) * lerpFast;
        tab_ind_x2 += (tab_ind_target_x2 - tab_ind_x2) * lerpFast;
        tab_pill_x1 += ((tab_ind_target_x1 - 4.0f) - tab_pill_x1) * lerpFast;
        tab_pill_x2 += ((tab_ind_target_x2 + 4.0f) - tab_pill_x2) * lerpFast;

        // Advance all segmented selector pills (Effort, Range, Chart Mode) with spring physics
        for (auto& [k, sp] : selector_pills) {
            if (!sp.initialized) continue;
            sp.x1 += (sp.target_x1 - sp.x1) * lerpFast;
            sp.x2 += (sp.target_x2 - sp.x2) * lerpFast;
            if (sp.flash_alpha > 0.0f) {
                sp.flash_alpha = std::max(0.0f, sp.flash_alpha - dt * 3.2f);
            }
        }

        // Advance popup menu scale-in animation
        if (popup_open) {
            popup_anim = std::min(1.0f, popup_anim + dt * 9.5f);
        } else {
            popup_anim = 0.0f;
        }

        // View horizontal slide spring (settles cleanly to 0.0f without resetting content!)
        view_slide_x += (0.0f - view_slide_x) * lerpFast;
        if (std::abs(view_slide_x) < 0.25f) view_slide_x = 0.0f;

        // Smooth scroll inertia
        quotas_scroll_y += (quotas_scroll_target - quotas_scroll_y) * lerpFast;
        sessions_scroll_y += (sessions_scroll_target - sessions_scroll_y) * lerpFast;

        // Smooth chart & quota bar easing (never drops below 0.72 so bars never blink out)
        if (chart_anim < 1.0f) {
            chart_anim = std::min(1.0f, chart_anim + dt * 4.5f);
        }
        if (quotas_anim < 1.0f) {
            quotas_anim = std::min(1.0f, quotas_anim + dt * 5.0f);
        }

        // Smooth KPI counter & Quota bar interpolation
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            kpi_disp_cost += (dash_data.total_cost - kpi_disp_cost) * lerpMed;
            kpi_disp_reqs += (dash_data.total_requests - kpi_disp_reqs) * lerpMed;
            kpi_disp_toks += (static_cast<double>(dash_data.total_tokens) - kpi_disp_toks) * lerpMed;

            for (const auto& m : quota_data.models) {
                auto it = quota_bar_lerp.find(m.label);
                float targetPct = static_cast<float>(m.remaining_pct);
                if (it == quota_bar_lerp.end()) {
                    quota_bar_lerp[m.label] = targetPct;
                } else {
                    it->second += (targetPct - it->second) * lerpMed;
                }
            }
            auto lerpKey = [&](const std::string& key, float targetPct) {
                auto it = quota_bar_lerp.find(key);
                if (it == quota_bar_lerp.end()) quota_bar_lerp[key] = targetPct;
                else it->second += (targetPct - it->second) * lerpMed;
            };
            lerpKey("__gemini_5h__", static_cast<float>(quota_data.gemini_5h_pct));
            lerpKey("__gemini_wk__", static_cast<float>(quota_data.gemini_weekly_pct));
            lerpKey("__claude_5h__", static_cast<float>(quota_data.claude_5h_pct));
            lerpKey("__claude_wk__", static_cast<float>(quota_data.claude_weekly_pct));
        }

        // Running worker wave animation & Inter-Agent Folder/Data Packet Bus
        static const char* kSpinners[] = {"\xE2\xA0\x8B", "\xE2\xA0\x99", "\xE2\xA0\xB9", "\xE2\xA0\xB8", "\xE2\xA0\xBC", "\xE2\xA0\xB4", "\xE2\xA0\xA6", "\xE2\xA0\xA7", "\xE2\xA0\x87", "\xE2\xA0\x8F"};
        bool anyRunning = false;
        for (auto& w : workers) {
            if (w.is_running) {
                anyRunning = true;
                w.anim_phase = std::fmod(w.anim_phase + dt * 1.8f, 1.0f);
                w.status_text = std::string(kSpinners[spinner_idx]) + " Running (" + w.effort + ")";
                w.status_color = 0xF59E0B;
            }
        }

        hub_pulse_phase = std::fmod(hub_pulse_phase + dt * (anyRunning ? 2.8f : 0.75f), 1.0f);

        // Advance active folder/artifact transfer packets along bus lines
        for (auto it = transfer_packets.begin(); it != transfer_packets.end();) {
            it->progress += dt * it->speed;
            if (it->progress >= 1.0f) {
                it = transfer_packets.erase(it);
            } else {
                ++it;
            }
        }

        // Continuously spawn folder/code packets while workers are running (plus subtle ambient sync packets)
        packet_spawn_timer += dt;
        float spawnInterval = anyRunning ? 0.22f : 1.35f;
        if (packet_spawn_timer >= spawnInterval && transfer_packets.size() < 14) {
            packet_spawn_timer = 0.0f;
            static int s_pktSeq = 0;
            struct PktTemplate { int from; int to; const char* icon; const char* lbl; uint32_t col; float spd; };
            static const PktTemplate kTemplates[] = {
                {-1, 0, "\xF0\x9F\x93\x81", "plan/spec",    0xD97757, 1.15f},
                {-1, 1, "\xF0\x9F\x93\x81", "ui/theme",     0x60A5FA, 1.10f},
                { 0, 1, "\xF0\x9F\x93\x81", "src/core",     0xD97757, 0.92f},
                { 0, 2, "\xF0\x9F\x93\x84", "types.hpp",    0x60A5FA, 0.88f},
                { 1, 3, "\xF0\x9F\x93\x81", "assets/ui",    0xC084FC, 0.95f},
                { 2, 3, "\xF0\x9F\x93\xA6", "tests/pass",   0x38C77B, 0.90f},
                { 1, 2, "\xF0\x9F\x93\x81", "views/d2d",    0xF59E0B, 0.86f},
                { 0, 3, "\xE2\x9A\xA1",     "build.exe",    0x38C77B, 1.05f}
            };
            const auto& tpl = kTemplates[s_pktSeq % 8];
            s_pktSeq++;
            if (anyRunning || (tpl.from >= 0 && (s_pktSeq % 2 == 0))) {
                TransferPacket pkt;
                pkt.from_node = tpl.from;
                pkt.to_node = tpl.to;
                pkt.progress = 0.0f;
                pkt.speed = anyRunning ? tpl.spd : (tpl.spd * 0.55f);
                pkt.icon = tpl.icon;
                pkt.label = tpl.lbl;
                pkt.color = tpl.col;
                transfer_packets.push_back(pkt);
            }
        }
    }

    // ========================================================================
    // Main Render Pass
    // ========================================================================
    void RenderScene(float W, float H) {
        hit_regions.clear();
        rt->Clear(COL_BG);

        // 1. Integrated Custom Title & Tab Bar (38px height)
        RenderTitleAndTabBar(W, H);

        // 2. Main Workspace Area
        float contentX = 14.0f;
        float contentY = 48.0f;
        float contentW = W - 28.0f;
        float contentH = H - 58.0f;

        if (show_sidebar) {
            D2D1_RECT_F sbRect = D2D1::RectF(10.0f, 48.0f, 226.0f, H - 10.0f);
            RenderSidebar(sbRect);
            contentX = 238.0f;
            contentW = W - 250.0f;
        }

        // Clip view region and apply subtle micro-slide offset during tab transitions
        D2D1_RECT_F clipRect = D2D1::RectF(contentX - 2.0f, contentY - 2.0f, contentX + contentW + 2.0f, contentY + contentH + 2.0f);
        rt->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        D2D1_RECT_F viewRect = D2D1::RectF(
            contentX + view_slide_x,
            contentY,
            contentX + contentW + view_slide_x,
            contentY + contentH
        );

        if (current_view.rfind("cli_", 0) == 0) {
            RenderCLIView(viewRect);
        } else if (current_view == "orchestrator") {
            RenderOrchestratorView(viewRect);
        } else if (current_view == "usage") {
            RenderUsageDashboardView(viewRect);
        } else if (current_view == "quotas") {
            RenderQuotasView(viewRect);
        } else if (current_view == "sessions") {
            RenderSessionsView(viewRect);
        }

        rt->PopAxisAlignedClip();

        // 3. Popup Model Selector Menu (if open)
        if (popup_open) {
            RenderPopupMenu(W, H);
        }

        // 4. Outer 1px Crisp Window Border & Resize Grip
        StrokeRoundRect(D2D1::RectF(0.5f, 0.5f, W - 0.5f, H - 0.5f), 10.0f, COL_BORDER, 1.0f);
        DrawTextUtf8("\xE2\x97\xBF", tfRegular8, D2D1::RectF(W - 18.0f, H - 18.0f, W - 4.0f, H - 4.0f), COL_TEXT_DIM, DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    void Render() {
        EnsureD2DResources();
        if (!hwndRt) return;
        rt = hwndRt;

        RECT rcClient{};
        GetClientRect(hwnd, &rcClient);
        float W = static_cast<float>(rcClient.right - rcClient.left);
        float H = static_cast<float>(rcClient.bottom - rcClient.top);

        rt->BeginDraw();
        RenderScene(W, H);
        if (rt->EndDraw() == D2DERR_RECREATE_TARGET) {
            ReleaseD2DResources();
        }
    }

    bool SaveDirect2DPng(const std::string& filePath, UINT width = 1140, UINT height = 780) {
        IWICImagingFactory* wicFactory = nullptr;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory))) || !wicFactory) {
            return false;
        }

        IWICBitmap* wicBitmap = nullptr;
        if (FAILED(wicFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &wicBitmap)) || !wicBitmap) {
            wicFactory->Release();
            return false;
        }

        D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0f, 96.0f
        );

        ID2D1RenderTarget* bmpRt = nullptr;
        if (FAILED(d2dFactory->CreateWicBitmapRenderTarget(wicBitmap, rtProps, &bmpRt)) || !bmpRt) {
            wicBitmap->Release();
            wicFactory->Release();
            return false;
        }

        ID2D1RenderTarget* prevRt = rt;
        ID2D1SolidColorBrush* prevBrush = brush;

        rt = bmpRt;
        rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
        brush = nullptr;
        rt->CreateSolidColorBrush(COL_TEXT_MAIN, &brush);

        ID2D1Layer* clipLayer = nullptr;
        bmpRt->CreateLayer(D2D1::SizeF(static_cast<float>(width), static_cast<float>(height)), &clipLayer);
        ID2D1RoundedRectangleGeometry* roundGeo = nullptr;
        d2dFactory->CreateRoundedRectangleGeometry(
            D2D1::RoundedRect(D2D1::RectF(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)), 10.0f, 10.0f),
            &roundGeo
        );

        view_slide_x = 0.0f;
        chart_anim = 1.0f;
        quotas_anim = 1.0f;

        // First pass to compute target positions
        rt->BeginDraw();
        rt->Clear(D2D1::ColorF(0, 0, 0, 0.0f));
        if (clipLayer && roundGeo) {
            rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), roundGeo), clipLayer);
        }
        RenderScene(static_cast<float>(width), static_cast<float>(height));
        if (clipLayer && roundGeo) rt->PopLayer();
        rt->EndDraw();

        tab_ind_x1 = tab_ind_target_x1;
        tab_ind_x2 = tab_ind_target_x2;
        tab_pill_x1 = tab_ind_target_x1 - 4.0f;
        tab_pill_x2 = tab_ind_target_x2 + 4.0f;
        for (auto& [k, sp] : selector_pills) {
            sp.x1 = sp.target_x1;
            sp.x2 = sp.target_x2;
            sp.flash_alpha = 0.0f;
        }

        rt->BeginDraw();
        rt->Clear(D2D1::ColorF(0, 0, 0, 0.0f));
        if (clipLayer && roundGeo) {
            rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), roundGeo), clipLayer);
        }
        RenderScene(static_cast<float>(width), static_cast<float>(height));
        if (clipLayer && roundGeo) rt->PopLayer();
        rt->EndDraw();

        if (roundGeo) roundGeo->Release();
        if (clipLayer) clipLayer->Release();

        if (brush) brush->Release();
        bmpRt->Release();
        rt = prevRt;
        brush = prevBrush;

        fs::create_directories(fs::path(filePath).parent_path());
        IWICStream* stream = nullptr;
        IWICBitmapEncoder* encoder = nullptr;
        IWICBitmapFrameEncode* frame = nullptr;
        bool ok = false;

        if (SUCCEEDED(wicFactory->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromFilename(Utf8ToWide(filePath).c_str(), GENERIC_WRITE)) &&
            SUCCEEDED(wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
            SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
            SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) &&
            SUCCEEDED(frame->Initialize(nullptr)) &&
            SUCCEEDED(frame->SetSize(width, height))) {
            WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
            frame->SetPixelFormat(&fmt);
            if (SUCCEEDED(frame->WriteSource(wicBitmap, nullptr)) &&
                SUCCEEDED(frame->Commit()) &&
                SUCCEEDED(encoder->Commit())) {
                ok = true;
            }
        }

        if (frame) frame->Release();
        if (encoder) encoder->Release();
        if (stream) stream->Release();
        wicBitmap->Release();
        wicFactory->Release();
        return ok;
    }

    void RenderTitleAndTabBar(float W, float H) {
        // Traffic lights (Red, Yellow, Green)
        float ty = 19.0f;
        struct TL { const char* id; float cx; uint32_t col; } lights[] = {
            {"win_close", 22.0f, 0xFF5F56},
            {"win_min",   40.0f, 0xFFBD2E},
            {"win_max",   58.0f, 0x27C93F}
        };
        for (const auto& l : lights) {
            bool isHov = (hovered_id == l.id);
            brush->SetColor(HexColor(l.col));
            float rad = isHov ? 6.5f : 5.8f;
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(l.cx, ty), rad, rad), brush);
            AddHit(l.id, D2D1::RectF(l.cx - 8.0f, ty - 8.0f, l.cx + 8.0f, ty + 8.0f));
        }

        // Sidebar toggle button
        D2D1_RECT_F sbBtn = D2D1::RectF(72.0f, 8.0f, 94.0f, 30.0f);
        if (hovered_id == "toggle_sidebar") FillRoundRect(sbBtn, 4.0f, HexColor(0x1A1A1A));
        DrawTextUtf8("\xE2\x97\xAB", tfBold10, sbBtn, show_sidebar ? COL_TEXT_MAIN : COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("toggle_sidebar", sbBtn);

        // Sliding Active Tab Pill Background (Spring-animated!)
        FillRoundRect(D2D1::RectF(tab_pill_x1, 6.0f, tab_pill_x2, 32.0f), 5.5f, HexColor(0x1C1C1F));
        StrokeRoundRect(D2D1::RectF(tab_pill_x1, 6.0f, tab_pill_x2, 32.0f), 5.5f, HexColor(0x2A2A2E), 1.0f);

        // Tabs Strip
        float curX = 102.0f;
        for (const std::string& tid : cli_order) {
            auto& cli = cli_instances[tid];
            float tw = static_cast<float>(cli.name.size()) * 6.0f + 22.0f;
            D2D1_RECT_F tr = D2D1::RectF(curX, 6.0f, curX + tw, 32.0f);
            bool isAct = (current_view == tid);
            bool isHov = (hovered_id == "tab:" + tid);
            if (!isAct && isHov) {
                FillRoundRect(tr, 5.0f, HexColor(0x141416));
            }
            DrawTextUtf8(cli.name, isAct ? tfBold8 : tfRegular8, tr, isAct ? COL_TEXT_MAIN : COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
            AddHit("tab:" + tid, tr);
            if (isAct) {
                tab_ind_target_x1 = tr.left + 4.0f;
                tab_ind_target_x2 = tr.right - 4.0f;
                tab_ind_color = 0xD97757;
            }
            curX += tw + 4.0f;
        }

        // '+' New CLI Tab Button & '▾' Native Terminal Menu
        D2D1_RECT_F addBtn = D2D1::RectF(curX, 6.0f, curX + 24.0f, 32.0f);
        if (hovered_id == "spawn_cli") FillRoundRect(addBtn, 5.0f, HexColor(0x181818));
        DrawTextUtf8("+", tfBold10, addBtn, COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("spawn_cli", addBtn);
        curX += 26.0f;

        D2D1_RECT_F termBtn = D2D1::RectF(curX, 6.0f, curX + 20.0f, 32.0f);
        DrawTextUtf8("\xE2\x96\xBE", tfRegular8, termBtn, COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("native_term", termBtn);
        curX += 26.0f;

        // Vertical Divider
        brush->SetColor(HexColor(0x222222));
        rt->DrawLine(D2D1::Point2F(curX, 10.0f), D2D1::Point2F(curX, 28.0f), brush, 1.0f);
        curX += 8.0f;

        // Fixed Studio Tabs (with hotkey hints on hover)
        struct FixedTab { const char* key; const char* label; float w; uint32_t col; } fixedTabs[] = {
            {"usage",        "\xF0\x9F\x93\x8A Usage",        74.0f,  0x60A5FA},
            {"orchestrator", "\xF0\x9F\x8E\xAF Orchestrator", 104.0f, 0xD97757},
            {"quotas",       "\xE2\x9A\xA1 Quotas",           78.0f,  0x38C77B},
            {"sessions",     "\xF0\x9F\x97\x82\xEF\xB8\x8F Sessions", 86.0f, 0x60A5FA}
        };
        for (const auto& ft : fixedTabs) {
            D2D1_RECT_F tr = D2D1::RectF(curX, 6.0f, curX + ft.w, 32.0f);
            bool isAct = (current_view == ft.key);
            bool isHov = (hovered_id == std::string("tab:") + ft.key);
            if (!isAct && isHov) {
                FillRoundRect(tr, 5.0f, HexColor(0x141416));
            }
            DrawTextUtf8(ft.label, isAct ? tfBold8 : tfRegular8, tr, isAct ? COL_TEXT_MAIN : COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
            AddHit(std::string("tab:") + ft.key, tr);
            if (isAct) {
                tab_ind_target_x1 = tr.left + 4.0f;
                tab_ind_target_x2 = tr.right - 4.0f;
                tab_ind_color = ft.col;
            }
            curX += ft.w + 4.0f;
        }

        // Animated 2.5px Active Tab Underline Indicator
        FillRoundRect(D2D1::RectF(tab_ind_x1, 35.0f, tab_ind_x2, 37.5f), 1.2f, HexColor(tab_ind_color));

        // Top-Right Borderless Status Indicator + Live FPS Pill + Pin
        std::string statusTxt = "Syncing RPC...";
        D2D1_COLOR_F dotCol = COL_AMBER;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (quota_data.connected) {
                dotCol = COL_GREEN;
                double pPct = quota_data.has_primary ? quota_data.primary_model.remaining_pct : 100.0;
                double wPct = quota_data.weekly_remaining_pct;
                std::string tierShort = quota_data.tier;
                size_t pPos = tierShort.find("Google AI ");
                if (pPos != std::string::npos) tierShort.erase(pPos, 10);
                char sBuf[128];
                std::snprintf(sBuf, sizeof(sBuf), "%s \xC2\xB7 5h %.0f%% \xC2\xB7 Wk %.0f%%", tierShort.c_str(), pPct, wPct);
                statusTxt = sBuf;
            }
        }

        D2D1_RECT_F pinRect = D2D1::RectF(W - 34.0f, 8.0f, W - 10.0f, 30.0f);
        DrawTextUtf8("\xF0\x9F\x93\x8C", tfRegular8, pinRect, is_pinned ? COL_TEXT_MAIN : COL_TEXT_DIM, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("toggle_pin", pinRect);

        // Live 60 FPS Telemetry Badge
        D2D1_RECT_F fpsPill = D2D1::RectF(W - 92.0f, 10.0f, W - 38.0f, 28.0f);
        FillRoundRect(fpsPill, 4.0f, HexColor(0x141A17));
        StrokeRoundRect(fpsPill, 4.0f, HexColor(0x203A2B), 0.8f);
        DrawTextUtf8(std::to_string(current_fps) + " FPS", tfMonoBold7, fpsPill, COL_GREEN, DWRITE_TEXT_ALIGNMENT_CENTER);

        D2D1_RECT_F stRect = D2D1::RectF(W - 292.0f, 8.0f, W - 96.0f, 30.0f);
        brush->SetColor(dotCol);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(stRect.left + 10.0f, 19.0f), 3.2f, 3.2f), brush);
        DrawTextUtf8(statusTxt, tfRegular8, D2D1::RectF(stRect.left + 18.0f, 8.0f, stRect.right, 30.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_LEADING);
        AddHit("tab:quotas", stRect);

        // Bottom 1px border under title bar
        brush->SetColor(HexColor(0x1F1F1F));
        rt->DrawLine(D2D1::Point2F(0.0f, 38.0f), D2D1::Point2F(W, 38.0f), brush, 1.0f);
    }

    void RenderSidebar(const D2D1_RECT_F& r) {
        DrawCard(r, 8.0f, COL_SIDEBAR, COL_BORDER);

        DrawTextUtf8("EXPLORER", tfBold7, D2D1::RectF(r.left + 12.0f, r.top + 8.0f, r.left + 110.0f, r.top + 26.0f), COL_TEXT_MUTED);

        D2D1_RECT_F refBtn = D2D1::RectF(r.right - 46.0f, r.top + 8.0f, r.right - 28.0f, r.top + 26.0f);
        DrawTextUtf8("\xF0\x9F\x94\x84", tfRegular7, refBtn, COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("refresh_sidebar", refBtn);

        D2D1_RECT_F clsBtn = D2D1::RectF(r.right - 26.0f, r.top + 8.0f, r.right - 8.0f, r.top + 26.0f);
        DrawTextUtf8("\xE2\x9C\x95", tfRegular7, clsBtn, COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("toggle_sidebar", clsBtn);

        fs::path wp(workspace_dir);
        std::string rootName = "\xF0\x9F\x93\x82 " + wp.filename().string();
        DrawTextUtf8(rootName, tfBold8, D2D1::RectF(r.left + 12.0f, r.top + 32.0f, r.right - 10.0f, r.top + 52.0f), COL_TEXT_MAIN);

        float fy = r.top + 56.0f;
        for (const auto& fe : sidebar_files) {
            if (fy + 22.0f > r.bottom - 8.0f) break;
            D2D1_RECT_F rowR = D2D1::RectF(r.left + 8.0f, fy, r.right - 8.0f, fy + 21.0f);
            std::string hitId = "file:" + fe.name;
            if (hovered_id == hitId) {
                FillRoundRect(rowR, 4.0f, HexColor(0x1E1E22));
            }
            if (fe.is_dir) {
                DrawTextUtf8("\xE2\x96\xB8 \xF0\x9F\x93\x81 " + fe.name, tfRegular8, D2D1::RectF(rowR.left + 6.0f, fy, rowR.right, fy + 21.0f), COL_TEXT_MUTED);
            } else {
                D2D1_RECT_F bdgR = D2D1::RectF(rowR.left + 6.0f, fy + 3.0f, rowR.left + 34.0f, fy + 18.0f);
                FillRoundRect(bdgR, 3.0f, HexColor(0x1A1A1A));
                DrawTextUtf8(fe.ext_badge, tfMonoBold7, bdgR, HexColor(fe.badge_color), DWRITE_TEXT_ALIGNMENT_CENTER);
                DrawTextUtf8(fe.name, tfRegular8, D2D1::RectF(rowR.left + 40.0f, fy, rowR.right - 4.0f, fy + 21.0f), COL_TEXT_MAIN);
                AddHit(hitId, rowR);
            }
            fy += 22.0f;
        }
    }
