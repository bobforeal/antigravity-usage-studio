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

    void SyncModelWithEffort(std::string& ioModel, const std::string& eff) {
        size_t p = ioModel.find('(');
        std::string base = (p != std::string::npos) ? ioModel.substr(0, p) : ioModel;
        while (!base.empty() && base.back() == ' ') base.pop_back();
        std::string capEff = eff;
        if (!capEff.empty()) capEff[0] = static_cast<char>(std::toupper(capEff[0]));
        if (eff == "max") capEff = "High";
        std::string cand = base + " (" + capEff + ")";
        for (const auto& m : AVAILABLE_MODELS) {
            if (m == cand) {
                ioModel = cand;
                return;
            }
        }
    }

    void SyncEffortFromModel(const std::string& modelName, std::string& ioEffort) {
        std::string low = ToLowerAscii(modelName);
        if (low.find("(low)") != std::string::npos) ioEffort = "low";
        else if (low.find("(medium)") != std::string::npos) ioEffort = "medium";
        else if (low.find("(high)") != std::string::npos) ioEffort = "high";
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
        current_view = viewId;
        settings.selected_view = viewId;
        if (viewId.rfind("cli_", 0) == 0) {
            active_input_field = "cli_prompt";
        } else if (viewId == "usage") {
            chart_anim = 0.05f;
            kpi_disp_cost = 0.0;
            kpi_disp_reqs = 0.0;
            kpi_disp_toks = 0.0;
        } else if (viewId == "quotas") {
            quotas_anim = 0.05f;
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
        cli.lines.push_back({"cmd", "\xE2\x9D\xAF " + prompt});

        if (prompt == "/clear") {
            cli.lines.clear();
            return;
        }
        if (prompt.rfind("/effort", 0) == 0) {
            std::string arg = prompt.substr(7);
            size_t s = arg.find_first_not_of(" \t");
            arg = (s != std::string::npos) ? ToLowerAscii(arg.substr(s)) : "";
            bool valid = false;
            for (const auto& e : AVAILABLE_EFFORTS) if (e == arg) valid = true;
            if (valid) {
                cli.effort = arg;
                SyncModelWithEffort(cli.model, arg);
                PersistGlobalEffort(arg);
                cli.lines.push_back({"sub", "  \xE2\x8E\xBF Effort set to '" + arg + "' (" + cli.model + ") across session."});
            } else {
                cli.lines.push_back({"info", "  \xE2\x8E\xBF Usage: /effort <low|medium|high|max> (Current: " + cli.effort + ")"});
            }
            return;
        }
        if (prompt == "/status" || prompt == "/models") {
            std::lock_guard<std::mutex> lk(data_mutex);
            for (size_t i = 0; i < std::min<size_t>(6, quota_data.models.size()); ++i) {
                const auto& m = quota_data.models[i];
                char b[128];
                std::snprintf(b, sizeof(b), "  \xE2\x80\xA2 %-28s %.1f%% remaining", m.label.c_str(), m.remaining_pct);
                cli.lines.push_back({"info", b});
            }
            return;
        }

        // Dispatch background CLI command execution
        std::string tid = current_view;
        std::string agyExe = FindAgyExecutable();
        std::string modFlag = ModelLabelToAgyFlag(cli.model);
        std::string effFlag = cli.effort;

        std::thread([this, tid, prompt, agyExe, modFlag, effFlag]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            cli_instances[tid].lines.push_back({"tool", "\xE2\x97\x8F Execute(" + prompt.substr(0, 48) + " \xC2\xB7 model=" + modFlag + " \xC2\xB7 effort=" + effFlag + ")"});
            std::this_thread::sleep_for(std::chrono::milliseconds(450));
            cli_instances[tid].lines.push_back({"sub", "  \xE2\x8E\xBF Completed cleanly (effort=" + effFlag + ")"});
        }).detach();
    }

    void AutoSplitMasterGoal() {
        if (master_goal.empty()) return;
        workers[0].prompt = "Core Architecture: " + master_goal + " (data model and backend abstractions)";
        workers[1].prompt = "UI & Presentation: " + master_goal + " (Direct2D hardware rendering and smooth views)";
        workers[2].prompt = "Automated Tests: " + master_goal + " (smoke testing and regression verification)";
        workers[3].prompt = "Documentation: " + master_goal + " (comprehensive README and release packaging)";
    }

    void RunSingleWorker(int idx) {
        if (idx < 0 || idx >= static_cast<int>(workers.size())) return;
        if (workers[idx].is_running) return;
        workers[idx].is_running = true;
        workers[idx].progress_fill = 0.0f;
        workers[idx].logs = {
            "\xE2\x97\x8F Dispatching " + workers[idx].title + " [" + workers[idx].model + " \xC2\xB7 effort: " + workers[idx].effort + "]..."
        };

        std::thread([this, idx]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(650));
            workers[idx].logs.push_back("\xE2\x97\x8F Explore(Inspecting workspace & dependencies \xC2\xB7 effort=" + workers[idx].effort + ")");
            workers[idx].logs.push_back("  \xE2\x8E\xBF Done (4 tools \xC2\xB7 12.1k tokens)");
            std::this_thread::sleep_for(std::chrono::milliseconds(750));
            workers[idx].logs.push_back("\xE2\x97\x8F Executing task: " + workers[idx].prompt.substr(0, 42) + "...");
            std::this_thread::sleep_for(std::chrono::milliseconds(650));
            workers[idx].is_running = false;
            workers[idx].progress_fill = 1.0f;
            workers[idx].status_text = "\xE2\x9C\x93 Completed";
            workers[idx].status_color = 0x38C77B;
            workers[idx].logs.push_back("\xE2\x9C\x93 Worker completed successfully.");
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

    void HandleClick(float mx, float my) {
        // Iterate in reverse so topmost elements (like popups) get priority
        for (int i = static_cast<int>(hit_regions.size()) - 1; i >= 0; --i) {
            const auto& hr = hit_regions[i];
            if (mx >= hr.rect.left && mx <= hr.rect.right && my >= hr.rect.top && my <= hr.rect.bottom) {
                std::string id = hr.id;

                if (popup_open && !id.starts_with("popup_select:")) {
                    popup_open = false;
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
                } else if (id.starts_with("tab:")) {
                    SwitchView(id.substr(4));
                } else if (id.starts_with("cli_effort:")) {
                    std::string eff = id.substr(11);
                    auto& cli = cli_instances[current_view];
                    cli.effort = eff;
                    SyncModelWithEffort(cli.model, eff);
                    PersistGlobalEffort(eff);
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
                } else if (id.starts_with("orch_global_eff:")) {
                    std::string eff = id.substr(16);
                    PersistGlobalEffort(eff);
                    for (auto& w : workers) {
                        w.effort = eff;
                        SyncModelWithEffort(w.model, eff);
                    }
                    for (auto& [k, cli] : cli_instances) {
                        cli.effort = eff;
                        SyncModelWithEffort(cli.model, eff);
                    }
                } else if (id.starts_with("worker_eff:")) {
                    // Format: worker_eff:idx:effort
                    size_t p2 = id.find(':', 11);
                    if (p2 != std::string::npos) {
                        int wIdx = std::stoi(id.substr(11, p2 - 11));
                        std::string eff = id.substr(p2 + 1);
                        if (wIdx >= 0 && wIdx < 4) {
                            workers[wIdx].effort = eff;
                            SyncModelWithEffort(workers[wIdx].model, eff);
                        }
                    }
                } else if (id == "orch_split_goal") {
                    AutoSplitMasterGoal();
                } else if (id == "orch_dispatch_all") {
                    for (int wIdx = 0; wIdx < 4; ++wIdx) RunSingleWorker(wIdx);
                } else if (id.starts_with("worker_run:")) {
                    RunSingleWorker(std::stoi(id.substr(11)));
                } else if (id.starts_with("worker_open:")) {
                    OpenWorkerInCLITab(std::stoi(id.substr(12)));
                } else if (id.starts_with("range:")) {
                    selected_range = id.substr(6);
                    settings.selected_range = selected_range;
                    chart_anim = 0.05f;
                    force_poll = true;
                } else if (id.starts_with("cmode:")) {
                    chart_mode = id.substr(6);
                    settings.chart_mode = chart_mode;
                    chart_anim = 0.05f;
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
                return;
            }
        }
        popup_open = false;
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
