// ============================================================================
// Antigravity Usage Studio — Native C++20 / Direct2D / DirectWrite Edition
// Hardware-Accelerated 60fps Desktop Studio & Multi-Agent Orchestrator
// ============================================================================

#include "collector.hpp"

#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <wincodec.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>
#include <thread>
#include <atomic>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

static D2D1_COLOR_F HexColor(uint32_t rgb, float alpha = 1.0f) {
    return D2D1::ColorF(
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(rgb & 0xFF) / 255.0f,
        alpha
    );
}

static const D2D1_COLOR_F COL_BG          = HexColor(0x0D0D0D);
static const D2D1_COLOR_F COL_SIDEBAR     = HexColor(0x111111);
static const D2D1_COLOR_F COL_CARD        = HexColor(0x141414);
static const D2D1_COLOR_F COL_CARD_HDR    = HexColor(0x181818);
static const D2D1_COLOR_F COL_BORDER      = HexColor(0x262626);
static const D2D1_COLOR_F COL_DIVIDER     = HexColor(0x1F1F1F);
static const D2D1_COLOR_F COL_PILL_BG     = HexColor(0x1E1E1E);
static const D2D1_COLOR_F COL_PILL_ACT    = HexColor(0x2A2A2A);
static const D2D1_COLOR_F COL_TEXT_MAIN   = HexColor(0xF2F2F2);
static const D2D1_COLOR_F COL_TEXT_MUTED  = HexColor(0x8C8C8C);
static const D2D1_COLOR_F COL_TEXT_DIM    = HexColor(0x555555);
static const D2D1_COLOR_F COL_GREEN       = HexColor(0x38C77B);
static const D2D1_COLOR_F COL_AMBER       = HexColor(0xF59E0B);
static const D2D1_COLOR_F COL_RED         = HexColor(0xEF4444);
static const D2D1_COLOR_F COL_BLUE        = HexColor(0x60A5FA);
static const D2D1_COLOR_F COL_PURPLE      = HexColor(0xC084FC);
static const D2D1_COLOR_F COL_CORAL       = HexColor(0xD97757);
static const D2D1_COLOR_F COL_CORAL_BG    = HexColor(0x1C1411);
static const D2D1_COLOR_F COL_CORAL_BDR   = HexColor(0x6E3B2B);
static const D2D1_COLOR_F COL_BAR         = HexColor(0xB4B4B4);
static const D2D1_COLOR_F COL_BAR_HOVER   = HexColor(0x38C77B);

// Clean base model families (Reasoning effort is controlled separately by the [low · medium · high · max] pills!)
static const std::vector<std::string> AVAILABLE_MODELS = {
    "Gemini 4 Argon",
    "Gemini 3.8 Flash",
    "Claude Sonnet 5.5",
    "Claude Opus 5.5",
    "GPT-OSS 120B"
};

static const std::vector<std::string> AVAILABLE_EFFORTS = {"low", "medium", "high", "max"};

inline std::string StripModelEffortSuffix(const std::string& label) {
    size_t p = label.find('(');
    std::string base = (p != std::string::npos) ? label.substr(0, p) : label;
    while (!base.empty() && base.back() == ' ') base.pop_back();
    return base;
}

struct ClickRegion {
    std::string id;
    D2D1_RECT_F rect;
};

struct AnimatedSelectorPill {
    float x1 = 0.0f;
    float x2 = 0.0f;
    float target_x1 = 0.0f;
    float target_x2 = 0.0f;
    float y1 = 0.0f;
    float y2 = 0.0f;
    float flash_alpha = 0.0f;
    bool initialized = false;
};

struct TermLine {
    std::string tag; // "tool", "sub", "code", "cmd", "info", "err"
    std::string text;
};

struct CLISession {
    std::string id;
    std::string name;
    std::string model = "Gemini 4 Argon";
    std::string effort = "medium";
    bool is_running = false;
    std::vector<TermLine> lines;
    float scroll_y = 0.0f;

    CLISession() = default;
    CLISession(const std::string& tid, const std::string& tname, const std::string& eff)
        : id(tid), name(tname), effort(eff) {
        model = "Gemini 4 Argon";
        lines = {
            {"tool", "● Explore(Explore codebase structure and active components)"},
            {"sub",  "  ⎿ Done (17 tool uses · 44.2k tokens · 2.4s)"},
            {"tool", "● Write(AGENTS.md)"},
            {"code", "  ┌────────────────────────────────────────────────────────┐"},
            {"code", "  │ 1 │ # Agent Orchestration & Development Workflow      │"},
            {"code", "  │ 2 │ ## Roles & Model Assignment                        │"},
            {"code", "  │ 3 │ - Orchestrator (Argon): High-level decomposition   │"},
            {"code", "  │ 4 │ - Subagents (Flash/Sonnet): Parallel workers       │"},
            {"code", "  └────────────────────────────────────────────────────────┘"},
            {"sub",  "  ⎿ Created AGENTS.md successfully (24 lines written)"}
        };
    }
};

struct WorkerCardState {
    int idx = 0;
    std::string title;
    std::string model;
    std::string effort;
    std::string prompt;
    std::string status_text = "● Idle";
    uint32_t status_color = 0x8C8C8C;
    bool is_running = false;
    float anim_phase = 0.0f;
    float progress_fill = 0.0f;
    std::vector<std::string> logs;
};

struct TransferPacket {
    int from_node = 0; // -1 = Master Goal, 0..3 = Worker #1..#4
    int to_node = 1;   // 0..3 = Worker #1..#4
    float progress = 0.0f;
    float speed = 0.85f;
    std::string icon = "\xF0\x9F\x93\x81"; // 📁
    std::string label = "core.hpp";
    uint32_t color = 0xD97757;
};

struct FileEntry {
    std::string name;
    std::string ext_badge;
    uint32_t badge_color = 0x8C8C8C;
    bool is_dir = false;
};
