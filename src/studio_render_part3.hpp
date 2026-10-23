    // ========================================================================
    // View 3: Usage Analytics Dashboard
    // ========================================================================
    void RenderUsageDashboardView(const D2D1_RECT_F& r) {
        DashboardData d;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            d = dash_data;
        }

        // 1. Header Row + Range Selector Pills + Export Button
        DrawTextUtf8("Usage Analytics", tfBold12, D2D1::RectF(r.left, r.top, r.left + 260.0f, r.top + 22.0f), COL_TEXT_MAIN);
        DrawTextUtf8("Real workspace token accounting, cache efficiency, and API cost.", tfRegular8,
                     D2D1::RectF(r.left, r.top + 22.0f, r.left + 440.0f, r.top + 40.0f), COL_TEXT_MUTED);

        D2D1_RECT_F expBtn = D2D1::RectF(r.right - 68.0f, r.top + 4.0f, r.right, r.top + 30.0f);
        DrawCard(expBtn, 6.0f, COL_CARD, COL_BORDER);
        DrawTextUtf8("\xE2\xAC\x87 Export", tfBold7, expBtn, COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_CENTER);
        AddHit("export_report", expBtn);

        struct RangeItem { const char* key; const char* lbl; float w; } ranges[] = {
            {"today_active", "Today",    48.0f},
            {"today_24h",    "24h",      38.0f},
            {"7d",           "7 Days",   52.0f},
            {"30d",          "30 Days",  56.0f},
            {"all",          "All Time", 58.0f}
        };
        float totalRangeW = 8.0f;
        for (const auto& rg : ranges) totalRangeW += rg.w + 2.0f;

        D2D1_RECT_F rangeBox = D2D1::RectF(expBtn.left - totalRangeW - 8.0f, r.top + 4.0f, expBtn.left - 8.0f, r.top + 30.0f);
        DrawCard(rangeBox, 6.0f, COL_CARD, COL_BORDER);

        float rx = rangeBox.left + 4.0f;
        for (const auto& rg : ranges) {
            D2D1_RECT_F rr = D2D1::RectF(rx, rangeBox.top + 3.0f, rx + rg.w, rangeBox.bottom - 3.0f);
            bool isAct = (selected_range == rg.key);
            if (isAct) FillRoundRect(rr, 4.0f, COL_PILL_ACT);
            DrawTextUtf8(rg.lbl, isAct ? tfBold7 : tfRegular7, rr, isAct ? COL_TEXT_MAIN : COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
            AddHit(std::string("range:") + rg.key, rr);
            rx += rg.w + 2.0f;
        }

        // 2. 3 KPI Cards Row (Height 92px)
        float kpiTop = r.top + 48.0f;
        float kpiW = (r.right - r.left - 24.0f) / 3.0f;
        float kpiH = 92.0f;

        // KPI 1: Total Cost
        D2D1_RECT_F k1 = D2D1::RectF(r.left, kpiTop, r.left + kpiW, kpiTop + kpiH);
        DrawCard(k1, 10.0f, COL_CARD, COL_BORDER);
        DrawTextUtf8("Total cost", tfRegular8, D2D1::RectF(k1.left + 16.0f, k1.top + 10.0f, k1.right - 16.0f, k1.top + 28.0f), COL_TEXT_MUTED);
        DrawTextUtf8(FormatCost(kpi_disp_cost), tfBold18, D2D1::RectF(k1.left + 16.0f, k1.top + 30.0f, k1.right - 16.0f, k1.top + 64.0f), COL_TEXT_MAIN);
        DrawTextUtf8("API-Equivalent Burn", tfRegular7, D2D1::RectF(k1.left + 16.0f, k1.top + 66.0f, k1.right - 16.0f, k1.bottom - 8.0f), COL_TEXT_DIM);

        // KPI 2: Requests
        D2D1_RECT_F k2 = D2D1::RectF(k1.right + 12.0f, kpiTop, k1.right + 12.0f + kpiW, kpiTop + kpiH);
        DrawCard(k2, 10.0f, COL_CARD, COL_BORDER);
        DrawTextUtf8("Requests", tfRegular8, D2D1::RectF(k2.left + 16.0f, k2.top + 10.0f, k2.right - 16.0f, k2.top + 28.0f), COL_TEXT_MUTED);
        DrawTextUtf8(FormatWithCommas(static_cast<long long>(std::round(kpi_disp_reqs))), tfBold18,
                     D2D1::RectF(k2.left + 16.0f, k2.top + 30.0f, k2.right - 16.0f, k2.top + 64.0f), COL_TEXT_MAIN);
        char chBuf[64];
        std::snprintf(chBuf, sizeof(chBuf), "Cache Hit: %.1f%%", d.cache_hit_pct);
        DrawTextUtf8(chBuf, tfRegular7, D2D1::RectF(k2.left + 16.0f, k2.top + 66.0f, k2.right - 16.0f, k2.bottom - 8.0f), COL_GREEN);

        // KPI 3: Tokens
        D2D1_RECT_F k3 = D2D1::RectF(k2.right + 12.0f, kpiTop, r.right, kpiTop + kpiH);
        DrawCard(k3, 10.0f, COL_CARD, COL_BORDER);
        DrawTextUtf8("Tokens", tfRegular8, D2D1::RectF(k3.left + 16.0f, k3.top + 10.0f, k3.right - 16.0f, k3.top + 28.0f), COL_TEXT_MUTED);
        DrawTextUtf8(FormatTokens(static_cast<long long>(std::round(kpi_disp_toks))), tfBold18,
                     D2D1::RectF(k3.left + 16.0f, k3.top + 30.0f, k3.right - 16.0f, k3.top + 64.0f), COL_TEXT_MAIN);
        std::string tokSub = "In: " + FormatTokens(d.total_input) + " \xC2\xB7 Cache: " + FormatTokens(d.total_cache) + " \xC2\xB7 Out: " + FormatTokens(d.total_output);
        DrawTextUtf8(tokSub, tfRegular7, D2D1::RectF(k3.left + 16.0f, k3.top + 66.0f, k3.right - 16.0f, k3.bottom - 8.0f), COL_TEXT_MUTED);

        // 3. Chart Card (Height 212px)
        D2D1_RECT_F chartCard = D2D1::RectF(r.left, k1.bottom + 12.0f, r.right, k1.bottom + 224.0f);
        DrawCard(chartCard, 10.0f, COL_CARD, COL_BORDER);

        std::string cTitle = (chart_mode == "cost") ? "Hourly Cost" : ((chart_mode == "tokens") ? "Hourly Tokens" : "Activity Heatmap");
        std::string cSub = (chart_mode == "cost") ? "Model and Web Search costs for the selected range."
                         : ((chart_mode == "tokens") ? "Rolling hourly token consumption across workspace sessions."
                         : "52-week contribution grid with active streak and daily token intensity.");
        D2D1_COLOR_F subCol = COL_TEXT_MUTED;

        if (chart_mode != "heatmap" && hovered_bar_idx >= 0 && hovered_bar_idx < static_cast<int>(d.hourly_slots.size())) {
            const auto& hs = d.hourly_slots[hovered_bar_idx];
            cSub = "Hour " + hs.display + "  \xE2\x80\x94  Cost: " + FormatCost(hs.cost) +
                   "  \xC2\xB7  Requests: " + FormatWithCommas(hs.requests) +
                   "  \xC2\xB7  Tokens: " + FormatTokens(hs.tokens);
            subCol = COL_TEXT_MAIN;
        } else if (chart_mode == "heatmap" && hovered_heat_w >= 0 && hovered_heat_w < static_cast<int>(d.heatmap.weeks.size()) &&
                   hovered_heat_d >= 0 && hovered_heat_d < 7) {
            const auto& hc = d.heatmap.weeks[hovered_heat_w][hovered_heat_d];
            cSub = hc.display_date + "  \xE2\x80\x94  Tokens: " + FormatTokens(hc.tokens) +
                   "  \xC2\xB7  Steps: " + FormatWithCommas(hc.steps) +
                   "  \xC2\xB7  Cost: " + FormatCost(hc.cost);
            subCol = COL_TEXT_MAIN;
        }

        DrawTextUtf8(cTitle, tfBold10, D2D1::RectF(chartCard.left + 16.0f, chartCard.top + 10.0f, chartCard.left + 260.0f, chartCard.top + 28.0f), COL_TEXT_MAIN);
        DrawTextUtf8(cSub, tfRegular7, D2D1::RectF(chartCard.left + 16.0f, chartCard.top + 28.0f, chartCard.right - 250.0f, chartCard.top + 44.0f), subCol);

        // Chart Mode Switcher Buttons
        struct CModeItem { const char* key; const char* lbl; float w; } cmodes[] = {
            {"cost",    "Hourly Cost",      68.0f},
            {"tokens",  "Hourly Tokens",    76.0f},
            {"heatmap", "Activity Heatmap", 90.0f}
        };
        float cmTotalW = 6.0f;
        for (const auto& cm : cmodes) cmTotalW += cm.w + 2.0f;
        D2D1_RECT_F cmBox = D2D1::RectF(chartCard.right - cmTotalW - 14.0f, chartCard.top + 10.0f, chartCard.right - 14.0f, chartCard.top + 34.0f);
        DrawCard(cmBox, 6.0f, COL_CARD_HDR, COL_BORDER);
        float cmx = cmBox.left + 3.0f;
        for (const auto& cm : cmodes) {
            D2D1_RECT_F mr = D2D1::RectF(cmx, cmBox.top + 2.0f, cmx + cm.w, cmBox.bottom - 2.0f);
            bool isAct = (chart_mode == cm.key);
            if (isAct) FillRoundRect(mr, 4.0f, COL_PILL_ACT);
            DrawTextUtf8(cm.lbl, isAct ? tfBold7 : tfRegular7, mr, isAct ? COL_TEXT_MAIN : COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
            AddHit(std::string("cmode:") + cm.key, mr);
            cmx += cm.w + 2.0f;
        }

        D2D1_RECT_F plotArea = D2D1::RectF(chartCard.left + 14.0f, chartCard.top + 48.0f, chartCard.right - 14.0f, chartCard.bottom - 10.0f);
        if (chart_mode == "heatmap") {
            RenderHeatmapGrid(plotArea, d.heatmap);
        } else {
            RenderHourlyBarChart(plotArea, d.hourly_slots, chart_mode == "cost");
        }

        // 4. Usage by Model Table Card
        D2D1_RECT_F tblCard = D2D1::RectF(r.left, chartCard.bottom + 12.0f, r.right, r.bottom);
        DrawCard(tblCard, 10.0f, COL_CARD, COL_BORDER);

        DrawTextUtf8("Usage by Model", tfBold10, D2D1::RectF(tblCard.left + 16.0f, tblCard.top + 10.0f, tblCard.left + 120.0f, tblCard.top + 28.0f), COL_TEXT_MAIN);

        std::string pillTxt = std::to_string(d.models_count) + " models \xC2\xB7 " + std::to_string(d.providers_count) + " providers";
        D2D1_RECT_F mPill = D2D1::RectF(tblCard.left + 124.0f, tblCard.top + 11.0f, tblCard.left + 236.0f, tblCard.top + 27.0f);
        FillRoundRect(mPill, 4.0f, COL_PILL_BG);
        DrawTextUtf8(pillTxt, tfRegular7, mPill, COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);

        DrawTextUtf8("Per-model breakdown for the selected range.", tfRegular7,
                     D2D1::RectF(tblCard.left + 16.0f, tblCard.top + 28.0f, tblCard.left + 340.0f, tblCard.top + 44.0f), COL_TEXT_MUTED);

        // Table Header Bar
        float thY = tblCard.top + 48.0f;
        D2D1_RECT_F thRect = D2D1::RectF(tblCard.left + 2.0f, thY, tblCard.right - 2.0f, thY + 24.0f);
        FillRoundRect(thRect, 0.0f, COL_CARD_HDR);

        float tw = tblCard.right - tblCard.left - 32.0f;
        float c0 = tblCard.left + 16.0f;
        float c1 = c0 + tw * 0.34f;
        float c2 = c0 + tw * 0.45f;
        float c3 = c0 + tw * 0.56f;
        float c4 = c0 + tw * 0.68f;
        float c5 = c0 + tw * 0.82f;
        float c6 = c0 + tw * 0.91f;
        float cEnd = tblCard.right - 16.0f;

        DrawTextUtf8("Model",      tfBold7, D2D1::RectF(c0, thY, c1, thY + 24.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_LEADING);
        DrawTextUtf8("Requests",   tfBold7, D2D1::RectF(c1, thY, c2, thY + 24.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
        DrawTextUtf8("Input",      tfBold7, D2D1::RectF(c2, thY, c3, thY + 24.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
        DrawTextUtf8("Cache Read", tfBold7, D2D1::RectF(c3, thY, c4, thY + 24.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
        DrawTextUtf8("Live Quota", tfBold7, D2D1::RectF(c4, thY, c5, thY + 24.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
        DrawTextUtf8("Output",     tfBold7, D2D1::RectF(c5, thY, c6, thY + 24.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
        DrawTextUtf8("Cost \xE2\x86\x93", tfBold7, D2D1::RectF(c6, thY, cEnd, thY + 24.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);

        float rowY = thY + 24.0f;
        float availRowsH = tblCard.bottom - rowY - 6.0f;
        float rowH = std::max(21.0f, std::min(25.0f, availRowsH / std::max<size_t>(1, d.models_table.size())));

        for (size_t i = 0; i < d.models_table.size(); ++i) {
            if (rowY + rowH > tblCard.bottom - 2.0f) break;
            const auto& m = d.models_table[i];

            brush->SetColor(COL_DIVIDER);
            rt->DrawLine(D2D1::Point2F(tblCard.left + 8.0f, rowY), D2D1::Point2F(tblCard.right - 8.0f, rowY), brush, 1.0f);

            D2D1_RECT_F bdgR = D2D1::RectF(c0, rowY + 4.0f, c0 + 22.0f, rowY + rowH - 4.0f);
            FillRoundRect(bdgR, 3.0f, COL_PILL_BG);
            D2D1_COLOR_F bdgCol = (m.badge == "GO") ? COL_BLUE : ((m.badge == "CL") ? COL_PURPLE : COL_GREEN);
            DrawTextUtf8(m.badge, tfMonoBold7, bdgR, bdgCol, DWRITE_TEXT_ALIGNMENT_CENTER);

            DrawTextUtf8(m.label, tfBold8, D2D1::RectF(c0 + 28.0f, rowY, c1, rowY + rowH), COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_LEADING);
            DrawTextUtf8(m.requests > 0 ? FormatWithCommas(m.requests) : "0", tfRegular8, D2D1::RectF(c1, rowY, c2, rowY + rowH), COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_TRAILING);
            DrawTextUtf8(m.input > 0 ? FormatTokens(m.input) : "-", tfRegular8, D2D1::RectF(c2, rowY, c3, rowY + rowH), COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_TRAILING);
            DrawTextUtf8(m.cache_read > 0 ? FormatTokens(m.cache_read) : "-", tfRegular8, D2D1::RectF(c3, rowY, c4, rowY + rowH), COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_TRAILING);

            if (m.has_quota) {
                D2D1_COLOR_F qCol = (m.remaining_pct >= 50.0) ? COL_GREEN : ((m.remaining_pct >= 20.0) ? COL_AMBER : COL_RED);
                char qBuf[64];
                if (!m.reset_relative.empty()) std::snprintf(qBuf, sizeof(qBuf), "%.1f%% (%s)", m.remaining_pct, m.reset_relative.c_str());
                else std::snprintf(qBuf, sizeof(qBuf), "%.1f%%", m.remaining_pct);
                DrawTextUtf8(qBuf, tfBold8, D2D1::RectF(c4, rowY, c5, rowY + rowH), qCol, DWRITE_TEXT_ALIGNMENT_TRAILING);
            } else {
                DrawTextUtf8("-", tfRegular8, D2D1::RectF(c4, rowY, c5, rowY + rowH), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
            }

            DrawTextUtf8(m.output > 0 ? FormatTokens(m.output) : "-", tfRegular8, D2D1::RectF(c5, rowY, c6, rowY + rowH), COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_TRAILING);
            DrawTextUtf8(FormatCost(m.cost), tfBold8, D2D1::RectF(c6, rowY, cEnd, rowY + rowH), COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_TRAILING);

            rowY += rowH;
        }
    }

    void RenderHourlyBarChart(const D2D1_RECT_F& area, const std::vector<HourlySlot>& slots, bool isCost) {
        if (slots.empty()) return;

        double rawMax = 0.001;
        for (const auto& s : slots) {
            double v = isCost ? s.cost : static_cast<double>(s.tokens);
            if (v > rawMax) rawMax = v;
        }

        double niceMax = rawMax * 1.1;
        std::string topLbl, botLbl;
        if (isCost) {
            niceMax = (rawMax > 1.0) ? std::ceil(rawMax) : std::max(0.5, std::ceil(rawMax * 2.0) / 2.0);
            char b[32];
            if ( std::abs(niceMax - std::round(niceMax)) < 1e-5 ) std::snprintf(b, sizeof(b), "$%d", static_cast<int>(niceMax));
            else std::snprintf(b, sizeof(b), "$%.1f", niceMax);
            topLbl = b;
            botLbl = "$0";
        } else {
            topLbl = FormatTokens(static_cast<long long>(rawMax));
            botLbl = "0";
        }

        float leftPad = area.left + 38.0f;
        float rightPad = area.right - 8.0f;
        float topPad = area.top + 6.0f;
        float botPad = area.bottom - 20.0f;
        float plotW = rightPad - leftPad;
        float plotH = botPad - topPad;

        DrawTextUtf8(topLbl, tfRegular7, D2D1::RectF(area.left, topPad - 6.0f, leftPad - 6.0f, topPad + 10.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
        DrawTextUtf8(botLbl, tfRegular7, D2D1::RectF(area.left, botPad - 8.0f, leftPad - 6.0f, botPad + 8.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);

        brush->SetColor(COL_BORDER);
        for (float frac : {0.0f, 0.5f, 1.0f}) {
            float gy = topPad + frac * plotH;
            rt->DrawLine(D2D1::Point2F(leftPad, gy), D2D1::Point2F(rightPad, gy), brush, 1.0f, dashedStroke);
        }

        int nBars = static_cast<int>(slots.size());
        float slotW = plotW / std::max(1, nBars);
        float barGap = std::max(3.0f, slotW * 0.28f);
        float ease = 1.0f - std::pow(1.0f - chart_anim, 3.0f);

        hovered_bar_idx = -1;
        for (int i = 0; i < nBars; ++i) {
            const auto& s = slots[i];
            double val = isCost ? s.cost : static_cast<double>(s.tokens);
            float bx1 = leftPad + i * slotW + barGap * 0.5f;
            float bx2 = leftPad + (i + 1) * slotW - barGap * 0.5f;

            if (last_mouse.x >= bx1 - 2.0f && last_mouse.x <= bx2 + 2.0f &&
                last_mouse.y >= topPad && last_mouse.y <= botPad) {
                hovered_bar_idx = i;
            }

            float ratio = static_cast<float>(std::min(1.0, val / std::max(1e-9, niceMax))) * ease;
            float bh = (val > 0.0) ? std::max(3.0f, ratio * plotH) : 1.0f;
            float by1 = botPad - bh;

            bool isHov = (hovered_bar_idx == i);
            D2D1_COLOR_F bCol = isHov ? COL_BAR_HOVER : ((val > 0.0) ? COL_BAR : HexColor(0x222222));
            FillRoundRect(D2D1::RectF(bx1, by1, bx2, botPad), 1.5f, bCol);

            if (val > 0.0 && bh >= 4.0f) {
                brush->SetColor(isHov ? HexColor(0x6EE7B7) : HexColor(0x52D690));
                rt->DrawLine(D2D1::Point2F(bx1, by1), D2D1::Point2F(bx2, by1), brush, 1.2f);
            }

            if (i == 0 || i == 4 || i == 8 || i == 12 || i == 16 || i == 20 || i == 23) {
                float cx = (bx1 + bx2) * 0.5f;
                DrawTextUtf8(s.label, tfRegular7, D2D1::RectF(cx - 24.0f, botPad + 4.0f, cx + 24.0f, area.bottom), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_CENTER);
            }
        }
    }

    void RenderHeatmapGrid(const D2D1_RECT_F& area, const HeatmapData& hm) {
        if (hm.weeks.empty()) return;

        float leftPad = area.left + 34.0f;
        float topPad = area.top + 16.0f;
        float botPad = area.bottom - 22.0f;
        float rightPad = area.right - 6.0f;

        int totalCols = static_cast<int>(hm.weeks.size());
        float availW = rightPad - leftPad;
        float availH = botPad - topPad;
        float cellGap = 3.0f;
        float stepX = availW / std::max(1, totalCols);
        float cellSize = std::max(7.0f, std::min({(availH - 6.0f * cellGap) / 7.0f, stepX - cellGap, 12.5f}));

        static const uint32_t kHeatCols[] = {0x1C1C1C, 0x1B4332, 0x2D6A4F, 0x40916C, 0x52B788};

        struct DayLbl { int r; const char* name; } dayLbls[] = {{0, "Mon"}, {2, "Wed"}, {4, "Fri"}};
        for (const auto& dl : dayLbls) {
            float ly = topPad + dl.r * (cellSize + cellGap);
            DrawTextUtf8(dl.name, tfRegular7, D2D1::RectF(area.left, ly - 2.0f, leftPad - 6.0f, ly + cellSize + 2.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
        }

        float ease = 1.0f - std::pow(1.0f - chart_anim, 3.0f);
        int revealLimit = static_cast<int>(totalCols * ease);

        hovered_heat_w = -1;
        hovered_heat_d = -1;
        float lastMonthX = -999.0f;

        for (int w = 0; w < totalCols; ++w) {
            float cx = leftPad + w * stepX;
            const auto& week = hm.weeks[w];
            if (!week.empty() && !week[0].month_label.empty() && (cx - lastMonthX >= 34.0f)) {
                DrawTextUtf8(week[0].month_label, tfBold7, D2D1::RectF(cx, area.top, cx + 36.0f, topPad - 2.0f), COL_TEXT_MUTED);
                lastMonthX = cx;
            }
            for (int d = 0; d < static_cast<int>(week.size()); ++d) {
                float cy = topPad + d * (cellSize + cellGap);
                const auto& cell = week[d];
                D2D1_RECT_F cr = D2D1::RectF(cx, cy, cx + cellSize, cy + cellSize);

                bool isHov = (last_mouse.x >= cr.left && last_mouse.x <= cr.right && last_mouse.y >= cr.top && last_mouse.y <= cr.bottom);
                if (isHov) {
                    hovered_heat_w = w;
                    hovered_heat_d = d;
                }

                int intens = (w <= revealLimit) ? cell.intensity : 0;
                D2D1_COLOR_F fillCol = cell.is_future ? HexColor(0x141414) : HexColor(kHeatCols[ std::clamp(intens, 0, 4) ]);
                if (isHov) fillCol = HexColor(0xFFFFFF);

                FillRoundRect(cr, 2.0f, fillCol);
                if (cell.is_today || isHov) {
                    StrokeRoundRect(cr, 2.0f, isHov ? COL_GREEN : HexColor(0xFFFFFF), 1.0f);
                } else if (intens == 0 && !cell.is_future) {
                    StrokeRoundRect(cr, 2.0f, COL_BORDER, 0.7f);
                }
            }
        }

        std::string sumTxt = FormatWithCommas(hm.total_steps) + " steps  \xC2\xB7  " + FormatTokens(hm.total_tokens) +
                             " tokens  \xC2\xB7  " + std::to_string(hm.active_days) + " active days  \xC2\xB7  \xF0\x9F\x94\xA5 " +
                             std::to_string(hm.streak) + "d streak";
        DrawTextUtf8(sumTxt, tfRegular7, D2D1::RectF(leftPad, botPad + 4.0f, rightPad - 150.0f, area.bottom), COL_TEXT_MUTED);

        float legX = rightPad - 115.0f;
        DrawTextUtf8("Less", tfRegular7, D2D1::RectF(legX - 30.0f, botPad + 4.0f, legX, area.bottom), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
        for (int i = 0; i < 5; ++i) {
            float lx = legX + 6.0f + i * 11.0f;
            FillRoundRect(D2D1::RectF(lx, botPad + 7.0f, lx + 8.0f, botPad + 15.0f), 1.5f, HexColor(kHeatCols[i]));
        }
        DrawTextUtf8("More", tfRegular7, D2D1::RectF(legX + 66.0f, botPad + 4.0f, rightPad, area.bottom), COL_TEXT_MUTED);
    }
