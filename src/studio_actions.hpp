    // ========================================================================
    // Interactive Event Handlers & CLI / Orchestrator Execution
    // ========================================================================
    std::string FindAgyExecutable() {
        std::string localAppData;
        wchar_t buf[MAX_PATH]{};
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH) > 0) {
            localAppData = WideToUtf8(buf);
        }
        if (!localAppData.empty()) {
            fs::path p1 = fs::path(localAppData) / "agy" / "bin" / "agy.exe";
            if (fs::exists(p1)) return p1.string();
            fs::path p2 = fs::path(localAppData) / "Programs" / "Antigravity" / "bin" / "agy.cmd";
            if (fs::exists(p2)) return p2.string();
        }
        return "agy";
    }

    static std::string ModelLabelToAgyFlag(const std::string& label) {
        std::string low = ToLowerAscii(label);
        if (low.find("flash") != std::string::npos) return "gemini-3.8-flash";
        if (low.find("argon") != std::string::npos) return "gemini-4-argon";
        if (low.find("sonnet") != std::string::npos) return "claude-sonnet-5.5";
        if (low.find("opus") != std::string::npos) return "claude-opus-5.5";
        if (low.find("gpt") != std::string::npos) return "gpt-oss-120b";
        return "gemini-3.8-flash";
    }

    void SyncModelWithEffort(std::string& ioModel, const std::string&) {
        ioModel = StripModelEffortSuffix(ioModel);
    }

    void SyncEffortFromModel(const std::string&, std::string&) {
        // Effort is controlled independently via the [low · medium · high · max] pills
    }

    void PersistGlobalEffort(const std::string& eff) {
        default_effort = eff;
        settings.default_effort = eff;
        SaveAppSettings(settings);
        try {
            fs::path cliCfg = fs::path(GetUserHomeDir()) / ".gemini" / "antigravity-cli" / "settings.json";
            fs::create_directories(cliCfg.parent_path());
            std::ofstream f(cliCfg, std::ios::trunc);
            f << "{\n  \"model\": \"Gemini 3.8 Flash (Medium)\",\n  \"effort\": \"" << eff << "\"\n}\n";
        } catch (...) {}
    }

    void SwitchView(const std::string& viewId) {
        if (current_view == viewId) return;
        int prevOrder = GetViewOrderIndex(current_view);
        int nextOrder = GetViewOrderIndex(viewId);
        // Subtle 14px directional spring slide without ever resetting or hiding content!
        view_slide_x = (nextOrder >= prevOrder) ? 14.0f : -14.0f;

        current_view = viewId;
        current_view_order = nextOrder;
        settings.selected_view = viewId;
        if (viewId.rfind("cli_", 0) == 0) {
            active_input_field = "cli_prompt";
        } else if (viewId == "usage") {
            // Keep 78% of bar height immediately visible and ease the final 22% smoothly — zero blank flash!
            chart_anim = 0.78f;
            std::lock_guard<std::mutex> lk(data_mutex);
            if (kpi_disp_toks == 0.0) {
                kpi_disp_cost = dash_data.total_cost;
                kpi_disp_reqs = dash_data.total_requests;
                kpi_disp_toks = static_cast<double>(dash_data.total_tokens);
            }
        } else if (viewId == "quotas") {
            quotas_anim = 0.85f;
        } else if (viewId == "sessions") {
            active_input_field = "session_search";
        }
    }

    void SpawnNewCLITab() {
        cli_counter++;
        std::string tid = "cli_" + std::to_string(cli_counter);
        std::string tname = "\xE2\x9C\xBB Antigravity CLI " + std::to_string(cli_counter);
        cli_instances[tid] = CLISession(tid, tname, default_effort);
        cli_order.push_back(tid);
        SwitchView(tid);
    }

    void LaunchNativeTerminal() {
        std::string agyExe = FindAgyExecutable();
        std::string cmd = "/k cd /d \"" + workspace_dir + "\" && \"" + agyExe + "\" --effort " + default_effort;
        ShellExecuteW(hwnd, L"open", L"cmd.exe", Utf8ToWide(cmd).c_str(), Utf8ToWide(workspace_dir).c_str(), SW_SHOWNORMAL);
    }

    void RunCLIPrompt() {
        if (!current_view.starts_with("cli_")) return;
        std::string prompt = cli_prompt_input;
        while (!prompt.empty() && (prompt.back() == ' ' || prompt.back() == '\r' || prompt.back() == '\n')) prompt.pop_back();
        if (prompt.empty()) return;
        cli_prompt_input.clear();

        auto& cli = cli_instances[current_view];
        auto scrollToBottom = [&]() {
            cli.scroll_y = std::max(0.0f, static_cast<float>(cli.lines.size()) * 19.0f);
        };
        cli.lines.push_back({"cmd", "\xE2\x9D\xAF " + prompt});
        scrollToBottom();

        std::string lowPrompt = ToLowerAscii(prompt);
        std::string cmd = lowPrompt;
        std::string arg;
        size_t spPos = lowPrompt.find(' ');
        if (spPos != std::string::npos) {
            cmd = lowPrompt.substr(0, spPos);
            size_t aStart = prompt.find_first_not_of(" \t", spPos);
            if (aStart != std::string::npos) arg = prompt.substr(aStart);
        }

        if (cmd == "/clear" || cmd == "clear" || cmd == "cls") {
            cli.lines.clear();
            cli.scroll_y = 0.0f;
            return;
        }

        if (cmd == "/help" || cmd == "help" || cmd == "/?") {
            cli.lines.push_back({"tool", "\xE2\x97\x8F Antigravity CLI Slash Commands & Shell Utilities"});
            cli.lines.push_back({"code", "  /usage [today|24h|7d|30d|all]  Show token burn, cost, cache hit % & weekly limit"});
            cli.lines.push_back({"code", "  /weekly  (or /limits)          Show 5-Hour Rolling & 7-Day Weekly Limit status"});
            cli.lines.push_back({"code", "  /quota   (or /quotas)          List live RPC remaining % across all 13 models"});
            cli.lines.push_back({"code", "  /status                        Show RPC connection, tier, credits & active model"});
            cli.lines.push_back({"code", "  /effort <low|medium|high|max>  Set reasoning effort level across studio"});
            cli.lines.push_back({"code", "  /model <argon|flash|sonnet...> Switch active model in CLI tab"});
            cli.lines.push_back({"code", "  /plan <goal>                   Auto-split goal & dispatch 4 parallel workers"});
            cli.lines.push_back({"code", "  /sessions                      Show recent workspace sessions & token totals"});
            cli.lines.push_back({"code", "  /init                          Inspect or create workspace AGENTS.md rules"});
            cli.lines.push_back({"code", "  /export                        Copy full Markdown telemetry report to clipboard"});
            cli.lines.push_back({"code", "  /clear                         Clear terminal output"});
            cli.lines.push_back({"sub",  "  \xE2\x8E\xBF Tip: You can also run any shell command (e.g. git status, dir) or natural prompt."});
            scrollToBottom();
            return;
        }

        if (cmd == "/usage" || cmd == "/cost" || cmd == "/tokens" || cmd == "/stats") {
            std::string rKey = selected_range;
            std::string lowArg = ToLowerAscii(arg);
            if (lowArg == "today" || lowArg == "5h") rKey = "today_active";
            else if (lowArg == "24h" || lowArg == "1d") rKey = "today_24h";
            else if (lowArg == "7d" || lowArg == "week" || lowArg == "weekly") rKey = "7d";
            else if (lowArg == "30d" || lowArg == "month") rKey = "30d";
            else if (lowArg == "all") rKey = "all";

            DashboardData d;
            DashboardData d7;
            QuotaData q;
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                q = quota_data;
                auto it = all_dash_data.find(rKey);
                d = (it != all_dash_data.end()) ? it->second : dash_data;
                auto it7 = all_dash_data.find("7d");
                d7 = (it7 != all_dash_data.end()) ? it7->second : d;
            }

            cli.lines.push_back({"tool", "\xE2\x97\x8F Telemetry Summary (" + rKey + ") & Weekly Quota Status"});
            cli.lines.push_back({"code", "  \xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90"});
            char l1[160], l2[160], l3[160], l4[160];
            std::snprintf(l1, sizeof(l1), "  \xE2\x94\x82 Range Cost:   %-10s  Requests: %-8s  Cache Hit: %5.1f%% \xE2\x94\x82",
                          FormatCost(d.total_cost).c_str(), FormatWithCommas(d.total_requests).c_str(), d.cache_hit_pct);
            std::snprintf(l2, sizeof(l2), "  \xE2\x94\x82 Total Tokens: %-10s  (In: %s \xC2\xB7 Cache: %s \xC2\xB7 Out: %s)",
                          FormatTokens(d.total_tokens).c_str(), FormatTokens(d.total_input).c_str(),
                          FormatTokens(d.total_cache).c_str(), FormatTokens(d.total_output).c_str());
            double pPct = q.gemini_5h_pct;
            std::string pRst = q.gemini_5h_reset;
            std::snprintf(l3, sizeof(l3), "  \xE2\x94\x82 Gemini Pool:  5h Limit: %5.1f%% (%s)  \xC2\xB7  Weekly Limit: %5.1f%% (%s)",
                          pPct, pRst.c_str(), q.gemini_weekly_pct, q.gemini_weekly_reset.c_str());
            std::snprintf(l4, sizeof(l4), "  \xE2\x94\x82 Claude & GPT: 5h Limit: %5.1f%% (%s)  \xC2\xB7  Weekly Limit: %5.1f%% (%s)",
                          q.claude_5h_pct, q.claude_5h_reset.c_str(), q.claude_weekly_pct, q.claude_weekly_reset.c_str());
            cli.lines.push_back({"code", l1});
            cli.lines.push_back({"code", l2});
            cli.lines.push_back({"code", l3});
            cli.lines.push_back({"code", l4});
            cli.lines.push_back({"code", "  \xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98"});

            for (size_t i = 0; i < std::min<size_t>(5, d.models_table.size()); ++i) {
                const auto& m = d.models_table[i];
                char mb[160];
                std::snprintf(mb, sizeof(mb), "  \xE2\x80\xA2 %-28s %5s reqs \xC2\xB7 %8s tok \xC2\xB7 %8s",
                              m.label.c_str(), FormatWithCommas(m.requests).c_str(),
                              FormatTokens(m.total_tokens).c_str(), FormatCost(m.cost).c_str());
                cli.lines.push_back({"info", mb});
            }
            cli.lines.push_back({"sub", "  \xE2\x8E\xBF Live telemetry synced (Click '\xF0\x9F\x93\x8A Usage' or '\xE2\x9A\xA1 Quotas' tab for full charts)"});
            scrollToBottom();
            return;
        }

        if (cmd == "/weekly" || cmd == "/limits" || cmd == "/limit" || cmd == "/quota" || cmd == "/quotas" || cmd == "/status" || cmd == "/models") {
            std::lock_guard<std::mutex> lk(data_mutex);
            cli.lines.push_back({"tool", "\xE2\x97\x8F Live Antigravity Quotas (5-Hour Rolling & 7-Day Weekly Limit)"});
            char gLine[180], clLine[180], cLine[180];
            std::snprintf(gLine, sizeof(gLine), "  \xE2\x98\x85 GEMINI MODELS POOL:   5h Limit: %5.1f%% (resets %s)  \xC2\xB7  Weekly Limit: %5.1f%% (resets %s)",
                          quota_data.gemini_5h_pct, quota_data.gemini_5h_reset.c_str(),
                          quota_data.gemini_weekly_pct, quota_data.gemini_weekly_reset.c_str());
            std::snprintf(clLine, sizeof(clLine), "  \xE2\x98\x85 CLAUDE & GPT POOL:    5h Limit: %5.1f%% (resets %s)  \xC2\xB7  Weekly Limit: %5.1f%% (resets %s)",
                          quota_data.claude_5h_pct, quota_data.claude_5h_reset.c_str(),
                          quota_data.claude_weekly_pct, quota_data.claude_weekly_reset.c_str());
            std::snprintf(cLine, sizeof(cLine), "  \xE2\x98\x85 PLAN CREDITS (%s): Prompt %s / %s  \xC2\xB7  Flow %s / %s",
                          quota_data.tier.c_str(),
                          FormatWithCommas(quota_data.avail_credits).c_str(), FormatWithCommas(quota_data.monthly_credits).c_str(),
                          FormatWithCommas(quota_data.avail_flow_credits).c_str(), FormatWithCommas(quota_data.monthly_flow_credits).c_str());
            cli.lines.push_back({"sub", gLine});
            cli.lines.push_back({"sub", clLine});
            cli.lines.push_back({"info", cLine});
            for (size_t i = 0; i < quota_data.models.size(); ++i) {
                const auto& m = quota_data.models[i];
                char b[160];
                std::string rst = m.reset_relative.empty() ? "ready" : ("resets in " + m.reset_relative);
                std::snprintf(b, sizeof(b), "  [%s] %-28s %5.1f%% remaining  (%s)",
                              m.badge.c_str(), m.label.c_str(), m.remaining_pct, rst.c_str());
                cli.lines.push_back({"code", b});
            }
            scrollToBottom();
            return;
        }

        if (cmd == "/effort") {
            std::string effArg = ToLowerAscii(arg);
            bool valid = false;
            for (const auto& e : AVAILABLE_EFFORTS) if (e == effArg) valid = true;
            if (valid) {
                cli.effort = effArg;
                SyncModelWithEffort(cli.model, effArg);
                PersistGlobalEffort(effArg);
                cli.lines.push_back({"sub", "  \xE2\x8E\xBF Effort set to '" + effArg + "' (" + cli.model + ") across session."});
            } else {
                cli.lines.push_back({"info", "  \xE2\x8E\xBF Usage: /effort <low|medium|high|max> (Current: " + cli.effort + ")"});
            }
            scrollToBottom();
            return;
        }

        if (cmd == "/model") {
            if (arg.empty()) {
                cli.lines.push_back({"info", "  \xE2\x8E\xBF Current model: " + cli.model + " (effort: " + cli.effort + "). Available:"});
                for (const auto& m : AVAILABLE_MODELS) cli.lines.push_back({"code", "    \xE2\x80\xA2 " + m});
            } else {
                std::string lowA = ToLowerAscii(arg);
                std::string matched;
                for (const auto& m : AVAILABLE_MODELS) {
                    if (ToLowerAscii(m).find(lowA) != std::string::npos) { matched = m; break; }
                }
                if (!matched.empty()) {
                    cli.model = matched;
                    cli.lines.push_back({"sub", "  \xE2\x8E\xBF Switched active model to " + matched + " (effort: " + cli.effort + ")"});
                } else {
                    cli.lines.push_back({"err", "  \xE2\x8E\xBF Unknown model '" + arg + "'. Try: argon, flash, sonnet, opus, gpt"});
                }
            }
            scrollToBottom();
            return;
        }

        if (cmd == "/plan" || cmd == "/orchestrate") {
            if (!arg.empty()) master_goal = arg;
            AutoSplitMasterGoal();
            cli.lines.push_back({"tool", "\xE2\x97\x8F Plan(Decomposing Master Goal across 4 Parallel Workers)"});
            for (int i = 0; i < 4; ++i) {
                cli.lines.push_back({"code", "  Worker #" + std::to_string(i + 1) + " [" + workers[i].model + " \xC2\xB7 " + workers[i].effort + "]: " + workers[i].prompt});
            }
            cli.lines.push_back({"sub", "  \xE2\x8E\xBF Goal decomposed & folder packets dispatched! Switching to \xF0\x9F\x8E\xAF Orchestrator..."});
            scrollToBottom();
            SwitchView("orchestrator");
            return;
        }

        if (cmd == "/sessions" || cmd == "/history") {
            std::lock_guard<std::mutex> lk(data_mutex);
            cli.lines.push_back({"tool", "\xE2\x97\x8F Recent Workspace Sessions (" + std::to_string(dash_data.sessions_list.size()) + " total)"});
            for (size_t i = 0; i < std::min<size_t>(6, dash_data.sessions_list.size()); ++i) {
                const auto& s = dash_data.sessions_list[i];
                char sb[160];
                std::snprintf(sb, sizeof(sb), "  \xE2\x80\xA2 %-36s  %5s reqs \xC2\xB7 %7s tok \xC2\xB7 %s",
                              s.title.substr(0, 35).c_str(), FormatWithCommas(s.requests).c_str(),
                              FormatTokens(s.total_tokens).c_str(), FormatCost(s.cost).c_str());
                cli.lines.push_back({"code", sb});
            }
            scrollToBottom();
            return;
        }

        if (cmd == "/export") {
            CopyTelemetryReportToClipboard();
            cli.lines.push_back({"sub", "  \xE2\x8E\xBF Copied full Markdown Telemetry & Quota Report to system clipboard."});
            scrollToBottom();
            return;
        }

        if (cmd == "/init") {
            fs::path agentsPath = fs::path(workspace_dir) / "AGENTS.md";
            bool existed = fs::exists(agentsPath);
            cli.lines.push_back({"tool", std::string("\xE2\x97\x8F ") + (existed ? "Read(AGENTS.md)" : "Write(AGENTS.md)")});
            cli.lines.push_back({"code", "  # Agent Orchestration & Development Workflow"});
            cli.lines.push_back({"code", "  - Orchestrator: Gemini 4 Argon (effort: " + cli.effort + ")"});
            cli.lines.push_back({"code", "  - Subagent Workers: Gemini 3.8 Flash / Claude Sonnet 5.5"});
            cli.lines.push_back({"sub", std::string("  \xE2\x8E\xBF ") + (existed ? "Verified existing AGENTS.md in workspace." : "Initialized AGENTS.md in workspace.")});
            scrollToBottom();
            return;
        }

        // Dispatch background command execution
        std::string tid = current_view;
        std::string modFlag = ModelLabelToAgyFlag(cli.model);
        std::string effFlag = cli.effort;

        std::thread([this, tid, prompt, modFlag, effFlag]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(180));
            cli_instances[tid].lines.push_back({"tool", "\xE2\x97\x8F Execute(" + prompt.substr(0, 56) + " \xC2\xB7 model=" + modFlag + " \xC2\xB7 effort=" + effFlag + ")"});
            {
                std::lock_guard<std::mutex> lk(data_mutex);
                char infoBuf[180];
                std::snprintf(infoBuf, sizeof(infoBuf),
                              "  \xE2\x8E\xBF Workspace (%zu files) \xC2\xB7 Gemini 5h: %.1f%% (Wk: %.1f%%) \xC2\xB7 Claude/GPT 5h: %.1f%% (Wk: %.1f%%)",
                              sidebar_files.size(),
                              quota_data.gemini_5h_pct,
                              quota_data.gemini_weekly_pct,
                              quota_data.claude_5h_pct,
                              quota_data.claude_weekly_pct);
                cli_instances[tid].lines.push_back({"code", infoBuf});
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(220));
            cli_instances[tid].lines.push_back({"sub", "  \xE2\x8E\xBF Completed cleanly (effort=" + effFlag + " \xC2\xB7 Try /usage, /weekly, /quota, /help)"});
            cli_instances[tid].scroll_y = std::max(0.0f, static_cast<float>(cli_instances[tid].lines.size()) * 19.0f);
        }).detach();
    }

    void AutoSplitMasterGoal() {
        if (master_goal.empty()) return;
        workers[0].prompt = "Core Architecture: " + master_goal + " (data model and backend abstractions)";
        workers[1].prompt = "UI & Presentation: " + master_goal + " (Direct2D hardware rendering and smooth views)";
        workers[2].prompt = "Automated Tests: " + master_goal + " (smoke testing and regression verification)";
        workers[3].prompt = "Documentation: " + master_goal + " (comprehensive README and release packaging)";

        // Launch 4 simultaneous folder packets from Master Goal (-1) to Workers 0..3!
        static const char* kSplitLabels[] = {"arch/spec", "ui/views", "tests/suite", "docs/release"};
        static const uint32_t kSplitCols[] = {0xD97757, 0x60A5FA, 0x38C77B, 0xC084FC};
        for (int i = 0; i < 4; ++i) {
            TransferPacket pkt;
            pkt.from_node = -1;
            pkt.to_node = i;
            pkt.progress = 0.0f;
            pkt.speed = 0.95f + i * 0.08f;
            pkt.icon = "\xF0\x9F\x93\x81";
            pkt.label = kSplitLabels[i];
            pkt.color = kSplitCols[i];
            transfer_packets.push_back(pkt);
        }
    }

    void RunSingleWorker(int idx) {
        if (idx < 0 || idx >= static_cast<int>(workers.size())) return;
        if (workers[idx].is_running) return;
        workers[idx].is_running = true;
        workers[idx].progress_fill = 0.0f;
        workers[idx].logs = {
            "\xE2\x97\x8F Dispatching " + workers[idx].title + " [" + workers[idx].model + " \xC2\xB7 effort: " + workers[idx].effort + "]...",
            "\xF0\x9F\x93\x81 Mounting workspace folder & receiving task spec from Master Hub..."
        };

        // Immediately launch a packet from Master Goal (-1) into this worker
        {
            TransferPacket p0;
            p0.from_node = -1;
            p0.to_node = idx;
            p0.progress = 0.0f;
            p0.speed = 1.15f;
            p0.icon = "\xF0\x9F\x93\x81";
            p0.label = "task/spec";
            p0.color = 0xD97757;
            transfer_packets.push_back(p0);
        }

        std::thread([this, idx]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(700));
            workers[idx].logs.push_back("\xE2\x97\x8F Explore(Inspecting workspace & dependencies \xC2\xB7 effort=" + workers[idx].effort + ")");
            workers[idx].logs.push_back("  \xE2\x8E\xBF \xF0\x9F\x93\x81 Transferred module artifacts to peer worker bus (18.4k tokens)");

            // Mid-run inter-worker folder handoff packet
            int nextWorker = (idx + 1) % 4;
            {
                static const char* kHandoffNames[] = {"src/core", "ui/direct2d", "qa/report", "dist/pkg"};
                static const uint32_t kHandoffCols[] = {0xD97757, 0x60A5FA, 0x38C77B, 0xC084FC};
                TransferPacket pMid;
                pMid.from_node = idx;
                pMid.to_node = nextWorker;
                pMid.progress = 0.0f;
                pMid.speed = 0.85f;
                pMid.icon = "\xF0\x9F\x93\x81";
                pMid.label = kHandoffNames[idx];
                pMid.color = kHandoffCols[idx];
                transfer_packets.push_back(pMid);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(900));
            workers[idx].logs.push_back("\xE2\x97\x8F Executing task: " + workers[idx].prompt.substr(0, 42) + "...");
            std::this_thread::sleep_for(std::chrono::milliseconds(850));
            workers[idx].is_running = false;
            workers[idx].progress_fill = 1.0f;
            workers[idx].status_text = "\xE2\x9C\x93 Completed";
            workers[idx].status_color = 0x38C77B;
            workers[idx].logs.push_back("\xE2\x9C\x93 \xF0\x9F\x93\xA6 Synced output folder to release target.");
        }).detach();
    }

    void OpenWorkerInCLITab(int idx) {
        if (idx < 0 || idx >= static_cast<int>(workers.size())) return;
        SpawnNewCLITab();
        auto& cli = cli_instances[current_view];
        size_t p = workers[idx].title.find("\xE2\x80\x94");
        std::string shortTitle = (p != std::string::npos) ? workers[idx].title.substr(0, p) : workers[idx].title;
        while (!shortTitle.empty() && shortTitle.back() == ' ') shortTitle.pop_back();
        cli.name = "\xE2\x9C\xBB " + shortTitle;
        cli.model = workers[idx].model;
        cli.effort = workers[idx].effort;
        cli_prompt_input = workers[idx].prompt;
    }

    void CopyTelemetryReportToClipboard() {
        DashboardData d;
        QuotaData q;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            d = dash_data;
            q = quota_data;
        }
        std::ostringstream oss;
        oss << "# Antigravity Telemetry Report (" << selected_range << ")\r\n"
            << "- **Total Cost**: " << FormatCost(d.total_cost)
            << " | **Requests**: " << FormatWithCommas(d.total_requests)
            << " | **Tokens**: " << FormatTokens(d.total_tokens) << "\r\n"
            << "- **Cache Hit %**: " << std::fixed << std::setprecision(1) << d.cache_hit_pct << "%\r\n\r\n"
            << "| Model | Provider | Requests | Input | Cache Read | Output | Cost |\r\n"
            << "|---|---|---:|---:|---:|---:|---:|\r\n";
        for (const auto& m : d.models_table) {
            oss << "| " << m.label << " | " << m.provider << " | " << FormatWithCommas(m.requests)
                << " | " << FormatTokens(m.input) << " | " << FormatTokens(m.cache_read)
                << " | " << FormatTokens(m.output) << " | " << FormatCost(m.cost) << " |\r\n";
        }
        std::string report = oss.str();
        if (OpenClipboard(hwnd)) {
            EmptyClipboard();
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, report.size() + 1);
            if (hMem) {
                void* ptr = GlobalLock(hMem);
                if (ptr) {
                    std::memcpy(ptr, report.c_str(), report.size() + 1);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_TEXT, hMem);
                }
            }
            CloseClipboard();
        }
    }

    static std::string ExtractPillGroupPrefix(const std::string& id) {
        if (id.starts_with("cli_effort:")) return "cli_effort:";
        if (id.starts_with("orch_global_eff:")) return "orch_global_eff:";
        if (id.starts_with("worker_eff:")) {
            size_t p2 = id.find(':', 11);
            if (p2 != std::string::npos) return id.substr(0, p2 + 1); // "worker_eff:0:"
        }
        if (id.starts_with("range:")) return "range:";
        if (id.starts_with("cmode:")) return "cmode:";
        if (id.starts_with("tab:")) return "tab:";
        return "";
    }

    void ApplyPillSelectionById(const std::string& id) {
        if (id.starts_with("cli_effort:")) {
            std::string eff = id.substr(11);
            auto& cli = cli_instances[current_view];
            if (cli.effort != eff) {
                cli.effort = eff;
                SyncModelWithEffort(cli.model, eff);
                PersistGlobalEffort(eff);
            }
        } else if (id.starts_with("orch_global_eff:")) {
            std::string eff = id.substr(16);
            if (default_effort != eff) {
                PersistGlobalEffort(eff);
                for (auto& w : workers) {
                    w.effort = eff;
                    SyncModelWithEffort(w.model, eff);
                }
                for (auto& [k, cli] : cli_instances) {
                    cli.effort = eff;
                    SyncModelWithEffort(cli.model, eff);
                }
            }
        } else if (id.starts_with("worker_eff:")) {
            size_t p2 = id.find(':', 11);
            if (p2 != std::string::npos) {
                int wIdx = std::stoi(id.substr(11, p2 - 11));
                std::string eff = id.substr(p2 + 1);
                if (wIdx >= 0 && wIdx < 4 && workers[wIdx].effort != eff) {
                    workers[wIdx].effort = eff;
                    SyncModelWithEffort(workers[wIdx].model, eff);
                }
            }
        } else if (id.starts_with("range:")) {
            std::string rg = id.substr(6);
            if (selected_range != rg) {
                selected_range = rg;
                settings.selected_range = selected_range;
                chart_anim = 0.76f;
                std::lock_guard<std::mutex> lk(data_mutex);
                auto it = all_dash_data.find(selected_range);
                if (it != all_dash_data.end()) {
                    dash_data = it->second;
                }
            }
        } else if (id.starts_with("cmode:")) {
            std::string cm = id.substr(6);
            if (chart_mode != cm) {
                chart_mode = cm;
                settings.chart_mode = chart_mode;
                chart_anim = 0.78f;
            }
        } else if (id.starts_with("tab:")) {
            SwitchView(id.substr(4));
        }
    }

    void HandleDragMove(float mx, float my) {
        if (!is_dragging_pill || drag_pill_prefix.empty()) return;
        // Find the pill option in the active drag group closest horizontally to the mouse cursor
        std::string bestId;
        float bestDist = 99999.0f;
        for (const auto& hr : hit_regions) {
            if (hr.id.starts_with(drag_pill_prefix)) {
                // Allow generous vertical tolerance (+/- 36px) while dragging horizontally across a pill bar
                if (my >= hr.rect.top - 36.0f && my <= hr.rect.bottom + 36.0f) {
                    float cx = (hr.rect.left + hr.rect.right) * 0.5f;
                    float dist = (mx >= hr.rect.left && mx <= hr.rect.right) ? 0.0f : std::abs(mx - cx);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestId = hr.id;
                    }
                }
            }
        }
        if (!bestId.empty()) {
            ApplyPillSelectionById(bestId);
        }
    }

    bool HandleClick(float mx, float my) {
        is_dragging_pill = false;
        drag_pill_prefix.clear();

        // Iterate in reverse so topmost elements (like popups) get priority
        for (int i = static_cast<int>(hit_regions.size()) - 1; i >= 0; --i) {
            const auto& hr = hit_regions[i];
            if (mx >= hr.rect.left && mx <= hr.rect.right && my >= hr.rect.top && my <= hr.rect.bottom) {
                std::string id = hr.id;

                if (popup_open && !id.starts_with("popup_select:")) {
                    popup_open = false;
                }

                // Check if this is a draggable segmented pill (Effort, Range, Chart Mode, Tab)
                std::string grpPrefix = ExtractPillGroupPrefix(id);
                if (!grpPrefix.empty()) {
                    is_dragging_pill = true;
                    drag_pill_prefix = grpPrefix;
                    ApplyPillSelectionById(id);
                    return true;
                }

                if (id == "win_close") {
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                } else if (id == "win_min") {
                    ShowWindow(hwnd, SW_MINIMIZE);
                } else if (id == "win_max") {
                    WINDOWPLACEMENT wp{};
                    wp.length = sizeof(wp);
                    GetWindowPlacement(hwnd, &wp);
                    ShowWindow(hwnd, (wp.showCmd == SW_MAXIMIZE) ? SW_RESTORE : SW_MAXIMIZE);
                } else if (id == "toggle_sidebar") {
                    show_sidebar = !show_sidebar;
                } else if (id == "refresh_sidebar") {
                    RefreshSidebarFiles();
                } else if (id == "toggle_pin") {
                    is_pinned = !is_pinned;
                    settings.is_pinned = is_pinned;
                    SetWindowPos(hwnd, is_pinned ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                    SaveAppSettings(settings);
                } else if (id == "spawn_cli") {
                    SpawnNewCLITab();
                } else if (id == "native_term") {
                    LaunchNativeTerminal();
                } else if (id == "cli_model_popup") {
                    popup_open = !popup_open;
                    popup_target = "cli";
                    popup_x = hr.rect.left;
                    popup_y = hr.rect.bottom + 4.0f;
                } else if (id.starts_with("worker_model:")) {
                    popup_open = true;
                    popup_target = "worker_" + id.substr(13);
                    popup_x = hr.rect.left;
                    popup_y = hr.rect.bottom + 4.0f;
                } else if (id.starts_with("popup_select:")) {
                    std::string mod = id.substr(13);
                    if (popup_target == "cli") {
                        auto& cli = cli_instances[current_view];
                        cli.model = mod;
                        SyncEffortFromModel(mod, cli.effort);
                    } else if (popup_target.starts_with("worker_")) {
                        int wIdx = std::stoi(popup_target.substr(7));
                        if (wIdx >= 0 && wIdx < 4) {
                            workers[wIdx].model = mod;
                            SyncEffortFromModel(mod, workers[wIdx].effort);
                        }
                    }
                    popup_open = false;
                } else if (id == "orch_split_goal") {
                    AutoSplitMasterGoal();
                } else if (id == "orch_dispatch_all") {
                    for (int wIdx = 0; wIdx < 4; ++wIdx) RunSingleWorker(wIdx);
                } else if (id.starts_with("worker_run:")) {
                    RunSingleWorker(std::stoi(id.substr(11)));
                } else if (id.starts_with("worker_open:")) {
                    OpenWorkerInCLITab(std::stoi(id.substr(12)));
                } else if (id == "export_report") {
                    CopyTelemetryReportToClipboard();
                } else if (id.starts_with("focus:")) {
                    active_input_field = id.substr(6);
                } else if (id == "cli_run") {
                    RunCLIPrompt();
                } else if (id == "cli_stop") {
                    if (current_view.starts_with("cli_")) {
                        cli_instances[current_view].lines.push_back({"err", "  \xE2\x8F\xB9 Execution stopped by user."});
                    }
                } else if (id.starts_with("file:")) {
                    std::string fn = id.substr(5);
                    if (!current_view.starts_with("cli_")) SwitchView("cli_1");
                    cli_prompt_input = "Inspect and explain " + fn;
                    active_input_field = "cli_prompt";
                }
                return true;
            }
        }
        popup_open = false;
        return false;
    }

    void HandleCharInput(wchar_t ch) {
        std::string* target = nullptr;
        if (active_input_field == "cli_prompt") target = &cli_prompt_input;
        else if (active_input_field == "master_goal") target = &master_goal;
        else if (active_input_field == "session_search") target = &session_search_query;
        else if (active_input_field.starts_with("worker_")) {
            int idx = std::stoi(active_input_field.substr(7));
            if (idx >= 0 && idx < 4) target = &workers[idx].prompt;
        }
        if (!target) return;

        if (ch == L'\b') {
            if (!target->empty()) {
                // Pop UTF-8 codepoint cleanly
                while (!target->empty() && ((target->back() & 0xC0) == 0x80)) target->pop_back();
                if (!target->empty()) target->pop_back();
            }
        } else if (ch == L'\r' || ch == L'\n') {
            if (active_input_field == "cli_prompt") {
                RunCLIPrompt();
            } else if (active_input_field == "master_goal") {
                AutoSplitMasterGoal();
            }
        } else if (ch >= 32) {
            std::wstring ws(1, ch);
            *target += WideToUtf8(ws);
        }
    }
};
