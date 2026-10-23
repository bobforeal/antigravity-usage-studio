#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <winhttp.h>
#include <winternl.h>
#include <winsock2.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// ============================================================================
// Lightweight Embedded JSON Value / Parser
// ============================================================================
struct JsonVal {
    enum Type { NUL, BOOL_T, NUM_T, STR_T, ARR_T, OBJ_T } type = NUL;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<JsonVal> a;
    std::map<std::string, JsonVal> o;

    bool is_null() const { return type == NUL; }
    bool is_obj() const { return type == OBJ_T; }
    bool is_arr() const { return type == ARR_T; }
    bool is_str() const { return type == STR_T; }
    bool is_num() const { return type == NUM_T; }

    const JsonVal& get(const std::string& key) const {
        static const JsonVal empty;
        if (type != OBJ_T) return empty;
        auto it = o.find(key);
        return (it != o.end()) ? it->second : empty;
    }

    std::string str(const std::string& key, const std::string& def = "") const {
        const auto& v = get(key);
        return (v.type == STR_T) ? v.s : def;
    }

    double num(const std::string& key, double def = 0.0) const {
        const auto& v = get(key);
        if (v.type == NUM_T) return v.n;
        if (v.type == STR_T) {
            try { return std::stod(v.s); } catch (...) {}
        }
        return def;
    }

    long long integer(const std::string& key, long long def = 0) const {
        const auto& v = get(key);
        if (v.type == NUM_T) return static_cast<long long>(v.n);
        if (v.type == STR_T) {
            try { return std::stoll(v.s); } catch (...) {}
        }
        return def;
    }

    bool boolean(const std::string& key, bool def = false) const {
        const auto& v = get(key);
        if (v.type == BOOL_T) return v.b;
        return def;
    }
};

class MiniJsonParser {
    const std::string& src;
    size_t pos = 0;

    void skip_ws() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n')) {
            ++pos;
        }
    }

    std::string parse_str() {
        std::string out;
        if (pos >= src.size() || src[pos] != '"') return out;
        ++pos;
        while (pos < src.size()) {
            char c = src[pos++];
            if (c == '"') return out;
            if (c == '\\' && pos < src.size()) {
                char esc = src[pos++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        if (pos + 4 <= src.size()) {
                            unsigned int cp = 0;
                            for (int i = 0; i < 4; ++i) {
                                char hc = src[pos++];
                                cp <<= 4;
                                if (hc >= '0' && hc <= '9') cp |= (hc - '0');
                                else if (hc >= 'a' && hc <= 'f') cp |= (hc - 'a' + 10);
                                else if (hc >= 'A' && hc <= 'F') cp |= (hc - 'A' + 10);
                            }
                            if (cp < 0x80) {
                                out.push_back(static_cast<char>(cp));
                            } else if (cp < 0x800) {
                                out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                            } else {
                                out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                            }
                        }
                        break;
                    }
                    default: out.push_back(esc); break;
                }
            } else {
                out.push_back(c);
            }
        }
        return out;
    }

public:
    explicit MiniJsonParser(const std::string& s) : src(s) {}

    JsonVal parse_val() {
        skip_ws();
        JsonVal v;
        if (pos >= src.size()) return v;
        char c = src[pos];
        if (c == '"') {
            v.type = JsonVal::STR_T;
            v.s = parse_str();
        } else if (c == '{') {
            v.type = JsonVal::OBJ_T;
            ++pos;
            skip_ws();
            if (pos < src.size() && src[pos] == '}') { ++pos; return v; }
            while (pos < src.size()) {
                skip_ws();
                if (pos >= src.size() || src[pos] != '"') break;
                std::string k = parse_str();
                skip_ws();
                if (pos < src.size() && src[pos] == ':') ++pos;
                v.o[k] = parse_val();
                skip_ws();
                if (pos < src.size() && src[pos] == ',') { ++pos; continue; }
                if (pos < src.size() && src[pos] == '}') { ++pos; break; }
                break;
            }
        } else if (c == '[') {
            v.type = JsonVal::ARR_T;
            ++pos;
            skip_ws();
            if (pos < src.size() && src[pos] == ']') { ++pos; return v; }
            while (pos < src.size()) {
                v.a.push_back(parse_val());
                skip_ws();
                if (pos < src.size() && src[pos] == ',') { ++pos; continue; }
                if (pos < src.size() && src[pos] == ']') { ++pos; break; }
                break;
            }
        } else if (c == 't' && src.compare(pos, 4, "true") == 0) {
            v.type = JsonVal::BOOL_T;
            v.b = true;
            pos += 4;
        } else if (c == 'f' && src.compare(pos, 5, "false") == 0) {
            v.type = JsonVal::BOOL_T;
            v.b = false;
            pos += 5;
        } else if (c == 'n' && src.compare(pos, 4, "null") == 0) {
            v.type = JsonVal::NUL;
            pos += 4;
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            v.type = JsonVal::NUM_T;
            size_t start = pos;
            if (src[pos] == '-') ++pos;
            while (pos < src.size() && ((src[pos] >= '0' && src[pos] <= '9') || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-')) {
                ++pos;
            }
            try { v.n = std::stod(src.substr(start, pos - start)); } catch (...) { v.n = 0.0; }
        } else {
            ++pos;
        }
        return v;
    }

    static JsonVal parse(const std::string& s) {
        MiniJsonParser p(s);
        return p.parse_val();
    }
};

// ============================================================================
// String / Formatting Helpers
// ============================================================================
inline std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int sz = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (sz <= 0) return L"";
    std::wstring res(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), res.data(), sz);
    return res;
}

inline std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sz = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (sz <= 0) return "";
    std::string res(sz, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), res.data(), sz, nullptr, nullptr);
    return res;
}

inline std::string ToLowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

inline std::string FormatWithCommas(long long n) {
    if (n < 0) return "-" + FormatWithCommas(-n);
    std::string s = std::to_string(n);
    int insertPosition = static_cast<int>(s.length()) - 3;
    while (insertPosition > 0) {
        s.insert(insertPosition, ",");
        insertPosition -= 3;
    }
    return s;
}

inline std::string FormatTokens(long long n) {
    if (n <= 0) return "0";
    char buf[64];
    if (n >= 1000000000LL) {
        std::snprintf(buf, sizeof(buf), "%.3fB", static_cast<double>(n) / 1000000000.0);
        return buf;
    }
    if (n >= 1000000LL) {
        std::snprintf(buf, sizeof(buf), "%.1fM", static_cast<double>(n) / 1000000.0);
        return buf;
    }
    if (n >= 1000LL) {
        std::snprintf(buf, sizeof(buf), "%.1fk", static_cast<double>(n) / 1000.0);
        return buf;
    }
    return FormatWithCommas(n);
}

