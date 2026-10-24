    // ========================================================================
    // View 1: Antigravity CLI View
    // ========================================================================
    void RenderCLIView(const D2D1_RECT_F& r) {
        auto& cli = cli_instances[current_view];
        fs::path wp(workspace_dir);
        std::string wsFolder = wp.filename().string();

        // 1. Sub-header Bar (0 .. 26px)
        float shY = r.top;
        std::string pathTxt = "v1.4.6   ~/" + wsFolder + "   git:(main)";
        DrawTextUtf8(pathTxt, tfMono8, D2D1::RectF(r.left + 2.0f, shY, r.left + 360.0f, shY + 24.0f), COL_TEXT_MUTED);

        // Right-aligned controls: [↗ Open Native TUI] [effort: low | medium | high | max] [Model ▾]
        D2D1_RECT_F tuiBtn = D2D1::RectF(r.right - 108.0f, shY + 1.0f, r.right, shY + 23.0f);
        DrawCard(tuiBtn, 5.0f, COL_CORAL_BG, COL_CORAL_BDR);
        DrawTextUtf8("\xE2\x86\x97 Open Native TUI", tfBold7, tuiBtn, COL_CORAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("native_term", tuiBtn);

        // Effort 4-Pill Chooser (Spring-Animated Sliding Pill!)
        D2D1_RECT_F effBox = D2D1::RectF(tuiBtn.left - 190.0f, shY + 1.0f, tuiBtn.left - 8.0f, shY + 23.0f);
        DrawCard(effBox, 5.0f, HexColor(0x161616), COL_BORDER);
        DrawTextUtf8("effort:", tfRegular7, D2D1::RectF(effBox.left + 6.0f, effBox.top, effBox.left + 40.0f, effBox.bottom), COL_TEXT_MUTED);

        float ex = effBox.left + 40.0f;
        D2D1_RECT_F cliActRect{};
        for (const std::string& eff : AVAILABLE_EFFORTS) {
            float pw = (eff == "medium") ? 44.0f : 31.0f;
            D2D1_RECT_F pr = D2D1::RectF(ex, effBox.top + 2.0f, ex + pw, effBox.bottom - 2.0f);
            if (cli.effort == eff) cliActRect = pr;
            ex += pw + 2.0f;
        }
        if (cliActRect.right > cliActRect.left) {
            UpdateAndDrawSlidingPill("cli_eff_" + current_view, cliActRect, 3.5f, HexColor(0x281F1B), COL_CORAL_BDR, COL_CORAL);
        }

        ex = effBox.left + 40.0f;
        for (const std::string& eff : AVAILABLE_EFFORTS) {
            float pw = (eff == "medium") ? 44.0f : 31.0f;
            D2D1_RECT_F pr = D2D1::RectF(ex, effBox.top + 2.0f, ex + pw, effBox.bottom - 2.0f);
            bool isAct = (cli.effort == eff);
            bool isHov = (hovered_id == "cli_effort:" + eff);
            if (!isAct && isHov) FillRoundRect(pr, 3.0f, HexColor(0x1E1E22));
            DrawTextUtf8(eff, isAct ? tfBold7 : tfRegular7, pr, isAct ? COL_CORAL : (isHov ? COL_TEXT_MAIN : COL_TEXT_MUTED), DWRITE_TEXT_ALIGNMENT_CENTER);
            AddHit("cli_effort:" + eff, pr);
            ex += pw + 2.0f;
        }

        // Model Selector Pill
        float modW = static_cast<float>(cli.model.size()) * 5.6f + 26.0f;
        D2D1_RECT_F modBtn = D2D1::RectF(effBox.left - modW - 8.0f, shY + 1.0f, effBox.left - 8.0f, shY + 23.0f);
        DrawCard(modBtn, 5.0f, COL_PILL_BG, COL_BORDER);
        DrawTextUtf8(cli.model + " \xE2\x96\xBE", tfBold7, modBtn, COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("cli_model_popup", modBtn);

        // 2. Coral Welcome Banner Card (Rounded r=10, height=136)
        D2D1_RECT_F bannerR = D2D1::RectF(r.left, r.top + 30.0f, r.right, r.top + 166.0f);
        DrawCard(bannerR, 10.0f, COL_CARD, COL_CORAL_BDR, 1.2f);

        float bLeft = bannerR.left + 18.0f;
        DrawTextUtf8("\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80 Antigravity CLI v1.4.6 \xE2\x94\x80\xE2\x94\x80\xE2\x94\x80", tfBold8,
                     D2D1::RectF(bLeft, bannerR.top + 12.0f, bLeft + 260.0f, bannerR.top + 30.0f), COL_CORAL);

        // Animated 8-bit Mascot
        float yOff = std::sin(mascot_tick * 0.12f) * 1.8f;
        bool blink = ((mascot_tick % 90) >= 85);
        DrawMascot(bLeft + 4.0f, bannerR.top + 42.0f, 3.2f, COL_CORAL, yOff, blink);

        float infoX = bLeft + 62.0f;
        DrawTextUtf8("Welcome back!", tfBold12, D2D1::RectF(infoX, bannerR.top + 38.0f, infoX + 260.0f, bannerR.top + 60.0f), COL_TEXT_MAIN);

        std::string tierName = "Google AI Ultra";
        std::string recentTxt = "\xE2\x80\xA2 Active workspace telemetry ready";
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            if (quota_data.connected) tierName = quota_data.tier;
            if (!dash_data.sessions_list.empty()) {
                const auto& s0 = dash_data.sessions_list[0];
                recentTxt = "\xE2\x80\xA2 Session " + s0.session_id.substr(0, 8) + " \xE2\x80\x94 " +
                            FormatWithCommas(s0.requests) + " requests \xC2\xB7 " + FormatTokens(s0.total_tokens) + " tokens";
            }
        }
        std::string modelSub = cli.model + " with " + cli.effort + " effort \xC2\xB7 " + tierName;
        DrawTextUtf8(modelSub, tfRegular8, D2D1::RectF(infoX, bannerR.top + 62.0f, bannerR.left + 440.0f, bannerR.top + 82.0f), COL_TEXT_MUTED);
        DrawTextUtf8("~/" + wsFolder, tfMono7, D2D1::RectF(infoX, bannerR.top + 84.0f, bannerR.left + 440.0f, bannerR.top + 102.0f), COL_TEXT_DIM);

        // Vertical Divider inside Banner
        float midBannerX = bannerR.left + (bannerR.right - bannerR.left) * 0.50f;
        brush->SetColor(HexColor(0x2A201C));
        rt->DrawLine(D2D1::Point2F(midBannerX, bannerR.top + 28.0f), D2D1::Point2F(midBannerX, bannerR.bottom - 16.0f), brush, 1.0f);

        float rightBannerX = midBannerX + 20.0f;
        DrawTextUtf8("Tips for getting started", tfBold8, D2D1::RectF(rightBannerX, bannerR.top + 32.0f, bannerR.right - 16.0f, bannerR.top + 50.0f), COL_CORAL);
        DrawTextUtf8("\xE2\x80\xA2 Run /usage, /weekly, /quota, or /help to inspect live limits\n\xE2\x80\xA2 Use /plan or \xF0\x9F\x8E\xAF Orchestrator to run parallel background workers",
                     tfRegular7, D2D1::RectF(rightBannerX, bannerR.top + 52.0f, bannerR.right - 16.0f, bannerR.top + 86.0f), COL_TEXT_MUTED);
        DrawTextUtf8("Recent activity", tfBold8, D2D1::RectF(rightBannerX, bannerR.top + 90.0f, bannerR.right - 16.0f, bannerR.top + 108.0f), COL_CORAL);
        DrawTextUtf8(recentTxt, tfRegular7, D2D1::RectF(rightBannerX, bannerR.top + 110.0f, bannerR.right - 16.0f, bannerR.top + 128.0f), COL_TEXT_MUTED);

        // 3. Structured Terminal Stream Area
        D2D1_RECT_F termR = D2D1::RectF(r.left, bannerR.bottom + 10.0f, r.right, r.bottom - 46.0f);
        rt->PushAxisAlignedClip(termR, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        float availH = termR.bottom - termR.top - 8.0f;
        float contentH = static_cast<float>(cli.lines.size()) * 19.0f;
        float maxScroll = std::max(0.0f, contentH - availH);
        cli.scroll_y = std::clamp(cli.scroll_y, 0.0f, maxScroll);

        float ty = termR.top + 4.0f - cli.scroll_y;
        for (const auto& tl : cli.lines) {
            if (ty + 20.0f >= termR.top && ty <= termR.bottom) {
                D2D1_COLOR_F col = COL_TEXT_MAIN;
                IDWriteTextFormat* fmt = tfMono8;
                if (tl.tag == "tool") col = HexColor(0x60A5FA);
                else if (tl.tag == "sub") col = COL_GREEN;
                else if (tl.tag == "cmd") col = COL_CORAL;
                else if (tl.tag == "info") col = COL_TEXT_MUTED;
                else if (tl.tag == "err") col = COL_RED;
                else if (tl.tag == "code") {
                    FillRoundRect(D2D1::RectF(termR.left + 12.0f, ty - 1.0f, termR.right - 18.0f, ty + 18.0f), 0.0f, HexColor(0x161616));
                    col = HexColor(0xE5E5E5);
                }
                DrawTextUtf8(tl.text, fmt, D2D1::RectF(termR.left + 8.0f, ty, termR.right - 14.0f, ty + 18.0f), col);
            }
            ty += 19.0f;
        }
        rt->PopAxisAlignedClip();

        // 4. Bottom Command Prompt Input Bar
        D2D1_RECT_F inpCard = D2D1::RectF(r.left, r.bottom - 38.0f, r.right, r.bottom);
        bool inpFocused = (active_input_field == "cli_prompt");
        DrawCard(inpCard, 8.0f, COL_CARD, inpFocused ? COL_CORAL_BDR : COL_BORDER);
        AddHit("focus:cli_prompt", inpCard);

        DrawTextUtf8("\xE2\x9D\xAF", tfBold10, D2D1::RectF(inpCard.left + 12.0f, inpCard.top, inpCard.left + 28.0f, inpCard.bottom), COL_CORAL);

        std::string dispPrompt = cli_prompt_input;
        D2D1_COLOR_F promptCol = COL_TEXT_MAIN;
        if (dispPrompt.empty() && !inpFocused) {
            dispPrompt = "Ask Antigravity CLI or type /usage, /weekly, /quota, /status, /effort, /help...";
            promptCol = COL_TEXT_DIM;
        } else if (inpFocused && ((mascot_tick / 18) % 2 == 0)) {
            dispPrompt += "|";
        }
        DrawTextUtf8(dispPrompt, tfMono8, D2D1::RectF(inpCard.left + 32.0f, inpCard.top, inpCard.right - 134.0f, inpCard.bottom), promptCol);

        // Stop & Run Buttons
        D2D1_RECT_F stopBtn = D2D1::RectF(inpCard.right - 60.0f, inpCard.top + 6.0f, inpCard.right - 8.0f, inpCard.bottom - 6.0f);
        FillRoundRect(stopBtn, 4.0f, COL_PILL_BG);
        DrawTextUtf8("\xE2\x8F\xB9 Stop", tfRegular7, stopBtn, COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("cli_stop", stopBtn);

        D2D1_RECT_F runBtn = D2D1::RectF(stopBtn.left - 62.0f, inpCard.top + 6.0f, stopBtn.left - 6.0f, inpCard.bottom - 6.0f);
        FillRoundRect(runBtn, 4.0f, COL_PILL_ACT);
        DrawTextUtf8("Run \xE2\x86\xB5", tfBold7, runBtn, COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("cli_run", runBtn);
    }

    // ========================================================================
    // View 2: Parallel Multi-Agent Orchestrator
    // ========================================================================
    void RenderOrchestratorView(const D2D1_RECT_F& r) {
        // Header Row
        DrawTextUtf8("Parallel Agent Orchestrator", tfBold12, D2D1::RectF(r.left, r.top, r.left + 340.0f, r.top + 22.0f), COL_TEXT_MAIN);
        DrawTextUtf8("Decompose goals, set per-worker effort, and dispatch parallel CLI agents.", tfRegular8,
                     D2D1::RectF(r.left, r.top + 22.0f, r.left + 440.0f, r.top + 40.0f), COL_TEXT_MUTED);

        // Top-Right Controls: [All Effort: low | medium | high | max] [▶ Dispatch All Parallel] [↗ Terminals]
        D2D1_RECT_F termBtn = D2D1::RectF(r.right - 88.0f, r.top + 4.0f, r.right, r.top + 30.0f);
        DrawCard(termBtn, 6.0f, COL_CARD, COL_BORDER);
        DrawTextUtf8("\xE2\x86\x97 Terminals", tfRegular8, termBtn, COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("native_term", termBtn);

        D2D1_RECT_F dispBtn = D2D1::RectF(termBtn.left - 148.0f, r.top + 4.0f, termBtn.left - 6.0f, r.top + 30.0f);
        DrawCard(dispBtn, 6.0f, COL_GREEN, COL_GREEN);
        DrawTextUtf8("\xE2\x96\xB6 Dispatch All Parallel", tfBold8, dispBtn, HexColor(0x0D0D0D), DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("orch_dispatch_all", dispBtn);

        D2D1_RECT_F allEffBox = D2D1::RectF(dispBtn.left - 216.0f, r.top + 4.0f, dispBtn.left - 8.0f, r.top + 30.0f);
        DrawCard(allEffBox, 6.0f, HexColor(0x161616), COL_BORDER);
        DrawTextUtf8("All Effort:", tfBold7, D2D1::RectF(allEffBox.left + 8.0f, allEffBox.top, allEffBox.left + 62.0f, allEffBox.bottom), COL_TEXT_MUTED);

        float gex = allEffBox.left + 62.0f;
        D2D1_RECT_F globActRect{};
        for (const std::string& eff : AVAILABLE_EFFORTS) {
            float pw = (eff == "medium") ? 46.0f : 31.0f;
            D2D1_RECT_F pr = D2D1::RectF(gex, allEffBox.top + 3.0f, gex + pw, allEffBox.bottom - 3.0f);
            if (default_effort == eff) globActRect = pr;
            gex += pw + 2.0f;
        }
        if (globActRect.right > globActRect.left) {
            UpdateAndDrawSlidingPill("orch_global_eff", globActRect, 3.5f, HexColor(0x281F1B), COL_CORAL_BDR, COL_CORAL);
        }

        gex = allEffBox.left + 62.0f;
        for (const std::string& eff : AVAILABLE_EFFORTS) {
            float pw = (eff == "medium") ? 46.0f : 31.0f;
            D2D1_RECT_F pr = D2D1::RectF(gex, allEffBox.top + 3.0f, gex + pw, allEffBox.bottom - 3.0f);
            bool isAct = (default_effort == eff);
            bool isHov = (hovered_id == "orch_global_eff:" + eff);
            if (!isAct && isHov) FillRoundRect(pr, 3.0f, HexColor(0x1E1E22));
            DrawTextUtf8(eff, isAct ? tfBold7 : tfRegular7, pr, isAct ? COL_CORAL : (isHov ? COL_TEXT_MAIN : COL_TEXT_MUTED), DWRITE_TEXT_ALIGNMENT_CENTER);
            AddHit("orch_global_eff:" + eff, pr);
            gex += pw + 2.0f;
        }

        // Master Goal Card
        D2D1_RECT_F goalCard = D2D1::RectF(r.left, r.top + 48.0f, r.right, r.top + 122.0f);
        DrawCard(goalCard, 10.0f, COL_CARD, COL_BORDER);
        DrawTextUtf8("PROJECT MASTER GOAL", tfBold7, D2D1::RectF(goalCard.left + 14.0f, goalCard.top + 10.0f, goalCard.left + 220.0f, goalCard.top + 26.0f), COL_CORAL);

        D2D1_RECT_F splitBtn = D2D1::RectF(goalCard.right - 134.0f, goalCard.top + 32.0f, goalCard.right - 14.0f, goalCard.bottom - 12.0f);
        DrawCard(splitBtn, 6.0f, COL_CORAL_BG, COL_CORAL_BDR);
        DrawTextUtf8("\xE2\x9A\xA1 Auto-Split Goal", tfBold8, splitBtn, COL_CORAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("orch_split_goal", splitBtn);

        D2D1_RECT_F goalInputR = D2D1::RectF(goalCard.left + 14.0f, goalCard.top + 34.0f, splitBtn.left - 10.0f, goalCard.bottom - 12.0f);
        FillRoundRect(goalInputR, 4.0f, COL_CARD_HDR);
        AddHit("focus:master_goal", goalInputR);
        std::string gTxt = master_goal;
        if (active_input_field == "master_goal" && ((mascot_tick / 18) % 2 == 0)) gTxt += "|";
        DrawTextUtf8(gTxt, tfRegular8, D2D1::RectF(goalInputR.left + 8.0f, goalInputR.top, goalInputR.right - 8.0f, goalInputR.bottom), COL_TEXT_MAIN);

        // 2x2 Worker Grid with wider 22px Telemetry Bus Alleys for Folder/Artifact Transfer Lines
        float gridTop = goalCard.bottom + 18.0f;
        float gridW = r.right - r.left;
        float gridH = r.bottom - gridTop;
        float gapX = 22.0f;
        float gapY = 20.0f;
        float cardW = (gridW - gapX) * 0.5f;
        float cardH = (gridH - gapY) * 0.5f;

        bool anyWorkerRunning = false;
        for (const auto& w : workers) if (w.is_running) anyWorkerRunning = true;

        D2D1_RECT_F cardRects[4];
        for (int i = 0; i < 4; ++i) {
            int row = i / 2;
            int col = i % 2;
            float cx = r.left + col * (cardW + gapX);
            float cy = gridTop + row * (cardH + gapY);
            cardRects[i] = D2D1::RectF(cx, cy, cx + cardW, cy + cardH);
        }

        // Precompute anchor ports for Master Goal (-1), Central Router Hub, and Workers 0..3
        float hubX = r.left + cardW + gapX * 0.5f;
        float hubY = gridTop + cardH + gapY * 0.5f;

        auto getNodePort = [&](int nodeIdx, bool isSource) -> D2D1_POINT_2F {
            if (nodeIdx < 0) {
                return D2D1::Point2F(hubX, goalCard.bottom);
            }
            const auto& cr = cardRects[std::clamp(nodeIdx, 0, 3)];
            float mx = (cr.left + cr.right) * 0.5f;
            float my = (cr.top + cr.bottom) * 0.5f;
            // Connect along inner alley edges facing the central bus
            if (nodeIdx == 0) return isSource ? D2D1::Point2F(cr.right, my - 10.0f) : D2D1::Point2F(mx + 40.0f, cr.top);
            if (nodeIdx == 1) return isSource ? D2D1::Point2F(cr.left, my + 10.0f)  : D2D1::Point2F(cr.left, my - 10.0f);
            if (nodeIdx == 2) return isSource ? D2D1::Point2F(cr.right, my - 10.0f) : D2D1::Point2F(mx, cr.top);
            return isSource ? D2D1::Point2F(cr.left, my + 10.0f) : D2D1::Point2F(cr.left, my - 10.0f);
        };

        // Draw Background Inter-Worker Circuit Tracks in the alleys BEFORE cards
        D2D1_COLOR_F trackCol = anyWorkerRunning ? HexColor(0x3D2922) : HexColor(0x222228);
        brush->SetColor(trackCol);
        // Vertical trunk from Master Goal down through Central Hub
        rt->DrawLine(D2D1::Point2F(hubX, goalCard.bottom), D2D1::Point2F(hubX, r.bottom - 12.0f), brush, 1.5f, dashedStroke);
        // Horizontal trunk across Central Hub alley
        rt->DrawLine(D2D1::Point2F(r.left + 24.0f, hubY), D2D1::Point2F(r.right - 24.0f, hubY), brush, 1.5f, dashedStroke);

        for (int i = 0; i < 4; ++i) {
            D2D1_RECT_F wc = cardRects[i];
            auto& w = workers[i];

            // Glowing active border when worker is running or receiving folder packets
            D2D1_COLOR_F bdrCol = w.is_running ? COL_CORAL_BDR : ((w.progress_fill >= 1.0f) ? HexColor(0x234734) : COL_BORDER);
            DrawCard(wc, 10.0f, COL_CARD, bdrCol, w.is_running ? 1.5f : 1.0f);

            float inL = wc.left + 14.0f;
            float inR = wc.right - 14.0f;

            // Worker Header
            DrawTextUtf8(w.title, tfBold8, D2D1::RectF(inL, wc.top + 10.0f, inR - 90.0f, wc.top + 28.0f), COL_TEXT_MAIN);

            D2D1_RECT_F stPill = D2D1::RectF(inR - 96.0f, wc.top + 10.0f, inR, wc.top + 28.0f);
            FillRoundRect(stPill, 4.0f, w.is_running ? HexColor(0x261C14) : COL_PILL_BG);
            DrawTextUtf8(w.status_text, tfBold7, stPill, HexColor(w.status_color), DWRITE_TEXT_ALIGNMENT_CENTER);

            // Model Selector + Per-Worker Effort Pills
            float cfgY = wc.top + 33.0f;
            std::string mText = "Model: " + w.model + " \xE2\x96\xBE";
            float mPillW = std::min(220.0f, static_cast<float>(mText.size()) * 5.2f + 14.0f);
            D2D1_RECT_F mPill = D2D1::RectF(inL, cfgY, inL + mPillW, cfgY + 19.0f);
            FillRoundRect(mPill, 4.0f, HexColor(0x1A1F29));
            DrawTextUtf8(mText, tfMonoBold7, mPill, COL_BLUE, DWRITE_TEXT_ALIGNMENT_CENTER);
            AddHit("worker_model:" + std::to_string(i), mPill);

            D2D1_RECT_F weBox = D2D1::RectF(inR - 158.0f, cfgY, inR, cfgY + 19.0f);
            DrawCard(weBox, 4.0f, HexColor(0x181818), COL_BORDER);
            DrawTextUtf8("effort:", tfRegular7, D2D1::RectF(weBox.left + 4.0f, weBox.top, weBox.left + 34.0f, weBox.bottom), COL_TEXT_MUTED);
            float wex = weBox.left + 34.0f;
            D2D1_RECT_F wActRect{};
            for (const std::string& eff : AVAILABLE_EFFORTS) {
                float pw = (eff == "medium") ? 38.0f : 26.0f;
                D2D1_RECT_F pr = D2D1::RectF(wex, weBox.top + 2.0f, wex + pw, weBox.bottom - 2.0f);
                if (w.effort == eff) wActRect = pr;
                wex += pw + 1.5f;
            }
            if (wActRect.right > wActRect.left) {
                UpdateAndDrawSlidingPill("worker_eff_" + std::to_string(i), wActRect, 3.0f, HexColor(0x281F1B), COL_CORAL_BDR, COL_CORAL);
            }

            wex = weBox.left + 34.0f;
            for (const std::string& eff : AVAILABLE_EFFORTS) {
                float pw = (eff == "medium") ? 38.0f : 26.0f;
                D2D1_RECT_F pr = D2D1::RectF(wex, weBox.top + 2.0f, wex + pw, weBox.bottom - 2.0f);
                bool isAct = (w.effort == eff);
                std::string hitKey = "worker_eff:" + std::to_string(i) + ":" + eff;
                bool isHov = (hovered_id == hitKey);
                if (!isAct && isHov) FillRoundRect(pr, 2.5f, HexColor(0x1E1E22));
                DrawTextUtf8(eff, isAct ? tfBold7 : tfRegular7, pr, isAct ? COL_CORAL : (isHov ? COL_TEXT_MAIN : COL_TEXT_MUTED), DWRITE_TEXT_ALIGNMENT_CENTER);
                AddHit(hitKey, pr);
                wex += pw + 1.5f;
            }

            // 4px Animated Progress & Folder Carrier Track inside each card
            D2D1_RECT_F pbarTrack = D2D1::RectF(inL, cfgY + 23.0f, inR, cfgY + 27.0f);
            FillRoundRect(pbarTrack, 2.0f, HexColor(0x1E1E1E));
            float trackW = inR - inL;
            if (w.is_running) {
                float segW = trackW * 0.35f;
                float x1 = inL + (trackW + segW) * w.anim_phase - segW;
                float x2 = x1 + segW;
                float cx1 = std::max(inL, x1);
                float cx2 = std::min(inR, x2);
                if (cx2 > cx1) {
                    FillRoundRect(D2D1::RectF(cx1, pbarTrack.top, cx2, pbarTrack.bottom), 2.0f, COL_CORAL);
                    // Mini folder carrier gliding along the worker's own progress bar!
                    float carrierX = std::clamp(cx2 - 8.0f, inL + 10.0f, inR - 14.0f);
                    DrawTextUtf8("\xF0\x9F\x93\x81", tfRegular7, D2D1::RectF(carrierX - 8.0f, pbarTrack.top - 7.0f, carrierX + 12.0f, pbarTrack.bottom + 7.0f), COL_CORAL);
                }
            } else if (w.progress_fill > 0.0f) {
                FillRoundRect(D2D1::RectF(inL, pbarTrack.top, inL + trackW * w.progress_fill, pbarTrack.bottom), 2.0f, COL_GREEN);
            }

            // Prompt Entry Box
            D2D1_RECT_F prBox = D2D1::RectF(inL, cfgY + 31.0f, inR, cfgY + 52.0f);
            FillRoundRect(prBox, 4.0f, COL_CARD_HDR);
            AddHit("focus:worker_" + std::to_string(i), prBox);
            std::string wpTxt = w.prompt;
            if (active_input_field == ("worker_" + std::to_string(i)) && ((mascot_tick / 18) % 2 == 0)) wpTxt += "|";
            DrawTextUtf8(wpTxt, tfRegular8, D2D1::RectF(prBox.left + 6.0f, prBox.top, prBox.right - 6.0f, prBox.bottom), COL_TEXT_MAIN);

            // Live Console Stream Preview Box
            D2D1_RECT_F logBox = D2D1::RectF(inL, cfgY + 57.0f, inR, wc.bottom - 34.0f);
            FillRoundRect(logBox, 5.0f, HexColor(0x0C0C0C));
            float ly = logBox.top + 5.0f;
            for (const auto& lstr : w.logs) {
                if (ly + 15.0f > logBox.bottom) break;
                D2D1_COLOR_F lCol = HexColor(0xA0A0A0);
                if (lstr.find("\xF0\x9F\x93\x81") != std::string::npos || lstr.find("\xF0\x9F\x93\xA6") != std::string::npos) lCol = COL_CORAL;
                else if (lstr.find("\xE2\x9C\x93") != std::string::npos) lCol = COL_GREEN;
                DrawTextUtf8(lstr, tfMono7, D2D1::RectF(logBox.left + 8.0f, ly, logBox.right - 8.0f, ly + 15.0f), lCol);
                ly += 15.0f;
            }

            // Bottom Actions Row
            D2D1_RECT_F runWBtn = D2D1::RectF(inL, wc.bottom - 28.0f, inL + 54.0f, wc.bottom - 8.0f);
            FillRoundRect(runWBtn, 4.0f, COL_PILL_ACT);
            DrawTextUtf8("\xE2\x96\xB6 Run", tfBold7, runWBtn, COL_GREEN, DWRITE_TEXT_ALIGNMENT_CENTER);
            AddHit("worker_run:" + std::to_string(i), runWBtn);

            D2D1_RECT_F openWBtn = D2D1::RectF(runWBtn.right + 6.0f, wc.bottom - 28.0f, runWBtn.right + 106.0f, wc.bottom - 8.0f);
            FillRoundRect(openWBtn, 4.0f, COL_PILL_BG);
            DrawTextUtf8("\xF0\x9F\x94\x8D Open in CLI Tab", tfRegular7, openWBtn, COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
            AddHit("worker_open:" + std::to_string(i), openWBtn);
        }

        // ====================================================================
        // OVERLAY: Central Sync Hub + Clean Alley-Routed Folder Packet Carriers
        // ====================================================================
        // 1. Central Router Node at intersection of the 4 workers (hubX, hubY)
        float pulseR = 11.0f + std::sin(hub_pulse_phase * 6.28318f) * (anyWorkerRunning ? 4.5f : 1.8f);
        brush->SetColor(HexColor(anyWorkerRunning ? 0xD97757 : 0x38C77B, anyWorkerRunning ? 0.22f : 0.10f));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hubX, hubY), pulseR, pulseR), brush);

        brush->SetColor(HexColor(0x16161A));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hubX, hubY), 8.5f, 8.5f), brush);
        brush->SetColor(anyWorkerRunning ? COL_CORAL : COL_BORDER);
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(hubX, hubY), 8.5f, 8.5f), brush, 1.4f);
        DrawTextUtf8("\xE2\x9C\xBB", tfBold7, D2D1::RectF(hubX - 8.0f, hubY - 8.0f, hubX + 8.0f, hubY + 8.0f),
                     anyWorkerRunning ? COL_CORAL : COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);

        // 2. Render each active TransferPacket strictly along the inner card alley borders (never crossing card text!)
        for (const auto& pkt : transfer_packets) {
            D2D1_POINT_2F pStart = getNodePort(pkt.from_node, true);
            D2D1_POINT_2F pEnd   = getNodePort(pkt.to_node, false);
            D2D1_POINT_2F pMid   = D2D1::Point2F(hubX, (pkt.from_node < 0) ? (gridTop - 9.0f) : hubY);

            // Draw active glowing connection line along the alley between source port -> hub -> target port
            D2D1_COLOR_F lineCol = HexColor(pkt.color, 0.55f);
            brush->SetColor(lineCol);
            rt->DrawLine(pStart, pMid, brush, 1.6f, dashedStroke);
            rt->DrawLine(pMid, pEnd, brush, 1.6f, dashedStroke);

            // Endpoint terminal sockets on the card border edges
            brush->SetColor(HexColor(pkt.color, 0.90f));
            rt->FillEllipse(D2D1::Ellipse(pStart, 3.2f, 3.2f), brush);
            rt->FillEllipse(D2D1::Ellipse(pEnd, 3.6f, 3.6f), brush);

            // Piecewise 2-segment alley routing (0..0.5: pStart -> pMid, 0.5..1.0: pMid -> pEnd)
            float t = std::clamp(pkt.progress, 0.0f, 1.0f);
            float et = t * t * (3.0f - 2.0f * t);

            auto evalAlleyPath = [&](float u) -> D2D1_POINT_2F {
                if (u < 0.5f) {
                    float s = u * 2.0f;
                    return D2D1::Point2F(pStart.x + (pMid.x - pStart.x) * s, pStart.y + (pMid.y - pStart.y) * s);
                } else {
                    float s = (u - 0.5f) * 2.0f;
                    return D2D1::Point2F(pMid.x + (pEnd.x - pMid.x) * s, pMid.y + (pEnd.y - pMid.y) * s);
                }
            };

            D2D1_POINT_2F curPt = evalAlleyPath(et);

            // Comet tail particles behind the folder carrier
            for (int tail = 1; tail <= 3; ++tail) {
                float tt = std::max(0.0f, et - tail * 0.045f);
                D2D1_POINT_2F tPt = evalAlleyPath(tt);
                brush->SetColor(HexColor(pkt.color, 0.45f / tail));
                rt->FillEllipse(D2D1::Ellipse(tPt, 3.8f - tail * 0.8f, 3.8f - tail * 0.8f), brush);
            }

            // Glowing Folder Carrier Pill Badge ("📁 src/core" etc.)
            std::string badgeTxt = pkt.icon + " " + pkt.label;
            float bw = static_cast<float>(pkt.label.size()) * 5.2f + 28.0f;
            float bh = 18.0f;
            D2D1_RECT_F pillR = D2D1::RectF(curPt.x - bw * 0.5f, curPt.y - bh * 0.5f, curPt.x + bw * 0.5f, curPt.y + bh * 0.5f);

            FillRoundRect(D2D1::RectF(pillR.left - 2.0f, pillR.top - 2.0f, pillR.right + 2.0f, pillR.bottom + 2.0f), 6.0f, HexColor(pkt.color, 0.22f));
            DrawCard(pillR, 5.0f, HexColor(0x141418), HexColor(pkt.color, 0.95f), 1.2f);
            DrawTextUtf8(badgeTxt, tfMonoBold7, pillR, COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_CENTER);
        }
    }
