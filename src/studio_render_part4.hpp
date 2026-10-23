    // ========================================================================
    // View 4: Live Antigravity Quotas & Limits (Zero-Flicker Direct2D Grid)
    // ========================================================================
    void RenderQuotasView(const D2D1_RECT_F& r) {
        QuotaData q;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            q = quota_data;
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
                    DrawCard(cardR, 10.0f, COL_CARD, COL_BORDER);

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

                    // Percentage Label
                    D2D1_COLOR_F qCol = (m.remaining_pct >= 50.0) ? COL_GREEN : ((m.remaining_pct >= 20.0) ? COL_AMBER : COL_RED);
                    char pctBuf[32];
                    std::snprintf(pctBuf, sizeof(pctBuf), "%.1f%%", m.remaining_pct);
                    DrawTextUtf8(pctBuf, tfBold10, D2D1::RectF(inR - 70.0f, cardR.top + 10.0f, inR, cardR.top + 30.0f), qCol, DWRITE_TEXT_ALIGNMENT_TRAILING);

                    // 6px Progress Bar
                    D2D1_RECT_F barTrack = D2D1::RectF(inL, cardR.top + 38.0f, inR, cardR.top + 44.0f);
                    FillRoundRect(barTrack, 3.0f, HexColor(0x242424));
                    float fillW = static_cast<float>((m.remaining_pct / 100.0) * (inR - inL)) * ease;
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
    // Popup Model Dropdown Menu
    // ========================================================================
    void RenderPopupMenu(float W, float H) {
        float mw = 215.0f;
        float itemH = 22.0f;
        float mh = AVAILABLE_MODELS.size() * itemH + 8.0f;
        float px = std::clamp(popup_x, 10.0f, W - mw - 10.0f);
        float py = std::clamp(popup_y, 38.0f, H - mh - 10.0f);

        D2D1_RECT_F menuR = D2D1::RectF(px, py, px + mw, py + mh);
        DrawCard(menuR, 8.0f, HexColor(0x181818), COL_CORAL_BDR, 1.2f);

        float iy = py + 4.0f;
        for (const auto& mod : AVAILABLE_MODELS) {
            D2D1_RECT_F ir = D2D1::RectF(px + 4.0f, iy, px + mw - 4.0f, iy + itemH);
            std::string hid = "popup_select:" + mod;
            if (hovered_id == hid) {
                FillRoundRect(ir, 4.0f, COL_CORAL);
            }
            DrawTextUtf8("  " + mod, tfRegular8, ir, (hovered_id == hid) ? HexColor(0x0D0D0D) : COL_TEXT_MAIN);
            AddHit(hid, ir);
            iy += itemH;
        }
    }