inline std::string FormatCost(double c) {
    if (c <= 0.0) return "$0.00";
    if (c < 0.01) return "<$0.01";
    long long whole = static_cast<long long>(c);
    int cents = static_cast<int>(std::round((c - whole) * 100.0));
    if (cents >= 100) { whole += 1; cents -= 100; }
    char buf[32];
    std::snprintf(buf, sizeof(buf), ".%02d", cents);
    return "$" + FormatWithCommas(whole) + buf;
}

inline void GetModelProviderAndBadge(const std::string& label, std::string& provider, std::string& badge) {
    std::string low = ToLowerAscii(label);
    if (low.find("claude") != std::string::npos || low.find("opus") != std::string::npos || low.find("sonnet") != std::string::npos) {
        provider = "Anthropic";
        badge = "CL";
    } else if (low.find("gpt") != std::string::npos || low.find("oss") != std::string::npos) {
        provider = "OpenAI";
        badge = "OA";
    } else {
        provider = "Google";
        badge = "GO";
    }
}

inline double ComputeModelCost(const std::string& label, long long inp, long long cache, long long out) {
    std::string low = ToLowerAscii(label);
    double inRate = 0.30, cacheRate = 0.075, outRate = 1.20;
    if (low.find("opus") != std::string::npos) {
        inRate = 5.00; cacheRate = 0.50; outRate = 25.00;
    } else if (low.find("sonnet") != std::string::npos) {
        inRate = 3.00; cacheRate = 0.30; outRate = 15.00;
    } else if (low.find("argon") != std::string::npos) {
        if (low.find("high") != std::string::npos) {
            inRate = 1.25; cacheRate = 0.3125; outRate = 5.00;
        } else if (low.find("medium") != std::string::npos) {
            inRate = 0.80; cacheRate = 0.20; outRate = 3.20;
        } else {
            inRate = 0.50; cacheRate = 0.125; outRate = 2.00;
        }
    } else if (low.find("gpt-oss") != std::string::npos) {
        inRate = 0.60; cacheRate = 0.15; outRate = 2.40;
    }
    return (inp / 1000000.0) * inRate + (cache / 1000000.0) * cacheRate + (out / 1000000.0) * outRate;
}

inline std::string CleanSessionTitle(const std::string& rawText, const std::string& cid) {
    std::string fallback = "Session " + cid.substr(0, std::min<size_t>(8, cid.size()));
    if (rawText.empty()) return fallback;
    std::string txt = rawText;

    auto extractTag = [&](const std::string& openTag, const std::string& closeTag) {
        size_t p1 = txt.find(openTag);
        if (p1 != std::string::npos) {
            p1 += openTag.size();
            while (p1 < txt.size() && (txt[p1] == ' ' || txt[p1] == '\r' || txt[p1] == '\n' || txt[p1] == '\t')) ++p1;
            size_t p2 = txt.find(closeTag, p1);
            size_t pNl = txt.find('\n', p1);
            size_t endP = std::min(p2, pNl);
            if (endP != std::string::npos) txt = txt.substr(p1, endP - p1);
            else txt = txt.substr(p1);
        }
    };

    extractTag("<USER_REQUEST>", "</USER_REQUEST>");
    extractTag("<original_task>", "</original_task>");

    if (txt.find("[Message]") != std::string::npos) {
        size_t pc = txt.find("content=");
        if (pc != std::string::npos) {
            pc += 8;
            size_t pnl = txt.find('\n', pc);
            txt = (pnl != std::string::npos) ? txt.substr(pc, pnl - pc) : txt.substr(pc);
        }
    }

    std::string filtered;
    for (char c : txt) {
        if (c != '*' && c != '_' && c != '#' && c != '`') filtered.push_back(c);
    }

    std::istringstream iss(filtered);
    std::string line;
    while (std::getline(iss, line)) {
        size_t s = line.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        size_t e = line.find_last_not_of(" \t\r\n");
        std::string cleaned = line.substr(s, e - s + 1);
        for (const char* prefix : {"Task:", "Objective:", "Prompt:", "task:", "objective:", "prompt:"}) {
            size_t plen = std::strlen(prefix);
            if (cleaned.compare(0, plen, prefix) == 0) {
                cleaned = cleaned.substr(plen);
                size_t s2 = cleaned.find_first_not_of(" \t");
                cleaned = (s2 != std::string::npos) ? cleaned.substr(s2) : "";
            }
        }
        if (cleaned.size() > 52) {
            cleaned = cleaned.substr(0, 49) + "...";
        }
        if (!cleaned.empty()) return cleaned;
    }
    return fallback;
}

// ============================================================================
// Data Models
// ============================================================================
struct QuotaModelInfo {
    std::string label;
    std::string model_id;
    std::string family;
    std::string provider;
    std::string badge;
    double remaining_pct = 100.0;
    double used_pct = 0.0;
    double remaining_fraction = 1.0;
    std::string reset_time_raw;
    std::string reset_relative;
    bool is_recommended = false;
};

struct QuotaData {
    bool connected = false;
    std::string tier = "Connecting...";
    std::string user_name;
    std::string email;
    std::vector<QuotaModelInfo> models;
    std::map<std::string, QuotaModelInfo> models_by_label;
    QuotaModelInfo primary_model;
    bool has_primary = false;
    QuotaModelInfo secondary_model;
    bool has_secondary = false;
    long long avail_credits = 500;
    long long monthly_credits = 50000;
    bool has_credits = false;
};

struct HourlySlot {
    int hour = 0;
    std::string label;
    std::string display;
    double cost = 0.0;
    long long tokens = 0;
    int requests = 0;
};

struct HeatmapDayCell {
    std::string date_str;
    std::string display_date;
    int weekday = 0;
    long long tokens = 0;
    int steps = 0;
    double cost = 0.0;
    int intensity = 0;
    bool is_future = false;
    bool is_today = false;
    std::string month_label;
};

struct HeatmapData {
    std::vector<std::vector<HeatmapDayCell>> weeks;
    int total_steps = 0;
    long long total_tokens = 0;
    int active_days = 0;
    int streak = 0;
    long long max_daily_tokens = 0;
};

struct ModelUsageRow {
    std::string label;
    std::string provider;
    std::string badge;
    int requests = 0;
    long long input = 0;
    long long cache_read = 0;
    long long output = 0;
    long long total_tokens = 0;
    double cost = 0.0;
    bool has_quota = false;
    double remaining_pct = 100.0;
    std::string reset_relative;
};

struct SessionRow {
    std::string session_id;
    std::string title;
    std::string model;
    std::string provider;
    std::string badge;
    int step_count = 0;
    int requests = 0;
    long long input_tokens = 0;
    long long cache_tokens = 0;
    long long output_tokens = 0;
    long long total_tokens = 0;
    double cost = 0.0;
    std::string last_modified_full;
    bool is_active_recent = false;
};

