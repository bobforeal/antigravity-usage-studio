    // ========================================================================
    // View 4: Live Antigravity Quotas & Limits (Zero-Flicker Direct2D Grid)
    // ========================================================================
    void RenderQuotasView(const D2D1_RECT_F& r) {
        QuotaData q;
        std::map<std::string, float> lerpSnap;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            q = quota_data;
            lerpSnap = quota_bar_lerp;
        }

        // Header Row
        DrawTextUtf8("Live Antigravity Quotas & Limits", tfBold12, D2D1::RectF(r.left, r.top, r.left + 340.0f, r.top + 22.0f), COL_TEXT_MAIN);
        DrawTextUtf8("Real-time RPC status directly from LanguageServerService/GetUserStatus.", tfRegular8,
                     D2D1::RectF(r.left, r.top + 22.0f, r.left + 460.0f, r.top + 40.0f), COL_TEXT_MUTED);

        // Credits & Connection Pills
        std::string credTxt = q.has_credits ? ("Credits: " + FormatWithCommas(q.avail_credits) + " / " + FormatWithCommas(q.monthly_credits)) : "Credits: Standard";
        D2D1_RECT_F credPill = D2D1::RectF(r.right - 134.0f, r.top + 6.0f, r.right, r.top + 30.0f);
        DrawCard(credPill, 6.0f, COL_CARD, COL_BORDER);
        DrawTextUtf8(credTxt, tfBold7, credPill, COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_CENTER);

        std::string connTxt = q.connected ? ("\xE2\x97\x8F Connected \xC2\xB7 " + q.tier + " \xC2\xB7 " + std::to_string(q.models.size()) + " Models") : "\xE2\x97\x8F Reconnecting RPC...";
        D2D1_RECT_F connPill = D2D1::RectF(credPill.left - 220.0f, r.top + 6.0f, credPill.left - 8.0f, r.top + 30.0f);
        DrawCard(connPill, 6.0f, COL_CARD, COL_BORDER);
        DrawTextUtf8(connTxt, tfBold7, connPill, q.connected ? COL_GREEN : COL_AMBER, DWRITE_TEXT_ALIGNMENT_CENTER);

        // Scrollable 2-Column Grid of Model Quota Cards
        D2D1_RECT_F scrollArea = D2D1::RectF(r.left, r.top + 48.0f, r.right, r.bottom);
        rt->PushAxisAlignedClip(scrollArea, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        float curY = scrollArea.top - quotas_scroll_y;
        float colW = (r.right - r.left - 12.0f) * 0.5f;
        float cardH = 82.0f;
        float ease = 1.0f - std::pow(1.0f - quotas_anim, 3.0f);

        // ====================================================================
        // TOP SUMMARY ROW: Gemini Pool (5h + Weekly)  |  Claude & GPT Pool (5h + Weekly)
        // ====================================================================
        float sumH = 110.0f;
        if (curY + sumH >= scrollArea.top && curY <= scrollArea.bottom) {
            auto drawPoolSummaryCard = [&](float cx, const char* badgeTxt, D2D1_COLOR_F accentCol, D2D1_COLOR_F bdrCol,
                                           const char* titleTxt, const char* subModelsTxt,
                                           float pct5h, const std::string& rst5h,
                                           float pctWk, const std::string& rstWk) {
                D2D1_RECT_F cardR = D2D1::RectF(cx, curY, cx + colW, curY + sumH);
                DrawCard(cardR, 10.0f, HexColor(0x141519), bdrCol, 1.2f);

                float inL = cardR.left + 14.0f;
                float inR = cardR.right - 14.0f;
                float barW = inR - inL;

                // Header Badge + Pool Title + Models Subtitle
                D2D1_RECT_F bdgR = D2D1::RectF(inL, cardR.top + 10.0f, inL + 24.0f, cardR.top + 26.0f);
                FillRoundRect(bdgR, 3.5f, COL_PILL_BG);
                DrawTextUtf8(badgeTxt, tfMonoBold7, bdgR, accentCol, DWRITE_TEXT_ALIGNMENT_CENTER);

                DrawTextUtf8(titleTxt, tfBold8, D2D1::RectF(inL + 30.0f, cardR.top + 9.0f, inR, cardR.top + 27.0f), COL_TEXT_MAIN);
                DrawTextUtf8(subModelsTxt, tfRegular7, D2D1::RectF(inL + 30.0f, cardR.top + 24.0f, inR, cardR.top + 38.0f), COL_TEXT_MUTED);

                // Bar 1: 5-Hour Limit Remaining
                float y5 = cardR.top + 43.0f;
                DrawTextUtf8("5-Hour Limit Remaining", tfBold7, D2D1::RectF(inL, y5, inL + 150.0f, y5 + 15.0f), COL_TEXT_MAIN);
                char b5[64];
                std::snprintf(b5, sizeof(b5), "\xE2\x8F\xB3 %s  \xC2\xB7  %.1f%%", rst5h.c_str(), pct5h);
                D2D1_COLOR_F col5 = (pct5h >= 50.0f) ? COL_GREEN : ((pct5h >= 20.0f) ? COL_AMBER : COL_RED);
                DrawTextUtf8(b5, tfBold7, D2D1::RectF(inR - 140.0f, y5, inR, y5 + 15.0f), col5, DWRITE_TEXT_ALIGNMENT_TRAILING);

                D2D1_RECT_F tr5 = D2D1::RectF(inL, y5 + 17.0f, inR, y5 + 23.0f);
                FillRoundRect(tr5, 3.0f, HexColor(0x222228));
                float fw5 = std::clamp(pct5h / 100.0f, 0.0f, 1.0f) * barW * ease;
                if (fw5 > 1.0f) FillRoundRect(D2D1::RectF(inL, tr5.top, inL + fw5, tr5.bottom), 3.0f, col5);

                // Bar 2: Weekly Limit Remaining
                float yW = cardR.top + 72.0f;
                DrawTextUtf8("Weekly Limit Remaining", tfBold7, D2D1::RectF(inL, yW, inL + 150.0f, yW + 15.0f), accentCol);
                char bW[64];
                std::snprintf(bW, sizeof(bW), "\xE2\x8F\xB3 %s  \xC2\xB7  %.1f%%", rstWk.c_str(), pctWk);
                D2D1_COLOR_F colW = (pctWk >= 50.0f) ? accentCol : ((pctWk >= 20.0f) ? COL_AMBER : COL_RED);
                DrawTextUtf8(bW, tfBold7, D2D1::RectF(inR - 140.0f, yW, inR, yW + 15.0f), colW, DWRITE_TEXT_ALIGNMENT_TRAILING);

                D2D1_RECT_F trW = D2D1::RectF(inL, yW + 17.0f, inR, yW + 23.0f);
                FillRoundRect(trW, 3.0f, HexColor(0x222228));
                float fwW = std::clamp(pctWk / 100.0f, 0.0f, 1.0f) * barW * ease;
                if (fwW > 1.0f) FillRoundRect(D2D1::RectF(inL, trW.top, inL + fwW, trW.bottom), 3.0f, colW);
            };

            float gem5h = static_cast<float>(q.gemini_5h_pct);
            if (lerpSnap.count("__gemini_5h__")) gem5h = lerpSnap["__gemini_5h__"];
            float gemWk = static_cast<float>(q.gemini_weekly_pct);
            if (lerpSnap.count("__gemini_wk__")) gemWk = lerpSnap["__gemini_wk__"];

            float cl5h = static_cast<float>(q.claude_5h_pct);
            if (lerpSnap.count("__claude_5h__")) cl5h = lerpSnap["__claude_5h__"];
            float clWk = static_cast<float>(q.claude_weekly_pct);
            if (lerpSnap.count("__claude_wk__")) clWk = lerpSnap["__claude_wk__"];

            drawPoolSummaryCard(r.left, "GO", COL_BLUE, HexColor(0x283B5B),
                                "GEMINI MODELS QUOTA POOL", "Shared across Gemini 4 Argon & Gemini 3.8 Flash",
                                gem5h, q.gemini_5h_reset, gemWk, q.gemini_weekly_reset);

            drawPoolSummaryCard(r.left + colW + 12.0f, "CL", COL_PURPLE, HexColor(0x3E2B5B),
                                "CLAUDE & GPT MODELS QUOTA POOL", "Shared across Claude Opus 5.5, Sonnet 5.5 & GPT-OSS",
                                cl5h, q.claude_5h_reset, clWk, q.claude_weekly_reset);
        }
        curY += sumH + 14.0f;

        for (const char* provName : {"Google", "Anthropic", "OpenAI"}) {
            std::vector<QuotaModelInfo> mList;
            for (const auto& m : q.models) {
                if (m.provider == provName) mList.push_back(m);
            }
            if (mList.empty()) continue;

            std::string secTitle = ToLowerAscii(provName);
            for (char& c : secTitle) c = static_cast<char>(std::toupper(c));
            secTitle += " MODELS (" + std::to_string(mList.size()) + ")";

            if (curY + 24.0f >= scrollArea.top && curY <= scrollArea.bottom) {
                DrawTextUtf8(secTitle, tfBold8, D2D1::RectF(r.left, curY + 2.0f, r.left + 260.0f, curY + 22.0f), COL_TEXT_MUTED);
            }
            curY += 26.0f;

            int rows = (static_cast<int>(mList.size()) + 1) / 2;
            for (int idx = 0; idx < static_cast<int>(mList.size()); ++idx) {
                int rIdx = idx / 2;
                int cIdx = idx % 2;
                float cx = r.left + cIdx * (colW + 12.0f);
                float cy = curY + rIdx * (cardH + 10.0f);

                if (cy + cardH >= scrollArea.top && cy <= scrollArea.bottom) {
                    const auto& m = mList[idx];
                    D2D1_RECT_F cardR = D2D1::RectF(cx, cy, cx + colW, cy + cardH);
                    bool cardHov = (last_mouse.x >= cardR.left && last_mouse.x <= cardR.right &&
                                    last_mouse.y >= cardR.top && last_mouse.y <= cardR.bottom);
                    DrawCard(cardR, 10.0f, cardHov ? HexColor(0x18181C) : COL_CARD, cardHov ? HexColor(0x36363F) : COL_BORDER);

                    float inL = cardR.left + 14.0f;
                    float inR = cardR.right - 14.0f;

                    // Provider Badge
                    D2D1_RECT_F bdgR = D2D1::RectF(inL, cardR.top + 12.0f, inL + 22.0f, cardR.top + 28.0f);
                    FillRoundRect(bdgR, 3.0f, COL_PILL_BG);
                    D2D1_COLOR_F bdgCol = (m.badge == "GO") ? COL_BLUE : ((m.badge == "CL") ? COL_PURPLE : COL_GREEN);
                    DrawTextUtf8(m.badge, tfMonoBold7, bdgR, bdgCol, DWRITE_TEXT_ALIGNMENT_CENTER);

                    // Model Name
                    DrawTextUtf8(m.label, tfBold8, D2D1::RectF(inL + 28.0f, cardR.top + 10.0f, inR - 70.0f, cardR.top + 30.0f), COL_TEXT_MAIN);

                    // Recommended Star Pill
                    if (m.is_recommended) {
                        float nameW = static_cast<float>(m.label.size()) * 5.8f + 34.0f;
                        D2D1_RECT_F recR = D2D1::RectF(inL + nameW, cardR.top + 13.0f, inL + nameW + 38.0f, cardR.top + 27.0f);
                        FillRoundRect(recR, 3.0f, COL_PILL_BG);
                        DrawTextUtf8("\xE2\x98\x85 REC", tfBold7, recR, COL_AMBER, DWRITE_TEXT_ALIGNMENT_CENTER);
                    }

                    // Percentage Label (uses spring-lerped value so numbers never jump!)
                    float dispPct = static_cast<float>(m.remaining_pct);
                    auto itL = lerpSnap.find(m.label);
                    if (itL != lerpSnap.end()) dispPct = itL->second;

                    D2D1_COLOR_F qCol = (dispPct >= 50.0f) ? COL_GREEN : ((dispPct >= 20.0f) ? COL_AMBER : COL_RED);
                    char pctBuf[32];
                    std::snprintf(pctBuf, sizeof(pctBuf), "%.1f%%", dispPct);
                    DrawTextUtf8(pctBuf, tfBold10, D2D1::RectF(inR - 70.0f, cardR.top + 10.0f, inR, cardR.top + 30.0f), qCol, DWRITE_TEXT_ALIGNMENT_TRAILING);

                    // 6px Progress Bar (spring-lerped width, never resets to 0!)
                    D2D1_RECT_F barTrack = D2D1::RectF(inL, cardR.top + 38.0f, inR, cardR.top + 44.0f);
                    FillRoundRect(barTrack, 3.0f, HexColor(0x242424));
                    float fillW = static_cast<float>((dispPct / 100.0f) * (inR - inL)) * ease;
                    if (fillW > 1.0f) {
                        FillRoundRect(D2D1::RectF(inL, barTrack.top, inL + fillW, barTrack.bottom), 3.0f, qCol);
                    }

                    // Reset Countdown & Model ID
                    std::string rstTxt = !m.reset_relative.empty() ? ("\xE2\x8F\xB3 Resets in " + m.reset_relative) : "\xE2\x8F\xB3 Ready";
                    DrawTextUtf8(rstTxt, tfRegular7, D2D1::RectF(inL, cardR.top + 52.0f, inL + 200.0f, cardR.bottom - 8.0f), COL_TEXT_MUTED);
                    if (!m.model_id.empty()) {
                        std::string shortId = m.model_id.substr(0, std::min<size_t>(22, m.model_id.size()));
                        DrawTextUtf8(shortId, tfMono7, D2D1::RectF(inR - 160.0f, cardR.top + 52.0f, inR, cardR.bottom - 8.0f), COL_TEXT_DIM, DWRITE_TEXT_ALIGNMENT_TRAILING);
                    }
                }
            }
            curY += rows * (cardH + 10.0f) + 8.0f;
        }

        rt->PopAxisAlignedClip();
    }

    // ========================================================================
    // View 5: Recent Sessions Table
    // ========================================================================
    void RenderSessionsView(const D2D1_RECT_F& r) {
        std::vector<SessionRow> allSessions;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            allSessions = dash_data.sessions_list;
        }

        std::string qLow = ToLowerAscii(session_search_query);
        std::vector<SessionRow> filtered;
        for (const auto& s : allSessions) {
            if (qLow.empty() ||
                ToLowerAscii(s.title).find(qLow) != std::string::npos ||
                ToLowerAscii(s.model).find(qLow) != std::string::npos ||
                ToLowerAscii(s.session_id).find(qLow) != std::string::npos) {
                filtered.push_back(s);
            }
        }

        std::string hdrTitle = qLow.empty()
            ? ("Recent Antigravity Sessions (" + std::to_string(allSessions.size()) + ")")
            : ("Recent Antigravity Sessions (" + std::to_string(filtered.size()) + " matches)");
        DrawTextUtf8(hdrTitle, tfBold12, D2D1::RectF(r.left, r.top, r.left + 380.0f, r.top + 22.0f), COL_TEXT_MAIN);
        DrawTextUtf8("Complete token breakdown across your recent workspace conversations.", tfRegular8,
                     D2D1::RectF(r.left, r.top + 22.0f, r.left + 440.0f, r.top + 40.0f), COL_TEXT_MUTED);

        // Search Box
        D2D1_RECT_F srchBox = D2D1::RectF(r.right - 230.0f, r.top + 6.0f, r.right, r.top + 32.0f);
        bool srchFocus = (active_input_field == "session_search");
        DrawCard(srchBox, 6.0f, COL_CARD, srchFocus ? COL_CORAL_BDR : COL_BORDER);
        AddHit("focus:session_search", srchBox);
        DrawTextUtf8("\xF0\x9F\x94\x8D", tfRegular8, D2D1::RectF(srchBox.left + 8.0f, srchBox.top, srchBox.left + 26.0f, srchBox.bottom), COL_TEXT_MUTED);

        std::string sDisp = session_search_query;
        D2D1_COLOR_F sCol = COL_TEXT_MAIN;
        if (sDisp.empty() && !srchFocus) {
            sDisp = "Filter sessions...";
            sCol = COL_TEXT_DIM;
        } else if (srchFocus && ((mascot_tick / 18) % 2 == 0)) {
            sDisp += "|";
        }
        DrawTextUtf8(sDisp, tfRegular8, D2D1::RectF(srchBox.left + 28.0f, srchBox.top, srchBox.right - 8.0f, srchBox.bottom), sCol);

        // Sessions Table Card
        D2D1_RECT_F cardR = D2D1::RectF(r.left, r.top + 48.0f, r.right, r.bottom);
        DrawCard(cardR, 10.0f, COL_CARD, COL_BORDER);

        float thY = cardR.top + 2.0f;
        FillRoundRect(D2D1::RectF(cardR.left + 2.0f, thY, cardR.right - 2.0f, thY + 26.0f), 8.0f, COL_CARD_HDR);

        float w = cardR.right - cardR.left - 24.0f;
        float x0 = cardR.left + 12.0f;
        float x1 = x0 + w * 0.28f;
        float x2 = x0 + w * 0.42f;
        float x3 = x0 + w * 0.60f;
        float x4 = x0 + w * 0.67f;
        float x5 = x0 + w * 0.74f;
        float x6 = x0 + w * 0.82f;
        float x7 = x0 + w * 0.90f;
        float xEnd = cardR.right - 12.0f;

        DrawTextUtf8("Session / Title", tfBold7, D2D1::RectF(x0, thY, x1, thY + 26.0f), COL_TEXT_MUTED);
        DrawTextUtf8("Last Active",     tfBold7, D2D1::RectF(x1, thY, x2, thY + 26.0f), COL_TEXT_MUTED);
        DrawTextUtf8("Model",           tfBold7, D2D1::RectF(x2, thY, x3, thY + 26.0f), COL_TEXT_MUTED);
        DrawTextUtf8("Reqs",            tfBold7, D2D1::RectF(x3, thY, x4, thY + 26.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
        DrawTextUtf8("Input",           tfBold7, D2D1::RectF(x4, thY, x5, thY + 26.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
        DrawTextUtf8("Cache",           tfBold7, D2D1::RectF(x5, thY, x6, thY + 26.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
        DrawTextUtf8("Total",           tfBold7, D2D1::RectF(x6, thY, x7, thY + 26.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
        DrawTextUtf8("Cost",            tfBold7, D2D1::RectF(x7, thY, xEnd, thY + 26.0f), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);

        D2D1_RECT_F listClip = D2D1::RectF(cardR.left + 2.0f, thY + 26.0f, cardR.right - 2.0f, cardR.bottom - 2.0f);
        rt->PushAxisAlignedClip(listClip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        float ry = listClip.top - sessions_scroll_y;
        float rowH = 36.0f;
        for (const auto& s : filtered) {
            if (ry + rowH >= listClip.top && ry <= listClip.bottom) {
                brush->SetColor(COL_DIVIDER);
                rt->DrawLine(D2D1::Point2F(cardR.left + 8.0f, ry), D2D1::Point2F(cardR.right - 8.0f, ry), brush, 1.0f);

                DrawTextUtf8(s.title, tfBold8, D2D1::RectF(x0, ry + 3.0f, x1 - 6.0f, ry + 20.0f), COL_TEXT_MAIN);
                DrawTextUtf8(s.session_id.substr(0, 12) + "...", tfMono7, D2D1::RectF(x0, ry + 19.0f, x1 - 6.0f, ry + 34.0f), COL_TEXT_DIM);

                DrawTextUtf8(s.last_modified_full, tfRegular8, D2D1::RectF(x1, ry, x2, ry + rowH), COL_TEXT_MUTED);

                D2D1_RECT_F bdgR = D2D1::RectF(x2, ry + 10.0f, x2 + 20.0f, ry + 25.0f);
                FillRoundRect(bdgR, 3.0f, COL_PILL_BG);
                D2D1_COLOR_F bCol = (s.badge == "GO") ? COL_BLUE : ((s.badge == "CL") ? COL_PURPLE : COL_GREEN);
                DrawTextUtf8(s.badge, tfMonoBold7, bdgR, bCol, DWRITE_TEXT_ALIGNMENT_CENTER);
                DrawTextUtf8(s.model.substr(0, 20), tfRegular8, D2D1::RectF(x2 + 24.0f, ry, x3, ry + rowH), COL_TEXT_MAIN);

                DrawTextUtf8(FormatWithCommas(s.requests), tfRegular8, D2D1::RectF(x3, ry, x4, ry + rowH), COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_TRAILING);
                DrawTextUtf8(FormatTokens(s.input_tokens), tfRegular8, D2D1::RectF(x4, ry, x5, ry + rowH), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
                DrawTextUtf8(FormatTokens(s.cache_tokens), tfRegular8, D2D1::RectF(x5, ry, x6, ry + rowH), COL_TEXT_MUTED, DWRITE_TEXT_ALIGNMENT_TRAILING);
                DrawTextUtf8(FormatTokens(s.total_tokens), tfBold8,    D2D1::RectF(x6, ry, x7, ry + rowH), COL_TEXT_MAIN, DWRITE_TEXT_ALIGNMENT_TRAILING);
                DrawTextUtf8(FormatCost(s.cost),           tfBold8,    D2D1::RectF(x7, ry, xEnd, ry + rowH), COL_GREEN,   DWRITE_TEXT_ALIGNMENT_TRAILING);
            }
            ry += rowH;
        }

        rt->PopAxisAlignedClip();
    }

    // ========================================================================
    // Popup Model Dropdown Menu (Spring-Animated Scale + Sliding Hover Pill)
    // ========================================================================
    void RenderPopupMenu(float W, float H) {
        float ease = 1.0f - std::pow(1.0f - std::clamp(popup_anim, 0.05f, 1.0f), 3.0f);
        float mw = 218.0f;
        float itemH = 26.0f;
        float fullMh = static_cast<float>(AVAILABLE_MODELS.size()) * itemH + 10.0f;
        float mh = fullMh * (0.82f + 0.18f * ease);
        float px = std::clamp(popup_x, 10.0f, W - mw - 10.0f);
        float py = std::clamp(popup_y - (1.0f - ease) * 6.0f, 38.0f, H - fullMh - 10.0f);

        std::string curSelected;
        if (popup_target == "cli" && current_view.rfind("cli_", 0) == 0) {
            curSelected = StripModelEffortSuffix(cli_instances[current_view].model);
        } else if (popup_target.rfind("worker_", 0) == 0) {
            int wIdx = std::stoi(popup_target.substr(7));
            if (wIdx >= 0 && wIdx < 4) curSelected = StripModelEffortSuffix(workers[wIdx].model);
        }

        // Multi-layer drop shadow & backdrop mask so underlying text/lines never bleed through
        FillRoundRect(D2D1::RectF(px - 4.0f, py - 2.0f, px + mw + 4.0f, py + mh + 6.0f), 11.0f, HexColor(0x000000, 0.65f * ease));
        FillRoundRect(D2D1::RectF(px - 2.0f, py - 1.0f, px + mw + 2.0f, py + mh + 3.0f), 9.5f, HexColor(0x050507, 0.85f * ease));

        D2D1_RECT_F menuR = D2D1::RectF(px, py, px + mw, py + mh);
        DrawCard(menuR, 8.0f, HexColor(0x16161A, ease), HexColor(0x6E3B2B, ease), 1.3f);

        float iy = py + 5.0f;
        for (const auto& mod : AVAILABLE_MODELS) {
            D2D1_RECT_F ir = D2D1::RectF(px + 5.0f, iy, px + mw - 5.0f, iy + itemH - 2.0f);
            std::string hid = "popup_select:" + mod;
            bool isHov = (hovered_id == hid);
            bool isSel = (curSelected == mod);

            if (isHov) {
                FillRoundRect(ir, 5.0f, COL_CORAL);
            } else if (isSel) {
                FillRoundRect(ir, 5.0f, HexColor(0x26201D));
                StrokeRoundRect(ir, 5.0f, HexColor(0x523025), 0.8f);
            }

            std::string prov, bdg;
            GetModelProviderAndBadge(mod, prov, bdg);
            D2D1_RECT_F bdgR = D2D1::RectF(ir.left + 6.0f, ir.top + 4.0f, ir.left + 26.0f, ir.bottom - 4.0f);
            FillRoundRect(bdgR, 3.0f, isHov ? HexColor(0x18181B, 0.25f) : COL_PILL_BG);
            D2D1_COLOR_F bCol = isHov ? HexColor(0x0D0D0D) : ((bdg == "GO") ? COL_BLUE : ((bdg == "CL") ? COL_PURPLE : COL_GREEN));
            DrawTextUtf8(bdg, tfMonoBold7, bdgR, bCol, DWRITE_TEXT_ALIGNMENT_CENTER);

            D2D1_COLOR_F txtCol = isHov ? HexColor(0x0D0D0D) : (isSel ? COL_CORAL : COL_TEXT_MAIN);
            DrawTextUtf8(mod, (isHov || isSel) ? tfBold8 : tfRegular8,
                         D2D1::RectF(ir.left + 32.0f, ir.top, ir.right - 22.0f, ir.bottom), txtCol);

            if (isSel) {
                DrawTextUtf8("\xE2\x9C\x93", tfBold8, D2D1::RectF(ir.right - 20.0f, ir.top, ir.right - 6.0f, ir.bottom),
                             isHov ? HexColor(0x0D0D0D) : COL_CORAL, DWRITE_TEXT_ALIGNMENT_CENTER);
            }

            AddHit(hid, ir);
            iy += itemH;
        }
    }
