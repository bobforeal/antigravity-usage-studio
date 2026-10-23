# Antigravity Usage Studio 🚀

A polished, high-performance **Desktop Application** for developers using **Google Antigravity**. Monitor real-time workspace token consumption, cache efficiency, API-equivalent costs, and live model quotas across Gemini, Claude, and OpenAI with zero guesswork.

![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?style=flat-square&logo=windows)
![Python](https://img.shields.io/badge/Python-3.10%2B-3776AB?style=flat-square&logo=python&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-2ea44f?style=flat-square)

---

## 🌟 Key Features

### 1. 100% Real Token Accounting & Cost Engine
- **No approximations or fake step multipliers**: Tokens are calculated strictly using real execution telemetry:  
  $$\text{Total Tokens} = \text{Input Tokens} + \text{Cache Read Tokens} + \text{Output Tokens}$$
- **Real-time Cache Hit Percentage**:
  $$\text{Cache Hit \%} = \frac{\text{Cache Read Tokens}}{\text{Input Tokens} + \text{Cache Read Tokens}} \times 100$$
- **API-Equivalent Cost Engine**: Tracks cumulative cost burn based on official model family pricing per 1M tokens (Claude Opus / Sonnet, Gemini 4 Argon, Gemini 3.8 Flash, GPT-OSS).

### 2. Three Dedicated Studio Views
- 📊 **Dashboard View**:
  - **3 KPI Cards**: `Total cost`, `Requests`, and `Tokens` with clear breakdown subtitles (`Cache Hit %`, `Input / Cache / Output`).
  - **Dual Chart Modes**:
    - **Hourly Cost & Hourly Tokens**: 24 rolling hourly buckets with interactive hover tooltips and dashed guidelines.
    - **Activity Heatmap**: 22-week × 7-day contribution grid (GitHub / Claude Code style) with active streak counter (`🔥 Xd streak`), active days, total steps, and daily token hover inspection.
  - **Usage by Model Table**: Up to 8 active models with provider badges (`GO` Google, `CL` Anthropic, `OA` OpenAI), request counts, input/cache/output tokens, live quota remaining %, and cost.
- 🗂️ **Sessions View**:
  - Full table of recent workspace sessions (top 40).
  - Clean titles resolved from `conversation_summaries.db` or parsed from `<USER_REQUEST>` prompt tags.
  - Real-time search filter by title, model, or session ID.
- ⚡ **Live Quotas View**:
  - Direct real-time RPC connection to the local Antigravity Language Server (`LanguageServerService/GetUserStatus`).
  - All 13 models tracked with live visual progress bars, remaining %, and countdown timers (e.g. `⏳ Resets in 4h 54m`).
  - Prompt credits indicator.

### 3. Native Windows Desktop Polish
- **Standard Windows OS Window**: Full maximize, minimize, resize handles, and taskbar icon (`overrideredirect(False)`).
- **Windows 11/10 Immersive Dark Title Bar**: Leverages native `DwmSetWindowAttribute` (`DWMWA_USE_IMMERSIVE_DARK_MODE`).
- **Flexible Time Ranges**: One-click toggling between `Today`, `24h`, `7 Days`, `30 Days`, and `All Time`.
- **One-Click Markdown Export**: Copy formatted telemetry tables directly to your clipboard.
- **Optional Always on Top (📌)**: Toggle anytime from the top app bar.

---

## 🚀 Quick Start

### Option 1: Standalone Portable Executable (No Python Required)
1. Download or locate `antigravity-hud.exe`.
2. Double-click **`antigravity-hud.exe`** to launch immediately.

### Option 2: Run from Python Source
1. Ensure Python 3.10+ is installed on Windows.
2. Install dependencies:
   ```powershell
   pip install -r requirements.txt
   ```
3. Run the studio:
   ```powershell
   python hud_app.py
   ```
   *(Or double-click `run.bat`)*

---

## 🛠️ Building the Standalone Executable

To compile a clean single-file executable using PyInstaller:

```powershell
pip install pyinstaller psutil
pyinstaller --noconsole --onefile --clean --name "antigravity-hud" hud_app.py
```

The resulting standalone executable will be placed in `dist\antigravity-hud.exe`.

---

## 🔒 Privacy & Architecture

- **100% Local & Offline**: All telemetry is read directly from your local Antigravity directories (`~/.gemini/antigravity/brain` and `conversation_summaries.db`).
- **Zero Cloud Tracking**: No analytics, telemetry, or personal data are ever sent over the internet.
- **Local RPC**: Connects exclusively to the local loopback port (`127.0.0.1`) spawned by your local Antigravity process.

---

## 📄 License

Distributed under the [MIT License](LICENSE).