struct DashboardData {
    std::string selected_range = "today_active";
    double total_cost = 0.0;
    int total_requests = 0;
    long long total_tokens = 0;
    long long total_input = 0;
    long long total_cache = 0;
    long long total_output = 0;
    double cache_hit_pct = 0.0;
    std::vector<HourlySlot> hourly_slots;
    HeatmapData heatmap;
    std::vector<ModelUsageRow> models_table;
    int models_count = 0;
    int providers_count = 1;
    std::vector<SessionRow> sessions_list;
};

struct AppSettings {
    std::string selected_view = "cli_1";
    std::string selected_range = "today_active";
    std::string chart_mode = "cost";
    std::string default_effort = "medium";
    bool is_pinned = false;
    int window_w = 1140;
    int window_h = 780;
    int window_x = -1;
    int window_y = -1;
};

// ============================================================================
// Settings Load / Save
// ============================================================================
inline std::string GetAppDirectory() {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    fs::path p(buf);
    return p.parent_path().string();
}

inline std::string GetUserHomeDir() {
    wchar_t buf[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH) > 0) {
        return WideToUtf8(buf);
    }
    return "C:\\Users\\Default";
}

inline AppSettings LoadAppSettings() {
    AppSettings s;
    fs::path p = fs::path(GetAppDirectory()) / "hud_settings.json";
    if (fs::exists(p)) {
        try {
            std::ifstream f(p);
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            JsonVal j = MiniJsonParser::parse(content);
            if (j.is_obj()) {
                s.selected_view = j.str("selected_view", s.selected_view);
                s.selected_range = j.str("selected_range", s.selected_range);
                s.chart_mode = j.str("chart_mode", s.chart_mode);
                s.default_effort = j.str("default_effort", s.default_effort);
                s.is_pinned = j.boolean("is_pinned", s.is_pinned);
                s.window_w = static_cast<int>(j.integer("window_w", s.window_w));
                s.window_h = static_cast<int>(j.integer("window_h", s.window_h));
                s.window_x = static_cast<int>(j.integer("window_x", -1));
                s.window_y = static_cast<int>(j.integer("window_y", -1));
            }
        } catch (...) {}
    }
    return s;
}

inline void SaveAppSettings(const AppSettings& s) {
    try {
        fs::path p = fs::path(GetAppDirectory()) / "hud_settings.json";
        std::ofstream f(p, std::ios::trunc);
        f << "{\n"
          << "  \"selected_view\": \"" << s.selected_view << "\",\n"
          << "  \"selected_range\": \"" << s.selected_range << "\",\n"
          << "  \"chart_mode\": \"" << s.chart_mode << "\",\n"
          << "  \"default_effort\": \"" << s.default_effort << "\",\n"
          << "  \"is_pinned\": " << (s.is_pinned ? "true" : "false") << ",\n"
          << "  \"window_w\": " << s.window_w << ",\n"
          << "  \"window_h\": " << s.window_h << ",\n"
          << "  \"window_x\": " << s.window_x << ",\n"
          << "  \"window_y\": " << s.window_y << "\n"
          << "}\n";
    } catch (...) {}
}

// ============================================================================
// Live RPC Client (WinHTTP + NtQueryInformationProcess + IPHelper)
// ============================================================================
class AntigravityLiveRPC {
    inline static int cached_port = 0;
    inline static bool cached_https = true;
    inline static std::string cached_token;
    inline static JsonVal last_successful_data;
    inline static double last_success_time = 0.0;

    typedef NTSTATUS(NTAPI* PFN_NtQueryInformationProcess)(
        HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

    static std::wstring GetProcCmdLine(DWORD pid) {
        std::wstring result;
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProc) return result;
        HMODULE hNt = GetModuleHandleW(L"ntdll.dll");
        if (hNt) {
            auto fn = reinterpret_cast<PFN_NtQueryInformationProcess>(GetProcAddress(hNt, "NtQueryInformationProcess"));
            if (fn) {
                ULONG len = 0;
                fn(hProc, static_cast<PROCESSINFOCLASS>(60), nullptr, 0, &len);
                if (len > 0 && len < 65536) {
                    std::vector<BYTE> buf(len);
                    if (fn(hProc, static_cast<PROCESSINFOCLASS>(60), buf.data(), len, &len) == 0) {
                        auto* us = reinterpret_cast<UNICODE_STRING*>(buf.data());
                        if (us->Buffer && us->Length > 0) {
                            result.assign(us->Buffer, us->Length / sizeof(WCHAR));
                        }
                    }
                }
            }
        }
        CloseHandle(hProc);
        return result;
    }

    static void FindServerAndToken(std::vector<int>& outPorts, std::string& outToken) {
        std::set<DWORD> pids;
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe{};
            pe.dwSize = sizeof(pe);
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    std::wstring exe = pe.szExeFile;
                    std::wstring exeLow = exe;
                    for (auto& c : exeLow) c = towlower(c);
                    std::wstring cmd = GetProcCmdLine(pe.th32ProcessID);
                    std::wstring cmdLow = cmd;
                    for (auto& c : cmdLow) c = towlower(c);

                    if (exeLow.find(L"antigravity") != std::wstring::npos ||
                        exeLow.find(L"language_server") != std::wstring::npos ||
                        cmdLow.find(L"antigravity") != std::wstring::npos ||
                        cmdLow.find(L"language_server") != std::wstring::npos) {
                        pids.insert(pe.th32ProcessID);
                        if (outToken.empty() && !cmd.empty()) {
                            std::string utf8Cmd = WideToUtf8(cmd);
                            size_t pos = utf8Cmd.find("--csrf_token");
                            if (pos != std::string::npos) {
                                pos += 12;
                                if (pos < utf8Cmd.size() && (utf8Cmd[pos] == '=' || utf8Cmd[pos] == ' ')) ++pos;
                                while (pos < utf8Cmd.size() && utf8Cmd[pos] == ' ') ++pos;
                                size_t end = pos;
                                while (end < utf8Cmd.size() && utf8Cmd[end] != ' ' && utf8Cmd[end] != '"' && utf8Cmd[end] != '\'') ++end;
                                if (end > pos) outToken = utf8Cmd.substr(pos, end - pos);
                            }
                        }
                    }
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }

        std::set<int> portSet;
        ULONG tcpSize = 0;
        GetExtendedTcpTable(nullptr, &tcpSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
        if (tcpSize > 0) {
            std::vector<BYTE> tcpBuf(tcpSize);
            if (GetExtendedTcpTable(tcpBuf.data(), &tcpSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) == NO_ERROR) {
                auto* tbl = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(tcpBuf.data());
                for (DWORD i = 0; i < tbl->dwNumEntries; ++i) {
                    if (pids.count(tbl->table[i].dwOwningPid)) {
                        int port = ntohs(static_cast<u_short>(tbl->table[i].dwLocalPort));
                        portSet.insert(port);
                    }
                }
            }
        }
        outPorts.assign(portSet.begin(), portSet.end());
    }

