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
    std::string master_goal = "Build full-stack feature, write unit tests, audit security, and update release docs in parallel";
    std::string cli_prompt_input;
    std::string session_search_query;
    std::string active_input_field = "cli_prompt"; // "cli_prompt", "master_goal", "session_search", "worker_0".."worker_3"

    // Thread-safe telemetry state
    std::mutex data_mutex;
    QuotaData quota_data;
    DashboardData dash_data;
    std::atomic<bool> running{true};
    std::atomic<bool> force_poll{false};
    std::thread poll_thread;

    // 60fps Motion Graphics State
    float tab_ind_x1 = 92.0f;
    float tab_ind_x2 = 210.0f;
    float tab_ind_target_x1 = 92.0f;
    float tab_ind_target_x2 = 210.0f;
    uint32_t tab_ind_color = 0xD97757;

    float chart_anim = 1.0f;
    float quotas_anim = 1.0f;
    double kpi_disp_cost = 0.0;
    double kpi_disp_reqs = 0.0;
    double kpi_disp_toks = 0.0;

    int mascot_tick = 0;
    int spinner_idx = 0;
    float quotas_scroll_y = 0.0f;
    float sessions_scroll_y = 0.0f;

    // Interactive Hit Regions & Hover
    std::vector<ClickRegion> hit_regions;
    std::string hovered_id;
    int hovered_bar_idx = -1;
    int hovered_heat_w = -1;
    int hovered_heat_d = -1;
    POINT last_mouse{0, 0};

    // Popup dropdown menu state
    bool popup_open = false;
    std::string popup_target; // "cli" or "worker_0".."worker_3"
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
            {0, "Worker #1 \xE2\x80\x94 Architecture & Core", "Gemini 4 Argon (High)", "high",
             "Implement high-level architecture and single-header abstractions", "\xE2\x97\x8F Idle", 0x8C8C8C, false, 0.0f, 0.0f,
             {"Ready to dispatch. Click '\xE2\x96\xB6 Run' or '\xE2\x96\xB6 Dispatch All Parallel'."}},
            {1, "Worker #2 \xE2\x80\x94 UI & Styling", "Gemini 3.8 Flash (Medium)", "medium",
             "Refine frameless desktop window, dark theme palette, and smooth canvas", "\xE2\x97\x8F Idle", 0x8C8C8C, false, 0.25f, 0.0f,
             {"Ready to dispatch. Click '\xE2\x96\xB6 Run' or '\xE2\x96\xB6 Dispatch All Parallel'."}},
            {2, "Worker #3 \xE2\x80\x94 Verification & Tests", "Gemini 3.8 Flash (Medium)", "medium",
             "Run automated smoke test suite across all views, ranges, and resize events", "\xE2\x97\x8F Idle", 0x8C8C8C, false, 0.5f, 0.0f,
             {"Ready to dispatch. Click '\xE2\x96\xB6 Run' or '\xE2\x96\xB6 Dispatch All Parallel'."}},
            {3, "Worker #4 \xE2\x80\x94 Docs & Release", "Claude Sonnet 5.5 (High)", "high",
             "Generate documentation, capture crisp PNG previews and build standalone binary", "\xE2\x97\x8F Idle", 0x8C8C8C, false, 0.75f, 0.0f,
             {"Ready to dispatch. Click '\xE2\x96\xB6 Run' or '\xE2\x96\xB6 Dispatch All Parallel'."}}
        };
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
            if (SUCCEEDED(d2dFactory->CreateHwndRenderTarget(props, D2D1::HwndRenderTargetProperties(hwnd, size), &hwndRt))) {
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

    void StartPolling() {
        // Initial synchronous poll for instant data
        PollOnce();
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            kpi_disp_cost = dash_data.total_cost;
            kpi_disp_reqs = dash_data.total_requests;
            kpi_disp_toks = static_cast<double>(dash_data.total_tokens);
        }
        poll_thread = std::thread([this]() {
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
        DashboardData d = AntigravityDataCollector::GetDashboardData(selected_range, q);
        std::lock_guard<std::mutex> lk(data_mutex);
        quota_data = std::move(q);
        dash_data = std::move(d);
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
    // 60fps Animation Tick
    // ========================================================================
    void StepAnimations() {
        mascot_tick++;
        if (mascot_tick % 4 == 0) spinner_idx = (spinner_idx + 1) % 10;

        // Smooth tab indicator slide
        tab_ind_x1 += (tab_ind_target_x1 - tab_ind_x1) * 0.32f;
        tab_ind_x2 += (tab_ind_target_x2 - tab_ind_x2) * 0.32f;

        // Smooth chart & quota bar easing
        if (chart_anim < 1.0f) {
            chart_anim = std::min(1.0f, chart_anim + 0.08f);
        }
        if (quotas_anim < 1.0f) {
            quotas_anim = std::min(1.0f, quotas_anim + 0.075f);
        }

        // Smooth KPI counter interpolation
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            kpi_disp_cost += (dash_data.total_cost - kpi_disp_cost) * 0.25;
            kpi_disp_reqs += (dash_data.total_requests - kpi_disp_reqs) * 0.25;
            kpi_disp_toks += (static_cast<double>(dash_data.total_tokens) - kpi_disp_toks) * 0.25;
        }

        // Running worker wave animation
        static const char* kSpinners[] = {"\xE2\xA0\x8B", "\xE2\xA0\x99", "\xE2\xA0\xB9", "\xE2\xA0\xB8", "\xE2\xA0\xBC", "\xE2\xA0\xB4", "\xE2\xA0\xA6", "\xE2\xA0\xA7", "\xE2\xA0\x87", "\xE2\xA0\x8F"};
        for (auto& w : workers) {
            if (w.is_running) {
                w.anim_phase = std::fmod(w.anim_phase + 0.035f, 1.0f);
                w.status_text = std::string(kSpinners[spinner_idx]) + " Running (" + w.effort + ")";
                w.status_color = 0xF59E0B;
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

        D2D1_RECT_F viewRect = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + contentH);
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

        // Two passes so tab_ind_target_x1 snaps cleanly
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

        // Encode WICBitmap to PNG file
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
            brush->SetColor(HexColor(l.col));
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(l.cx, ty), 5.8f, 5.8f), brush);
            AddHit(l.id, D2D1::RectF(l.cx - 8.0f, ty - 8.0f, l.cx + 8.0f, ty + 8.0f));
        }

        // Sidebar toggle button
        D2D1_RECT_F sbBtn = D2D1::RectF(72.0f, 8.0f, 94.0f, 30.0f);
        DrawTextUtf8("\xE2\x97\xAB", tfBold10, sbBtn, show_sidebar ? COL_TEXT_MAIN : COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("toggle_sidebar", sbBtn);

        // Tabs Strip
        float curX = 102.0f;
        for (const std::string& tid : cli_order) {
            auto& cli = cli_instances[tid];
            float tw = static_cast<float>(cli.name.size()) * 6.0f + 22.0f;
            D2D1_RECT_F tr = D2D1::RectF(curX, 6.0f, curX + tw, 32.0f);
            bool isAct = (current_view == tid);
            bool isHov = (hovered_id == "tab:" + tid);
            if (isAct || isHov) {
                FillRoundRect(tr, 5.0f, isAct ? HexColor(0x1A1A1A) : HexColor(0x141414));
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

        // Fixed Studio Tabs
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
            if (isAct || isHov) {
                FillRoundRect(tr, 5.0f, isAct ? HexColor(0x1A1A1A) : HexColor(0x141414));
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

        // Animated 2px Active Tab Underline Indicator
        FillRoundRect(D2D1::RectF(tab_ind_x1, 35.0f, tab_ind_x2, 37.0f), 1.0f, HexColor(tab_ind_color));

        // Top-Right Borderless Status Indicator + Pin
        std::string statusTxt = "Connecting RPC...";
        D2D1_COLOR_F dotCol = COL_AMBER;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (quota_data.connected) {
                dotCol = COL_GREEN;
                double pPct = quota_data.has_primary ? quota_data.primary_model.remaining_pct : 100.0;
                double sPct = quota_data.has_secondary ? quota_data.secondary_model.remaining_pct : 100.0;
                std::string tierShort = quota_data.tier;
                size_t pPos = tierShort.find("Google AI ");
                if (pPos != std::string::npos) tierShort.erase(pPos, 10);
                char sBuf[128];
                std::snprintf(sBuf, sizeof(sBuf), "%s \xC2\xB7 Gem %.0f%% \xC2\xB7 Claude %.0f%%", tierShort.c_str(), pPct, sPct);
                statusTxt = sBuf;
            }
        }

        D2D1_RECT_F pinRect = D2D1::RectF(W - 34.0f, 8.0f, W - 10.0f, 30.0f);
        DrawTextUtf8("\xF0\x9F\x93\x8C", tfRegular8, pinRect, is_pinned ? COL_TEXT_MAIN : COL_TEXT_DIM, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("toggle_pin", pinRect);

        D2D1_RECT_F stRect = D2D1::RectF(W - 240.0f, 8.0f, W - 38.0f, 30.0f);
        brush->SetColor(dotCol);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(stRect.left + 12.0f, 19.0f), 3.2f, 3.2f), brush);
        DrawTextUtf8(statusTxt, tfRegular8, D2D1::RectF(stRect.left + 22.0f, 8.0f, stRect.right, 30.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_LEADING);
        AddHit("tab:quotas", stRect);

        // Bottom 1px border under title bar
        brush->SetColor(HexColor(0x1F1F1F));
        rt->DrawLine(D2D1::Point2F(0.0f, 38.0f), D2D1::Point2F(W, 38.0f), brush, 1.0f);
    }

    void RenderSidebar(const D2D1_RECT_F& r) {
        DrawCard(r, 8.0f, COL_SIDEBAR, COL_BORDER);

        DrawTextUtf8("EXPLORER", tfBold7, D2D1::RectF(r.left + 12.0f, r.top + 8.0f, r.left + 110.0f, r.top + 26.0f), COL_TEXT_MUTED);

        // Refresh & Close icons
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
                FillRoundRect(rowR, 4.0f, HexColor(0x1C1C1C));
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