    static bool CallEndpoint(bool useHttps, int port, const std::string& csrfToken, JsonVal& outJson) {
        HINTERNET hSession = WinHttpOpen(L"AntigravityUsageStudio/2.5", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        WinHttpSetTimeouts(hSession, 1500, 1500, 1500, 1500);
        HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", static_cast<INTERNET_PORT>(port), 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"POST",
            L"/exa.language_server_pb.LanguageServerService/GetUserStatus",
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags
        );
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        if (useHttps) {
            DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                             SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                             SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                             SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
        }

        std::wstring headers = L"Content-Type: application/json\r\nConnect-Protocol-Version: 1\r\n";
        if (!csrfToken.empty()) {
            headers += L"X-Codeium-Csrf-Token: " + Utf8ToWide(csrfToken) + L"\r\n";
        }

        static const char* kPayload = "{\"metadata\":{\"ideName\":\"antigravity\",\"extensionName\":\"antigravity\",\"locale\":\"en\"}}";
        DWORD payloadLen = static_cast<DWORD>(std::strlen(kPayload));

        bool ok = false;
        if (WinHttpSendRequest(hRequest, headers.c_str(), static_cast<DWORD>(headers.size()), (LPVOID)kPayload, payloadLen, payloadLen, 0) &&
            WinHttpReceiveResponse(hRequest, nullptr)) {
            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
            if (statusCode == 200) {
                std::string body;
                DWORD avail = 0;
                while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
                    std::vector<char> chunk(avail);
                    DWORD read = 0;
                    if (WinHttpReadData(hRequest, chunk.data(), avail, &read) && read > 0) {
                        body.append(chunk.data(), read);
                    } else {
                        break;
                    }
                }
                if (!body.empty()) {
                    outJson = MiniJsonParser::parse(body);
                    ok = outJson.is_obj();
                }
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return ok;
    }

public:
    static bool FetchLiveUserStatus(JsonVal& outData) {
        double nowSec = static_cast<double>( std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()) / 1000.0;

        if (cached_port > 0 && !cached_token.empty()) {
            if (CallEndpoint(cached_https, cached_port, cached_token, outData)) {
                last_successful_data = outData;
                last_success_time = nowSec;
                return true;
            }
            if (CallEndpoint(!cached_https, cached_port, cached_token, outData)) {
                cached_https = !cached_https;
                last_successful_data = outData;
                last_success_time = nowSec;
                return true;
            }
        }

        std::vector<int> ports;
        std::string token;
        FindServerAndToken(ports, token);
        if (!token.empty()) cached_token = token;

        for (int port : ports) {
            for (bool https : {true, false}) {
                if (CallEndpoint(https, port, cached_token, outData)) {
                    cached_port = port;
                    cached_https = https;
                    last_successful_data = outData;
                    last_success_time = nowSec;
                    return true;
                }
            }
        }

        if (last_successful_data.is_obj() && (nowSec - last_success_time < 30.0)) {
            outData = last_successful_data;
            return true;
        }
        return false;
    }
};

// ============================================================================
// Telemetry & Session Data Collector
// ============================================================================
struct ParsedEvent {
    long long ts_epoch = 0;
    std::string date_str;
    int hour_int = 0;
    std::string model_label;
    long long inp = 0;
    long long cache = 0;
    long long out = 0;
    double cost = 0.0;
};

struct ParsedSession {
    std::string id;
    long long mtime = 0;
    uintmax_t t_size = 0;
    std::string last_modified_full;
    std::string date_str;
    std::set<std::string> sess_dates;
    std::string detected_model = "Gemini 3.8 Flash (Medium)";
    std::string first_prompt;
    bool is_active_recent = false;
    int step_count = 0;
    int requests = 0;
    long long input_tokens = 0;
    long long cache_tokens = 0;
    long long output_tokens = 0;
    long long total_tokens = 0;
    double cost = 0.0;
    std::vector<ParsedEvent> events;
};

class AntigravityDataCollector {
    inline static std::unordered_map<std::string, ParsedSession> session_cache;
    inline static std::string latest_detected_model;
    inline static std::mutex cache_mutex;

    static long long ParseIsoTimestamp(const std::string& iso, std::string& outDateStr, int& outHour) {
        // Format: YYYY-MM-DDTHH:MM:SS...
        if (iso.size() < 19) return 0;
        int Y = 0, M = 0, D = 0, h = 0, m = 0, s = 0;
        if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) < 6) return 0;

        std::tm tmUtc{};
        tmUtc.tm_year = Y - 1900;
        tmUtc.tm_mon = M - 1;
        tmUtc.tm_mday = D;
        tmUtc.tm_hour = h;
        tmUtc.tm_min = m;
        tmUtc.tm_sec = s;

        time_t epoch = _mkgmtime(&tmUtc);
        if (epoch == -1) return 0;

        // Check timezone offset if not Z
        size_t tzPos = iso.find_first_of("+-Z", 19);
        if (tzPos != std::string::npos && iso[tzPos] != 'Z') {
            int tzH = 0, tzM = 0;
            if (std::sscanf(iso.c_str() + tzPos + 1, "%d:%d", &tzH, &tzM) >= 1) {
                int offsetSec = tzH * 3600 + tzM * 60;
                if (iso[tzPos] == '+') epoch -= offsetSec;
                else epoch += offsetSec;
            }
        }

        std::tm tmLocal{};
        localtime_s(&tmLocal, &epoch);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tmLocal.tm_year + 1900, tmLocal.tm_mon + 1, tmLocal.tm_mday);
        outDateStr = buf;
        outHour = tmLocal.tm_hour;
        return static_cast<long long>(epoch);
    }

    static std::string ComputeRelativeReset(const std::string& resetIso) {
        if (resetIso.empty() || resetIso.size() < 19) return "";
        int Y = 0, M = 0, D = 0, h = 0, m = 0, s = 0;
        if (std::sscanf(resetIso.c_str(), "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) < 6) return "";
        std::tm tmUtc{};
        tmUtc.tm_year = Y - 1900;
        tmUtc.tm_mon = M - 1;
        tmUtc.tm_mday = D;
        tmUtc.tm_hour = h;
        tmUtc.tm_min = m;
        tmUtc.tm_sec = s;
        time_t resetEpoch = _mkgmtime(&tmUtc);
        time_t nowEpoch = std::time(nullptr);
        long long diff = static_cast<long long>(resetEpoch - nowEpoch);
        if (diff <= 0) return "soon";
        long long hours = diff / 3600;
        long long mins = (diff % 3600) / 60;
        if (hours > 0) return std::to_string(hours) + "h " + std::to_string(mins) + "m";
        return std::to_string(mins) + "m";
    }

    // Fast scanner for a single JSONL line to avoid full DOM parse on lines without tokens or settings
    static bool ExtractJsonStringField(const std::string& line, const char* keyPattern, std::string& outVal) {
        size_t pos = line.find(keyPattern);
        if (pos == std::string::npos) return false;
        pos = line.find(':', pos);
        if (pos == std::string::npos) return false;
        ++pos;
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
        if (pos >= line.size() || line[pos] != '"') return false;
        ++pos;
        outVal.clear();
        while (pos < line.size()) {
            char c = line[pos++];
            if (c == '"') return true;
            if (c == '\\' && pos < line.size()) {
                char esc = line[pos++];
                if (esc == 'n') outVal.push_back('\n');
                else if (esc == 't') outVal.push_back('\t');
                else if (esc == '"') outVal.push_back('"');
                else if (esc == '\\') outVal.push_back('\\');
                else outVal.push_back(esc);
            } else {
                outVal.push_back(c);
            }
        }
        return false;
    }

    static bool ExtractJsonIntField(const std::string& line, const char* keyPattern, long long& outVal) {
        size_t pos = line.find(keyPattern);
        if (pos == std::string::npos) return false;
        pos = line.find(':', pos);
        if (pos == std::string::npos) return false;
        ++pos;
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
        if (pos >= line.size()) return false;
        if (line[pos] == '"') ++pos;
        if (pos >= line.size() || (line[pos] < '0' || line[pos] > '9')) return false;
        long long v = 0;
        while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
            v = v * 10 + (line[pos] - '0');
            ++pos;
        }
        outVal = v;
        return true;
    }

    static ParsedSession ParseSessionDirectory(const fs::path& sdir) {
        std::string key = sdir.string();
        std::string cid = sdir.filename().string();
        fs::path transcriptPath = sdir / ".system_generated" / "logs" / "transcript.jsonl";

        long long dirMtime = 0;
        long long tMtime = 0;
        uintmax_t tSize = 0;
        bool hasTranscript = false;

        try {
            if (fs::exists(sdir)) {
                auto ftime = fs::last_write_time(sdir);
                auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                dirMtime = sctp.time_since_epoch().count();
            }
            if (fs::exists(transcriptPath)) {
                hasTranscript = true;
                tSize = fs::file_size(transcriptPath);
                auto ftime = fs::last_write_time(transcriptPath);
                auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                tMtime = sctp.time_since_epoch().count();
            }
        } catch (...) {}

        long long effectiveMtime = std::max(dirMtime, tMtime);
        long long nowEpoch = static_cast<long long>(std::time(nullptr));

        {
            std::lock_guard<std::mutex> lk(cache_mutex);
            auto it = session_cache.find(key);
            if (it != session_cache.end() && it->second.mtime == effectiveMtime && it->second.t_size == tSize) {
                it->second.is_active_recent = (nowEpoch - effectiveMtime) < 60;
                return it->second;
            }
        }

        ParsedSession ps;
        ps.id = cid;
        ps.mtime = effectiveMtime;
        ps.t_size = tSize;
        ps.is_active_recent = (nowEpoch - effectiveMtime) < 60;

        time_t effTimeT = static_cast<time_t>(effectiveMtime);
        std::tm tmFallback{};
        localtime_s(&tmFallback, &effTimeT);
        char dateBuf[32];
        std::snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d", tmFallback.tm_year + 1900, tmFallback.tm_mon + 1, tmFallback.tm_mday);
        ps.date_str = dateBuf;
        char fullBuf[32];
        std::snprintf(fullBuf, sizeof(fullBuf), "%04d-%02d-%02d %02d:%02d",
                      tmFallback.tm_year + 1900, tmFallback.tm_mon + 1, tmFallback.tm_mday, tmFallback.tm_hour, tmFallback.tm_min);
        ps.last_modified_full = fullBuf;

        std::string currentModel = "Gemini 3.8 Flash (Medium)";
        std::string detectedModel;

        if (hasTranscript) {
            std::ifstream f(transcriptPath);
            std::string line;
            while (std::getline(f, line)) {
                if (line.empty()) continue;
                ps.step_count++;

                if (ps.first_prompt.empty()) {
                    if (line.find("<USER_REQUEST>") != std::string::npos ||
                        line.find("\"USER_EXPLICIT\"") != std::string::npos ||
                        line.find("\"USER_INPUT\"") != std::string::npos) {
                        std::string content;
                        if (ExtractJsonStringField(line, "\"content\"", content) && !content.empty()) {
                            ps.first_prompt = content;
                        }
                    }
                }

                if (line.find("<USER_SETTINGS_CHANGE>") != std::string::npos) {
                    std::string content;
                    if (ExtractJsonStringField(line, "\"content\"", content) && content.find("<USER_SETTINGS_CHANGE>") != std::string::npos) {
                        size_t pm = content.find("Model Selection` from ");
                        if (pm != std::string::npos) {
                            size_t pto = content.find(" to ", pm);
                            if (pto != std::string::npos) {
                                pto += 4;
                                size_t pnl = content.find('\n', pto);
                                size_t endDot = (pnl != std::string::npos) ? pnl : content.size();
                                while (endDot > pto && (content[endDot - 1] == '\r' || content[endDot - 1] == ' ' || content[endDot - 1] == '.')) {
                                    --endDot;
                                }
                                std::string val = content.substr(pto, endDot - pto);
                                if (!val.empty() && val.size() <= 40 && val.find('\\') == std::string::npos &&
                                    val.find('[') == std::string::npos && ToLowerAscii(val) != "none") {
                                    currentModel = val;
                                    detectedModel = val;
                                }
                            }
                        }
                    }
                }

                long long inp = 0, out = 0, cache = 0;
                bool hasInp = ExtractJsonIntField(line, "\"input_tokens\"", inp);
                bool hasOut = ExtractJsonIntField(line, "\"output_tokens\"", out);
                bool hasCache = ExtractJsonIntField(line, "\"cache_read_tokens\"", cache);

                std::string stepDateStr = ps.date_str;
                int stepHour = tmFallback.tm_hour;
                long long stepTs = effectiveMtime;

                std::string createdAt;
                if (ExtractJsonStringField(line, "\"created_at\"", createdAt)) {
                    long long parsedTs = ParseIsoTimestamp(createdAt, stepDateStr, stepHour);
                    if (parsedTs > 0) stepTs = parsedTs;
                }
                ps.sess_dates.insert(stepDateStr);

                if (hasInp || hasOut || hasCache) {
                    double cVal = ComputeModelCost(currentModel, inp, cache, out);
                    ps.requests++;
                    ps.input_tokens += inp;
                    ps.cache_tokens += cache;
                    ps.output_tokens += out;
                    ps.total_tokens += (inp + cache + out);
                    ps.cost += cVal;
                    ps.events.push_back({stepTs, stepDateStr, stepHour, currentModel, inp, cache, out, cVal});
                }
            }
        }

        ps.detected_model = detectedModel.empty() ? currentModel : detectedModel;

        {
            std::lock_guard<std::mutex> lk(cache_mutex);
            session_cache[key] = ps;
        }
        return ps;
    }

    static std::unordered_map<std::string, std::string> LoadDbSessionTitles() {
        std::unordered_map<std::string, std::string> titles;
        fs::path dbPath = fs::path(GetUserHomeDir()) / ".gemini" / "antigravity" / "conversation_summaries.db";
        if (!fs::exists(dbPath)) return titles;

        HMODULE hSql = LoadLibraryW(L"winsqlite3.dll");
        if (!hSql) return titles;

        typedef struct sqlite3 sqlite3;
        typedef struct sqlite3_stmt sqlite3_stmt;
        typedef int(__cdecl* PFN_open)(const char*, sqlite3**, int, const char*);
        typedef int(__cdecl* PFN_prep)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
        typedef int(__cdecl* PFN_step)(sqlite3_stmt*);
        typedef const unsigned char* (__cdecl* PFN_text)(sqlite3_stmt*, int);
        typedef int(__cdecl* PFN_fin)(sqlite3_stmt*);
        typedef int(__cdecl* PFN_close)(sqlite3*);

        auto open_fn = reinterpret_cast<PFN_open>(GetProcAddress(hSql, "sqlite3_open_v2"));
        auto prep_fn = reinterpret_cast<PFN_prep>(GetProcAddress(hSql, "sqlite3_prepare_v2"));
        auto step_fn = reinterpret_cast<PFN_step>(GetProcAddress(hSql, "sqlite3_step"));
        auto text_fn = reinterpret_cast<PFN_text>(GetProcAddress(hSql, "sqlite3_column_text"));
        auto fin_fn  = reinterpret_cast<PFN_fin>(GetProcAddress(hSql, "sqlite3_finalize"));
        auto close_fn= reinterpret_cast<PFN_close>(GetProcAddress(hSql, "sqlite3_close"));

        if (open_fn && prep_fn && step_fn && text_fn && fin_fn && close_fn) {
            sqlite3* db = nullptr;
            std::string pathUtf8 = dbPath.string();
            if (open_fn(pathUtf8.c_str(), &db, 1 /*SQLITE_OPEN_READONLY*/, nullptr) == 0 && db) {
                sqlite3_stmt* stmt = nullptr;
                if (prep_fn(db, "SELECT conversation_id, title, preview FROM conversation_summaries", -1, &stmt, nullptr) == 0 && stmt) {
                    while (step_fn(stmt) == 100 /*SQLITE_ROW*/) {
                        const char* cid = reinterpret_cast<const char*>(text_fn(stmt, 0));
                        const char* title = reinterpret_cast<const char*>(text_fn(stmt, 1));
                        const char* preview = reinterpret_cast<const char*>(text_fn(stmt, 2));
                        if (cid) {
                            std::string t = title ? title : "";
                            if (t.empty() && preview) t = preview;
                            if (!t.empty()) titles[cid] = t;
                        }
                    }
                    fin_fn(stmt);
                }
                close_fn(db);
            }
        }
        FreeLibrary(hSql);
        return titles;
    }

public:
    static QuotaData GetRealQuotaData() {
        QuotaData q;
        JsonVal raw;
        if (!AntigravityLiveRPC::FetchLiveUserStatus(raw)) {
            q.connected = false;
            q.tier = "Reconnecting...";
            return q;
        }

        q.connected = true;
        const JsonVal& us = raw.get("userStatus");
        q.tier = us.get("userTier").str("name", "Google AI Ultra");
        q.user_name = us.str("name", "");
        q.email = us.str("email", "");

        const JsonVal& ps = us.get("planStatus");
        if (ps.is_obj()) {
            q.has_credits = true;
            q.avail_credits = ps.integer("availablePromptCredits", 500);
            q.monthly_credits = ps.get("planInfo").integer("monthlyPromptCredits", 50000);
        }

        const JsonVal& configs = us.get("cascadeModelConfigData").get("clientModelConfigs");
        if (configs.is_arr()) {
            for (const auto& cfg : configs.a) {
                QuotaModelInfo m;
                m.label = cfg.str("label", "");
                if (m.label.empty()) continue;
                m.model_id = cfg.str("modelId", "");
                const JsonVal& qi = cfg.get("quotaInfo");
                m.remaining_fraction = qi.is_obj() ? qi.num("remainingFraction", 1.0) : 1.0;
                m.remaining_pct = std::round(m.remaining_fraction * 1000.0) / 10.0;
                m.used_pct = std::round((1.0 - m.remaining_fraction) * 1000.0) / 10.0;
                m.reset_time_raw = qi.str("resetTime", "");
                m.reset_relative = ComputeRelativeReset(m.reset_time_raw);
                m.is_recommended = cfg.boolean("isRecommended", false);

                GetModelProviderAndBadge(m.label, m.provider, m.badge);
                std::string low = ToLowerAscii(m.label);
                if (low.find("gemini") != std::string::npos || low.find("argon") != std::string::npos || low.find("flash") != std::string::npos) {
                    m.family = "gemini";
                } else if (low.find("claude") != std::string::npos || low.find("sonnet") != std::string::npos || low.find("opus") != std::string::npos) {
                    m.family = "claude";
                } else {
                    m.family = "other";
                }

                q.models.push_back(m);
                q.models_by_label[m.label] = m;
            }
        }

        // Deterministic sort order so models never shuffle across polls
        auto effortRank = [](const std::string& lbl) {
            std::string low = ToLowerAscii(lbl);
            if (low.find("(high)") != std::string::npos) return 0;
            if (low.find("(medium)") != std::string::npos) return 1;
            if (low.find("(low)") != std::string::npos) return 2;
            return 3;
        };
        std::sort(q.models.begin(), q.models.end(), [&](const QuotaModelInfo& a, const QuotaModelInfo& b) {
            if (a.provider != b.provider) return a.provider < b.provider;
            std::string baseA = a.label.substr(0, a.label.find('('));
            std::string baseB = b.label.substr(0, b.label.find('('));
            if (baseA != baseB) return baseA < baseB;
            int rA = effortRank(a.label), rB = effortRank(b.label);
            if (rA != rB) return rA < rB;
            return a.label < b.label;
        });

        if (!latest_detected_model.empty() && q.models_by_label.count(latest_detected_model)) {
            q.primary_model = q.models_by_label[latest_detected_model];
            q.has_primary = true;
        }
        if (!q.has_primary) {
            for (const auto& m : q.models) {
                if (m.remaining_fraction < 0.9999) {
                    q.primary_model = m;
                    q.has_primary = true;
                    break;
                }
            }
            if (!q.has_primary && !q.models.empty()) {
                q.primary_model = q.models[0];
                q.has_primary = true;
            }
        }
        if (q.has_primary) {
            std::string targetFam = (q.primary_model.family == "gemini") ? "claude" : "gemini";
            for (const auto& m : q.models) {
                if (m.family == targetFam) {
                    q.secondary_model = m;
                    q.has_secondary = true;
                    break;
                }
            }
        }
        return q;
    }

    static DashboardData GetDashboardData(const std::string& selectedRange, const QuotaData& quotaData) {
        DashboardData d;
        d.selected_range = selectedRange;

        time_t nowEpoch = std::time(nullptr);
        std::tm tmNow{};
        localtime_s(&tmNow, &nowEpoch);
        char todayBuf[16];
        std::snprintf(todayBuf, sizeof(todayBuf), "%04d-%02d-%02d", tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);
        std::string todayStr = todayBuf;

        fs::path brainDir = fs::path(GetUserHomeDir()) / ".gemini" / "antigravity" / "brain";
        std::vector<ParsedSession> parsedSessions;
        if (fs::exists(brainDir)) {
            try {
                for (const auto& entry : fs::directory_iterator(brainDir)) {
                    if (entry.is_directory() && entry.path().filename() != "tempmediaStorage") {
                        parsedSessions.push_back(ParseSessionDirectory(entry.path()));
                    }
                }
            } catch (...) {}
        }

        std::sort(parsedSessions.begin(), parsedSessions.end(), [](const ParsedSession& a, const ParsedSession& b) {
            return a.mtime > b.mtime;
        });

        if (!parsedSessions.empty() && !parsedSessions[0].detected_model.empty()) {
            latest_detected_model = parsedSessions[0].detected_model;
        }

        // 24 rolling hourly slots
        std::tm tmHour = tmNow;
        tmHour.tm_min = 0;
        tmHour.tm_sec = 0;
        time_t curHourEpoch = std::mktime(&tmHour);

        std::map<int, size_t> slotIdxByHour;
        for (int i = 23; i >= 0; --i) {
            time_t slotEpoch = curHourEpoch - i * 3600;
            std::tm tmSlot{};
            localtime_s(&tmSlot, &slotEpoch);
            HourlySlot hs;
            hs.hour = tmSlot.tm_hour;
            hs.label = std::to_string(hs.hour) + ":00";
            char dispBuf[16];
            std::snprintf(dispBuf, sizeof(dispBuf), "%02d:00", hs.hour);
            hs.display = dispBuf;
            slotIdxByHour[hs.hour] = d.hourly_slots.size();
            d.hourly_slots.push_back(hs);
        }

        long long cutoff24h = static_cast<long long>(nowEpoch) - 86400;
        long long cutoff7d  = static_cast<long long>(nowEpoch) - 7 * 86400;
        long long cutoff30d = static_cast<long long>(nowEpoch) - 30 * 86400;

        struct DailyStat { long long tokens = 0; int steps = 0; double cost = 0.0; };
        std::unordered_map<std::string, DailyStat> dailyStats;
        std::map<std::string, ModelUsageRow> modelsAgg;

        for (const auto& s : parsedSessions) {
            bool isTodaySess = (s.date_str == todayStr) || (s.sess_dates.count(todayStr) > 0);

            for (const auto& ev : s.events) {
                auto& ds = dailyStats[ev.date_str];
                ds.tokens += (ev.inp + ev.cache + ev.out);
                ds.steps += 1;
                ds.cost += ev.cost;
            }

            if (selectedRange == "today_active" && !isTodaySess) continue;

            for (const auto& ev : s.events) {
                if (selectedRange == "today_24h" && ev.ts_epoch < cutoff24h) continue;
                if (selectedRange == "7d" && ev.ts_epoch < cutoff7d) continue;
                if (selectedRange == "30d" && ev.ts_epoch < cutoff30d) continue;

                long long tok = ev.inp + ev.cache + ev.out;
                d.total_cost += ev.cost;
                d.total_requests += 1;
                d.total_input += ev.inp;
                d.total_cache += ev.cache;
                d.total_output += ev.out;
                d.total_tokens += tok;

                auto& mRow = modelsAgg[ev.model_label];
                mRow.label = ev.model_label;
                mRow.requests += 1;
                mRow.input += ev.inp;
                mRow.cache_read += ev.cache;
                mRow.output += ev.out;
                mRow.total_tokens += tok;
                mRow.cost += ev.cost;

                auto itSlot = slotIdxByHour.find(ev.hour_int);
                if (itSlot != slotIdxByHour.end()) {
                    auto& slot = d.hourly_slots[itSlot->second];
                    slot.cost += ev.cost;
                    slot.tokens += tok;
                    slot.requests += 1;
                }
            }
        }

        long long cacheDenom = d.total_input + d.total_cache;
        d.cache_hit_pct = (cacheDenom > 0) ? (std::round((static_cast<double>(d.total_cache) / cacheDenom) * 1000.0) / 10.0) : 0.0;

        // Build 52-week Heatmap
        int todayWday = (tmNow.tm_wday + 6) % 7; // Monday = 0 .. Sunday = 6
        std::tm tmMonday = tmNow;
        tmMonday.tm_hour = 12; tmMonday.tm_min = 0; tmMonday.tm_sec = 0;
        time_t mondayEpoch = std::mktime(&tmMonday) - todayWday * 86400;
        time_t startEpoch = mondayEpoch - 51LL * 7 * 86400;

        long long maxDailyTok = 0;
        for (int w = 0; w < 52; ++w) {
            for (int dayIdx = 0; dayIdx < 7; ++dayIdx) {
                time_t cellEpoch = startEpoch + (w * 7LL + dayIdx) * 86400;
                if (cellEpoch <= nowEpoch + 43200) {
                    std::tm tmCell{};
                    localtime_s(&tmCell, &cellEpoch);
                    char cBuf[16];
                    std::snprintf(cBuf, sizeof(cBuf), "%04d-%02d-%02d", tmCell.tm_year + 1900, tmCell.tm_mon + 1, tmCell.tm_mday);
                    auto itD = dailyStats.find(cBuf);
                    if (itD != dailyStats.end() && itD->second.tokens > maxDailyTok) {
                        maxDailyTok = itD->second.tokens;
                    }
                }
            }
        }
        d.heatmap.max_daily_tokens = maxDailyTok;

        static const char* kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        for (int w = 0; w < 52; ++w) {
            std::vector<HeatmapDayCell> weekCells;
            for (int dayIdx = 0; dayIdx < 7; ++dayIdx) {
                time_t cellEpoch = startEpoch + (w * 7LL + dayIdx) * 86400;
                std::tm tmCell{};
                localtime_s(&tmCell, &cellEpoch);
                char cBuf[16];
                std::snprintf(cBuf, sizeof(cBuf), "%04d-%02d-%02d", tmCell.tm_year + 1900, tmCell.tm_mon + 1, tmCell.tm_mday);

                HeatmapDayCell cell;
                cell.date_str = cBuf;
                char dispDate[32];
                std::snprintf(dispDate, sizeof(dispDate), "%s %02d, %04d", kMonths[tmCell.tm_mon], tmCell.tm_mday, tmCell.tm_year + 1900);
                cell.display_date = dispDate;
                cell.weekday = dayIdx;
                cell.is_today = (cell.date_str == todayStr);
                cell.is_future = (cell.date_str > todayStr);

                auto itD = dailyStats.find(cell.date_str);
                if (itD != dailyStats.end()) {
                    cell.tokens = itD->second.tokens;
                    cell.steps = itD->second.steps;
                    cell.cost = itD->second.cost;
                }

                if (!cell.is_future) {
                    d.heatmap.total_tokens += cell.tokens;
                    d.heatmap.total_steps += cell.steps;
                    if (cell.tokens > 0 || cell.steps > 0) d.heatmap.active_days++;
                }

                if (cell.is_future || (cell.tokens == 0 && cell.steps == 0)) {
                    cell.intensity = 0;
                } else if (maxDailyTok > 0) {
                    double pct = static_cast<double>(cell.tokens) / maxDailyTok;
                    if (pct < 0.15) cell.intensity = 1;
                    else if (pct < 0.40) cell.intensity = 2;
                    else if (pct < 0.75) cell.intensity = 3;
                    else cell.intensity = 4;
                } else {
                    cell.intensity = 1;
                }

                if (tmCell.tm_mday == 1 || (dayIdx == 0 && tmCell.tm_mday <= 7)) {
                    cell.month_label = kMonths[tmCell.tm_mon];
                }
                weekCells.push_back(cell);
            }
            d.heatmap.weeks.push_back(weekCells);
        }

        // Streak calculation
        int streak = 0;
        time_t checkEpoch = std::mktime(&tmMonday) + todayWday * 86400;
        auto itToday = dailyStats.find(todayStr);
        if (itToday != dailyStats.end() && (itToday->second.tokens > 0 || itToday->second.steps > 0)) {
            streak++;
        }
        checkEpoch -= 86400;
        for (int back = 0; back < 365; ++back) {
            std::tm tmCk{};
            localtime_s(&tmCk, &checkEpoch);
            char ckBuf[16];
            std::snprintf(ckBuf, sizeof(ckBuf), "%04d-%02d-%02d", tmCk.tm_year + 1900, tmCk.tm_mon + 1, tmCk.tm_mday);
            auto itCk = dailyStats.find(ckBuf);
            if (itCk != dailyStats.end() && (itCk->second.tokens > 0 || itCk->second.steps > 0)) {
                streak++;
                checkEpoch -= 86400;
            } else {
                break;
            }
        }
        d.heatmap.streak = streak;

        // Models Table
        std::set<std::string> providersSeen;
        for (auto& [lbl, mRow] : modelsAgg) {
            GetModelProviderAndBadge(lbl, mRow.provider, mRow.badge);
            providersSeen.insert(mRow.provider);
            auto itQ = quotaData.models_by_label.find(lbl);
            if (itQ != quotaData.models_by_label.end()) {
                mRow.has_quota = true;
                mRow.remaining_pct = itQ->second.remaining_pct;
                mRow.reset_relative = itQ->second.reset_relative;
            }
            d.models_table.push_back(mRow);
        }

        std::sort(d.models_table.begin(), d.models_table.end(), [](const ModelUsageRow& a, const ModelUsageRow& b) {
            if (std::abs(a.cost - b.cost) > 1e-6) return a.cost > b.cost;
            return a.total_tokens > b.total_tokens;
        });

        static const char* kKeyModels[] = {
            "Gemini 4 Argon (High)",
            "Gemini 4 Argon (Medium)",
            "Gemini 3.8 Flash (Medium)",
            "Claude Sonnet 5.5 (High)",
            "Claude Opus 5.5 (High)",
            "GPT-OSS 120B (Medium)"
        };
        for (const char* fallbackLbl : kKeyModels) {
            if (d.models_table.size() >= 8) break;
            bool exists = false;
            for (const auto& m : d.models_table) {
                if (m.label == fallbackLbl) { exists = true; break; }
            }
            if (!exists) {
                auto itQ = quotaData.models_by_label.find(fallbackLbl);
                if (itQ != quotaData.models_by_label.end()) {
                    ModelUsageRow row;
                    row.label = fallbackLbl;
                    row.provider = itQ->second.provider;
                    row.badge = itQ->second.badge;
                    row.has_quota = true;
                    row.remaining_pct = itQ->second.remaining_pct;
                    row.reset_relative = itQ->second.reset_relative;
                    providersSeen.insert(row.provider);
                    d.models_table.push_back(row);
                }
            }
        }
        d.models_count = static_cast<int>(d.models_table.size());
        if (d.models_table.size() > 8) d.models_table.resize(8);
        d.providers_count = std::max(1, static_cast<int>(providersSeen.size()));

        // Sessions Table (Top 40)
        auto dbTitles = LoadDbSessionTitles();
        size_t maxSess = std::min<size_t>(40, parsedSessions.size());
        for (size_t i = 0; i < maxSess; ++i) {
            const auto& ps = parsedSessions[i];
            SessionRow sr;
            sr.session_id = ps.id;
            std::string rawTitle;
            auto itT = dbTitles.find(ps.id);
            if (itT != dbTitles.end()) rawTitle = itT->second;
            else if (!ps.first_prompt.empty()) rawTitle = ps.first_prompt;
            sr.title = CleanSessionTitle(rawTitle, ps.id);
            sr.model = ps.detected_model;
            GetModelProviderAndBadge(sr.model, sr.provider, sr.badge);
            sr.step_count = ps.step_count;
            sr.requests = ps.requests;
            sr.input_tokens = ps.input_tokens;
            sr.cache_tokens = ps.cache_tokens;
            sr.output_tokens = ps.output_tokens;
            sr.total_tokens = ps.total_tokens;
            sr.cost = ps.cost;
            sr.last_modified_full = ps.last_modified_full;
            sr.is_active_recent = ps.is_active_recent;
            d.sessions_list.push_back(sr);
        }

        return d;
    }
};
