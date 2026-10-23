import os
import sys
import math
import time
import threading
import subprocess
from datetime import datetime, date
import tkinter as tk
from tkinter import ttk

from collector import (
    AntigravityDataCollector,
    load_settings,
    save_settings,
    format_tokens,
    format_cost,
    get_model_provider_and_badge,
    BRAIN_DIR
)

# Enable High DPI Awareness on Windows
try:
    import ctypes
    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(2)  # Per-Monitor High DPI
    except Exception:
        ctypes.windll.shcore.SetProcessDpiAwareness(1)
except Exception:
    pass

def apply_dark_title_bar(window):
    """Enables Windows 10/11 immersive dark mode on the OS title bar."""
    try:
        import ctypes
        window.update_idletasks()
        hwnd = ctypes.windll.user32.GetParent(window.winfo_id())
        if not hwnd:
            hwnd = window.winfo_id()
        DWMWA_USE_IMMERSIVE_DARK_MODE = 20
        val = ctypes.c_int(2)
        res = ctypes.windll.dwmapi.DwmSetWindowAttribute(
            hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, ctypes.byref(val), ctypes.sizeof(val)
        )
        if res != 0:
            DWMWA_USE_IMMERSIVE_DARK_MODE_OLD = 19
            val = ctypes.c_int(1)
            ctypes.windll.dwmapi.DwmSetWindowAttribute(
                hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, ctypes.byref(val), ctypes.sizeof(val)
            )
    except Exception:
        pass


RANGE_MAP = {
    "today_active": "Today",
    "today_24h": "24h",
    "7d": "7 Days",
    "30d": "30 Days",
    "all": "All Time",
}


class DesktopUsageStudio(tk.Tk):
    def __init__(self):
        super().__init__()

        self.settings = load_settings()

        # Sleek Monochrome Dark Dashboard Palette
        self.C_BG = "#111111"
        self.C_CARD = "#1a1a1a"
        self.C_CARD_HEADER = "#202020"
        self.C_CARD_HOVER = "#242424"
        self.C_BORDER = "#2a2a2a"
        self.C_DIVIDER = "#252525"
        self.C_PILL_BG = "#262626"
        self.C_PILL_ACTIVE = "#333333"
        self.C_TEXT_MAIN = "#f5f5f5"
        self.C_TEXT_MUTED = "#8c8c8c"
        self.C_TEXT_DIM = "#555555"
        self.C_BAR = "#a6a6a6"
        self.C_BAR_HOVER = "#ffffff"
        self.C_GREEN = "#34d399"
        self.C_AMBER = "#fbbf24"
        self.C_RED = "#f87171"
        self.C_BLUE = "#60a5fa"
        self.C_PURPLE = "#c084fc"

        # Window Config - Standard Desktop Window (overrideredirect=False)
        self.title("Antigravity Usage Studio")

        # Dark style for ttk scrollbars
        self.style = ttk.Style()
        try:
            self.style.theme_use("clam")
            self.style.configure(
                "Vertical.TScrollbar",
                gripcount=0,
                background="#2a2a2a",
                darkcolor="#1a1a1a",
                lightcolor="#1a1a1a",
                troughcolor="#141414",
                bordercolor="#141414",
                arrowcolor="#8c8c8c"
            )
            self.style.map(
                "Vertical.TScrollbar",
                background=[("active", "#3a3a3a"), ("pressed", "#4a4a4a")]
            )
        except Exception:
            pass
        self.overrideredirect(False)
        self.configure(bg=self.C_BG)

        self.is_pinned = self.settings.get("is_pinned", False)
        self.attributes("-topmost", self.is_pinned)

        # State
        self.current_view = self.settings.get("selected_view", "dashboard")
        if self.current_view not in ("dashboard", "sessions", "quotas"):
            self.current_view = "dashboard"

        self.selected_range = self.settings.get("selected_range", "today_active")
        if self.selected_range not in RANGE_MAP:
            self.selected_range = "today_active"

        self.chart_mode = self.settings.get("chart_mode", "cost")  # 'cost', 'tokens', 'heatmap'
        if self.chart_mode not in ("cost", "tokens", "heatmap"):
            self.chart_mode = "cost"

        self.hovered_bar_idx = None
        self.bar_rects = []
        self.hovered_heat_cell = None
        self.heat_rects = []
        self.session_search_query = ""

        # Geometry Setup
        screen_w = self.winfo_screenwidth()
        screen_h = self.winfo_screenheight()
        saved_w = self.settings.get("window_w", 1140)
        saved_h = self.settings.get("window_h", 780)
        self.win_w = max(940, min(saved_w, screen_w - 60))
        self.win_h = max(680, min(saved_h, screen_h - 60))

        saved_x = self.settings.get("window_x")
        saved_y = self.settings.get("window_y")
        if isinstance(saved_x, int) and 0 <= saved_x <= screen_w - 200 and isinstance(saved_y, int) and 0 <= saved_y <= screen_h - 200:
            x, y = saved_x, saved_y
        else:
            x = max(20, (screen_w - self.win_w) // 2)
            y = max(20, (screen_h - self.win_h) // 2)

        self.geometry(f"{self.win_w}x{self.win_h}+{x}+{y}")
        self.minsize(940, 680)

        # Live Data Cache
        self.proc_stats = {"running": False, "memory_mb": 0, "cpu_percent": 0.0, "process_count": 0}
        self.quota_data = {
            "connected": False,
            "tier": "Connecting...",
            "user_name": "",
            "email": "",
            "models": [],
            "models_by_label": {},
            "primary_model": None,
            "secondary_model": None,
            "prompt_credits": None
        }
        self.dash_data = {
            "selected_range": self.selected_range,
            "active_session": None,
            "total_cost": 0.0,
            "total_requests": 0,
            "total_tokens": 0,
            "total_input": 0,
            "total_cache": 0,
            "total_output": 0,
            "cache_hit_pct": 0.0,
            "hourly_slots": [],
            "heatmap": {"weeks": [], "total_steps": 0, "total_tokens": 0, "active_days": 0, "streak": 0},
            "models_table": [],
            "models_count": 0,
            "providers_count": 1,
            "sessions_list": []
        }

        # Build UI Structure
        self.create_widgets()

        # Apply dark title bar after main window initialization
        self.after(50, lambda: apply_dark_title_bar(self))

        # Event Handlers
        self.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.bind("<Configure>", self.on_window_configure)

        # Background Poller Thread
        self.running = True
        self.poll_thread = threading.Thread(target=self.data_poll_worker, daemon=True)
        self.poll_thread.start()

        # UI Refresh Loop
        self.update_ui_loop()

    # -----------------------------------------------------------------
    # UI Layout Construction
    # -----------------------------------------------------------------
    def create_widgets(self):
        # Outer Frame
        self.main_container = tk.Frame(self, bg=self.C_BG, padx=24, pady=16)
        self.main_container.pack(fill=tk.BOTH, expand=True)

        # =============================================================
        # 1. TOP APP BAR
        # =============================================================
        self.top_app_bar = tk.Frame(self.main_container, bg=self.C_BG)
        self.top_app_bar.pack(fill=tk.X, pady=(0, 16))

        # Left: Title + Subtitle + Live RPC Badge
        self.top_left = tk.Frame(self.top_app_bar, bg=self.C_BG)
        self.top_left.pack(side=tk.LEFT)

        self.title_row = tk.Frame(self.top_left, bg=self.C_BG)
        self.title_row.pack(anchor="w")

        self.lbl_app_title = tk.Label(
            self.title_row,
            text="Usage",
            font=("Segoe UI", 15, "bold"),
            fg=self.C_TEXT_MAIN,
            bg=self.C_BG
        )
        self.lbl_app_title.pack(side=tk.LEFT)

        # Live RPC Status Badge
        self.lbl_rpc_badge = tk.Label(
            self.title_row,
            text="● Connecting RPC...",
            font=("Segoe UI", 8, "bold"),
            fg=self.C_AMBER,
            bg=self.C_PILL_BG,
            padx=9,
            pady=3,
            cursor="hand2"
        )
        self.lbl_rpc_badge.pack(side=tk.LEFT, padx=(12, 0))
        self.lbl_rpc_badge.bind("<Button-1>", lambda e: self.switch_view("quotas"))

        self.lbl_app_subtitle = tk.Label(
            self.top_left,
            text="Monitor workspace requests, token usage, and costs.",
            font=("Segoe UI", 8),
            fg=self.C_TEXT_MUTED,
            bg=self.C_BG
        )
        self.lbl_app_subtitle.pack(anchor="w", pady=(3, 0))

        # Center / View Tabs
        self.tabs_container = tk.Frame(self.top_app_bar, bg=self.C_CARD, highlightbackground=self.C_BORDER, highlightthickness=1)
        self.tabs_container.pack(side=tk.LEFT, padx=(18, 0), pady=2)

        self.tab_buttons = {}
        tabs = [
            ("dashboard", "Dashboard"),
            ("sessions", "Sessions (40)"),
            ("quotas", "Live Quotas"),
        ]
        for key, label in tabs:
            btn = tk.Label(
                self.tabs_container,
                text=label,
                font=("Segoe UI", 8, "bold"),
                fg=self.C_TEXT_MAIN if self.current_view == key else self.C_TEXT_MUTED,
                bg=self.C_PILL_ACTIVE if self.current_view == key else self.C_CARD,
                padx=12,
                pady=5,
                cursor="hand2"
            )
            btn.pack(side=tk.LEFT)
            btn.bind("<Button-1>", lambda e, k=key: self.switch_view(k))
            self.tab_buttons[key] = btn

        # Right: Range Selector Buttons + Export + Pin
        self.top_right = tk.Frame(self.top_app_bar, bg=self.C_BG)
        self.top_right.pack(side=tk.RIGHT, anchor="center")

        # Range Picker Container
        self.range_box = tk.Frame(self.top_right, bg=self.C_CARD, highlightbackground=self.C_BORDER, highlightthickness=1)
        self.range_box.pack(side=tk.LEFT, padx=(0, 10))

        self.range_buttons = {}
        for r_key, r_label in RANGE_MAP.items():
            is_active = (self.selected_range == r_key)
            r_btn = tk.Label(
                self.range_box,
                text=r_label,
                font=("Segoe UI", 8, "bold" if is_active else "normal"),
                fg=self.C_TEXT_MAIN if is_active else self.C_TEXT_MUTED,
                bg=self.C_PILL_ACTIVE if is_active else self.C_CARD,
                padx=9,
                pady=4,
                cursor="hand2"
            )
            r_btn.pack(side=tk.LEFT)
            r_btn.bind("<Button-1>", lambda e, k=r_key: self.select_range(k))
            self.range_buttons[r_key] = r_btn

        # Export Button [⤓]
        self.btn_export_box = tk.Frame(self.top_right, bg=self.C_CARD, highlightbackground=self.C_BORDER, highlightthickness=1)
        self.btn_export_box.pack(side=tk.LEFT, padx=(0, 8))
        self.btn_export = tk.Label(
            self.btn_export_box,
            text="⤓ Export",
            font=("Segoe UI", 8, "bold"),
            fg=self.C_TEXT_MAIN,
            bg=self.C_CARD,
            padx=10,
            pady=4,
            cursor="hand2"
        )
        self.btn_export.pack()
        self.btn_export.bind("<Button-1>", lambda e: self.copy_telemetry_report())

        # Pin / Always on Top Toggle
        self.btn_pin_box = tk.Frame(self.top_right, bg=self.C_CARD, highlightbackground=self.C_BORDER, highlightthickness=1)
        self.btn_pin_box.pack(side=tk.LEFT)
        self.btn_pin = tk.Label(
            self.btn_pin_box,
            text="📌",
            font=("Segoe UI", 8),
            fg=self.C_TEXT_MAIN if self.is_pinned else self.C_TEXT_DIM,
            bg=self.C_CARD,
            padx=7,
            pady=4,
            cursor="hand2"
        )
        self.btn_pin.pack()
        self.btn_pin.bind("<Button-1>", lambda e: self.toggle_pin())

        # =============================================================
        # 2. VIEW CONTAINERS
        # =============================================================
        self.view_container = tk.Frame(self.main_container, bg=self.C_BG)
        self.view_container.pack(fill=tk.BOTH, expand=True)

        self.view_dashboard = tk.Frame(self.view_container, bg=self.C_BG)
        self.view_sessions = tk.Frame(self.view_container, bg=self.C_BG)
        self.view_quotas = tk.Frame(self.view_container, bg=self.C_BG)

        self.setup_dashboard_view()
        self.setup_sessions_view()
        self.setup_quotas_view()

        # Display Initial View
        self.switch_view(self.current_view)

    # -----------------------------------------------------------------
    # View 1: DASHBOARD VIEW
    # -----------------------------------------------------------------
    def setup_dashboard_view(self):
        # 3 KPI Cards Row
        self.kpi_row = tk.Frame(self.view_dashboard, bg=self.C_BG)
        self.kpi_row.pack(fill=tk.X, pady=(0, 10))
        self.kpi_row.columnconfigure(0, weight=1, uniform="kpi")
        self.kpi_row.columnconfigure(1, weight=1, uniform="kpi")
        self.kpi_row.columnconfigure(2, weight=1, uniform="kpi")

        # Card 1: Total cost
        self.card_cost = tk.Frame(self.kpi_row, bg=self.C_CARD, highlightbackground=self.C_BORDER, highlightthickness=1, padx=16, pady=12)
        self.card_cost.grid(row=0, column=0, sticky="nsew", padx=(0, 6))
        tk.Label(self.card_cost, text="Total cost", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD).pack(anchor="w")
        self.lbl_kpi_cost = tk.Label(self.card_cost, text="$0.00", font=("Segoe UI", 18, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD)
        self.lbl_kpi_cost.pack(anchor="w", pady=(3, 0))
        self.lbl_kpi_cost_sub = tk.Label(self.card_cost, text="API-Equivalent Burn", font=("Segoe UI", 7), fg=self.C_TEXT_MUTED, bg=self.C_CARD)
        self.lbl_kpi_cost_sub.pack(anchor="w", pady=(2, 0))

        # Card 2: Requests
        self.card_reqs = tk.Frame(self.kpi_row, bg=self.C_CARD, highlightbackground=self.C_BORDER, highlightthickness=1, padx=16, pady=12)
        self.card_reqs.grid(row=0, column=1, sticky="nsew", padx=6)
        tk.Label(self.card_reqs, text="Requests", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD).pack(anchor="w")
        self.lbl_kpi_reqs = tk.Label(self.card_reqs, text="0", font=("Segoe UI", 18, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD)
        self.lbl_kpi_reqs.pack(anchor="w", pady=(3, 0))
        self.lbl_kpi_reqs_sub = tk.Label(self.card_reqs, text="Cache Hit: 0.0%", font=("Segoe UI", 7), fg=self.C_GREEN, bg=self.C_CARD)
        self.lbl_kpi_reqs_sub.pack(anchor="w", pady=(2, 0))

        # Card 3: Tokens
        self.card_toks = tk.Frame(self.kpi_row, bg=self.C_CARD, highlightbackground=self.C_BORDER, highlightthickness=1, padx=16, pady=12)
        self.card_toks.grid(row=0, column=2, sticky="nsew", padx=(6, 0))
        tk.Label(self.card_toks, text="Tokens", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD).pack(anchor="w")
        self.lbl_kpi_toks = tk.Label(self.card_toks, text="0", font=("Segoe UI", 18, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD)
        self.lbl_kpi_toks.pack(anchor="w", pady=(3, 0))
        self.lbl_kpi_toks_sub = tk.Label(self.card_toks, text="Input: 0 · Cache: 0 · Output: 0", font=("Segoe UI", 7), fg=self.C_TEXT_MUTED, bg=self.C_CARD)
        self.lbl_kpi_toks_sub.pack(anchor="w", pady=(2, 0))

        # -------------------------------------------------------------
        # CHART CARD (Bar Chart / Heatmap Switcher)
        # -------------------------------------------------------------
        self.chart_outer = tk.Frame(self.view_dashboard, bg=self.C_CARD, highlightbackground=self.C_BORDER, highlightthickness=1, padx=16, pady=10)
        self.chart_outer.pack(fill=tk.X, pady=(0, 10))

        # Chart Header
        self.chart_hdr_frame = tk.Frame(self.chart_outer, bg=self.C_CARD)
        self.chart_hdr_frame.pack(fill=tk.X, pady=(0, 8))

        self.chart_hdr_left = tk.Frame(self.chart_hdr_frame, bg=self.C_CARD)
        self.chart_hdr_left.pack(side=tk.LEFT)

        self.lbl_chart_title = tk.Label(
            self.chart_hdr_left,
            text="Hourly Cost",
            font=("Segoe UI", 10, "bold"),
            fg=self.C_TEXT_MAIN,
            bg=self.C_CARD
        )
        self.lbl_chart_title.pack(anchor="w")

        self.lbl_chart_sub = tk.Label(
            self.chart_hdr_left,
            text="Model and Web Search costs for the selected range.",
            font=("Segoe UI", 8),
            fg=self.C_TEXT_MUTED,
            bg=self.C_CARD
        )
        self.lbl_chart_sub.pack(anchor="w", pady=(2, 0))

        # Chart Mode Selector Pills
        self.chart_mode_box = tk.Frame(self.chart_hdr_frame, bg=self.C_CARD, highlightbackground=self.C_BORDER, highlightthickness=1)
        self.chart_mode_box.pack(side=tk.RIGHT)

        self.chart_mode_btns = {}
        chart_modes = [
            ("cost", "Hourly Cost"),
            ("tokens", "Hourly Tokens"),
            ("heatmap", "Activity Heatmap")
        ]
        for c_mode, c_title in chart_modes:
            btn = tk.Label(
                self.chart_mode_box,
                text=c_title,
                font=("Segoe UI", 8, "bold" if self.chart_mode == c_mode else "normal"),
                fg=self.C_TEXT_MAIN if self.chart_mode == c_mode else self.C_TEXT_MUTED,
                bg=self.C_PILL_ACTIVE if self.chart_mode == c_mode else self.C_CARD,
                padx=10,
                pady=4,
                cursor="hand2"
            )
            btn.pack(side=tk.LEFT)
            btn.bind("<Button-1>", lambda e, m=c_mode: self.select_chart_mode(m))
            self.chart_mode_btns[c_mode] = btn

        # Canvas for Bar Chart & Heatmap
        self.canvas_chart = tk.Canvas(
            self.chart_outer, height=150, bg=self.C_CARD, highlightthickness=0, cursor="crosshair"
        )
        self.canvas_chart.pack(fill=tk.X)
        self.canvas_chart.bind("<Motion>", self.on_chart_canvas_hover)
        self.canvas_chart.bind("<Leave>", self.on_chart_canvas_leave)

        # -------------------------------------------------------------
        # USAGE BY MODEL CARD
        # -------------------------------------------------------------
        self.table_card = tk.Frame(self.view_dashboard, bg=self.C_CARD, highlightbackground=self.C_BORDER, highlightthickness=1, padx=16, pady=10)
        self.table_card.pack(fill=tk.BOTH, expand=True)

        # Table Header Row
        self.tbl_top_bar = tk.Frame(self.table_card, bg=self.C_CARD)
        self.tbl_top_bar.pack(fill=tk.X, pady=(0, 8))

        self.tbl_top_left = tk.Frame(self.tbl_top_bar, bg=self.C_CARD)
        self.tbl_top_left.pack(side=tk.LEFT)

        self.tbl_title_row = tk.Frame(self.tbl_top_left, bg=self.C_CARD)
        self.tbl_title_row.pack(anchor="w")

        tk.Label(
            self.tbl_title_row,
            text="Usage by Model",
            font=("Segoe UI", 10, "bold"),
            fg=self.C_TEXT_MAIN,
            bg=self.C_CARD
        ).pack(side=tk.LEFT)

        self.lbl_models_pill = tk.Label(
            self.tbl_title_row,
            text="0 models · 1 providers",
            font=("Segoe UI", 7, "bold"),
            fg=self.C_TEXT_MUTED,
            bg=self.C_PILL_BG,
            padx=7,
            pady=1
        )
        self.lbl_models_pill.pack(side=tk.LEFT, padx=(9, 0))

        tk.Label(
            self.tbl_top_left,
            text="Per-model breakdown for the selected range.",
            font=("Segoe UI", 8),
            fg=self.C_TEXT_MUTED,
            bg=self.C_CARD
        ).pack(anchor="w", pady=(2, 0))

        # Column Header Row
        self.tbl_col_hdr = tk.Frame(self.table_card, bg=self.C_CARD_HEADER, padx=12, pady=7)
        self.tbl_col_hdr.pack(fill=tk.X)
        self._setup_table_columns(self.tbl_col_hdr)

        tk.Label(self.tbl_col_hdr, text="Model", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="w").grid(row=0, column=0, sticky="w")
        tk.Label(self.tbl_col_hdr, text="Requests", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=1, sticky="e")
        tk.Label(self.tbl_col_hdr, text="Input", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=2, sticky="e")
        tk.Label(self.tbl_col_hdr, text="Cache Read", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=3, sticky="e")
        tk.Label(self.tbl_col_hdr, text="Live Quota", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=4, sticky="e")
        tk.Label(self.tbl_col_hdr, text="Output", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=5, sticky="e")
        tk.Label(self.tbl_col_hdr, text="Cost ↓", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=6, sticky="e")

        # 8 Pre-allocated Rows with 1px dividers
        self.table_rows = []
        for i in range(8):
            div = tk.Frame(self.table_card, bg=self.C_DIVIDER, height=1)
            div.pack(fill=tk.X)

            row_f = tk.Frame(self.table_card, bg=self.C_CARD, padx=12, pady=4)
            row_f.pack(fill=tk.X)
            self._setup_table_columns(row_f)

            # Cell 0: Badge + Model Label
            cell_m = tk.Frame(row_f, bg=self.C_CARD)
            cell_m.grid(row=0, column=0, sticky="w")

            lbl_badge = tk.Label(cell_m, text="GO", font=("Consolas", 7, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_PILL_BG, padx=4, pady=1)
            lbl_badge.pack(side=tk.LEFT, padx=(0, 8))

            lbl_name = tk.Label(cell_m, text="Model", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD)
            lbl_name.pack(side=tk.LEFT)

            lbl_req = tk.Label(row_f, text="0", font=("Segoe UI", 8), fg=self.C_TEXT_MAIN, bg=self.C_CARD, anchor="e")
            lbl_req.grid(row=0, column=1, sticky="e")

            lbl_in = tk.Label(row_f, text="-", font=("Segoe UI", 8), fg=self.C_TEXT_MAIN, bg=self.C_CARD, anchor="e")
            lbl_in.grid(row=0, column=2, sticky="e")

            lbl_cache = tk.Label(row_f, text="-", font=("Segoe UI", 8), fg=self.C_TEXT_MAIN, bg=self.C_CARD, anchor="e")
            lbl_cache.grid(row=0, column=3, sticky="e")

            lbl_quota = tk.Label(row_f, text="100%", font=("Segoe UI", 8, "bold"), fg=self.C_GREEN, bg=self.C_CARD, anchor="e")
            lbl_quota.grid(row=0, column=4, sticky="e")

            lbl_out = tk.Label(row_f, text="-", font=("Segoe UI", 8), fg=self.C_TEXT_MAIN, bg=self.C_CARD, anchor="e")
            lbl_out.grid(row=0, column=5, sticky="e")

            lbl_cost = tk.Label(row_f, text="$0.00", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD, anchor="e")
            lbl_cost.grid(row=0, column=6, sticky="e")

            # Row Hover Effect
            row_widgets = [row_f, cell_m, lbl_badge, lbl_name, lbl_req, lbl_in, lbl_cache, lbl_quota, lbl_out, lbl_cost]
            for w in row_widgets:
                w.bind("<Enter>", lambda e, rf=row_f, cm=cell_m: self._highlight_row(rf, cm, True))
                w.bind("<Leave>", lambda e, rf=row_f, cm=cell_m: self._highlight_row(rf, cm, False))

            self.table_rows.append({
                "div": div,
                "frame": row_f,
                "cell_m": cell_m,
                "badge": lbl_badge,
                "name": lbl_name,
                "req": lbl_req,
                "in": lbl_in,
                "cache": lbl_cache,
                "quota": lbl_quota,
                "out": lbl_out,
                "cost": lbl_cost
            })

    def _setup_table_columns(self, frame):
        frame.columnconfigure(0, weight=3, minsize=240)
        frame.columnconfigure(1, weight=1, minsize=80)
        frame.columnconfigure(2, weight=1, minsize=90)
        frame.columnconfigure(3, weight=1, minsize=100)
        frame.columnconfigure(4, weight=1, minsize=120)
        frame.columnconfigure(5, weight=1, minsize=80)
        frame.columnconfigure(6, weight=1, minsize=90)

    def _highlight_row(self, row_frame, cell_m, is_hover):
        bg_col = self.C_CARD_HOVER if is_hover else self.C_CARD
        row_frame.configure(bg=bg_col)
        cell_m.configure(bg=bg_col)
        for child in row_frame.winfo_children():
            if child != cell_m:
                child.configure(bg=bg_col)
        for child in cell_m.winfo_children():
            if not isinstance(child, tk.Label) or child.cget("text") not in ("GO", "CL", "OA"):
                child.configure(bg=bg_col)

    # -----------------------------------------------------------------
    # View 2: SESSIONS VIEW
    # -----------------------------------------------------------------
    def setup_sessions_view(self):
        self.sess_outer = tk.Frame(self.view_sessions, bg=self.C_CARD, highlightbackground=self.C_BORDER, highlightthickness=1, padx=16, pady=14)
        self.sess_outer.pack(fill=tk.BOTH, expand=True)

        # Header with Search Box
        self.sess_hdr = tk.Frame(self.sess_outer, bg=self.C_CARD)
        self.sess_hdr.pack(fill=tk.X, pady=(0, 10))

        self.sess_hdr_left = tk.Frame(self.sess_hdr, bg=self.C_CARD)
        self.sess_hdr_left.pack(side=tk.LEFT)

        self.lbl_sess_title = tk.Label(
            self.sess_hdr_left,
            text="Recent Antigravity Sessions",
            font=("Segoe UI", 11, "bold"),
            fg=self.C_TEXT_MAIN,
            bg=self.C_CARD
        )
        self.lbl_sess_title.pack(anchor="w")

        self.lbl_sess_sub = tk.Label(
            self.sess_hdr_left,
            text="Detailed token usage, cache efficiency, and API cost per session.",
            font=("Segoe UI", 8),
            fg=self.C_TEXT_MUTED,
            bg=self.C_CARD
        )
        self.lbl_sess_sub.pack(anchor="w", pady=(2, 0))

        # Search / Filter Bar
        self.sess_search_box = tk.Frame(self.sess_hdr, bg=self.C_CARD_HEADER, highlightbackground=self.C_BORDER, highlightthickness=1)
        self.sess_search_box.pack(side=tk.RIGHT)

        tk.Label(self.sess_search_box, text="🔍", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, padx=6).pack(side=tk.LEFT)
        self.ent_sess_search = tk.Entry(
            self.sess_search_box, font=("Segoe UI", 8), fg=self.C_TEXT_MAIN, bg=self.C_CARD_HEADER,
            insertbackground=self.C_TEXT_MAIN, relief="flat", width=22
        )
        self.ent_sess_search.pack(side=tk.LEFT, padx=(0, 6), pady=4)
        self.ent_sess_search.bind("<KeyRelease>", self.on_session_search_change)

        # Column Header Row
        self.sess_tbl_hdr = tk.Frame(self.sess_outer, bg=self.C_CARD_HEADER, padx=12, pady=7)
        self.sess_tbl_hdr.pack(fill=tk.X)
        self._setup_session_columns(self.sess_tbl_hdr)

        tk.Label(self.sess_tbl_hdr, text="Session Title / Prompt", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="w").grid(row=0, column=0, sticky="w")
        tk.Label(self.sess_tbl_hdr, text="Last Activity", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="w").grid(row=0, column=1, sticky="w")
        tk.Label(self.sess_tbl_hdr, text="Model", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="w").grid(row=0, column=2, sticky="w")
        tk.Label(self.sess_tbl_hdr, text="Steps", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=3, sticky="e")
        tk.Label(self.sess_tbl_hdr, text="Requests", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=4, sticky="e")
        tk.Label(self.sess_tbl_hdr, text="Input", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=5, sticky="e")
        tk.Label(self.sess_tbl_hdr, text="Cache Read", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=6, sticky="e")
        tk.Label(self.sess_tbl_hdr, text="Output", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=7, sticky="e")
        tk.Label(self.sess_tbl_hdr, text="Total Tokens", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=8, sticky="e")
        tk.Label(self.sess_tbl_hdr, text="Cost", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=9, sticky="e")

        # Scrollable Canvas Frame for Sessions
        self.sess_canvas_container = tk.Frame(self.sess_outer, bg=self.C_CARD)
        self.sess_canvas_container.pack(fill=tk.BOTH, expand=True)

        self.sess_canvas = tk.Canvas(self.sess_canvas_container, bg=self.C_CARD, highlightthickness=0)
        self.sess_scrollbar = ttk.Scrollbar(self.sess_canvas_container, orient="vertical", command=self.sess_canvas.yview, style="Vertical.TScrollbar")
        self.sess_scrollable_frame = tk.Frame(self.sess_canvas, bg=self.C_CARD)

        self.sess_scrollable_frame.bind(
            "<Configure>",
            lambda e: self.sess_canvas.configure(scrollregion=self.sess_canvas.bbox("all"))
        )

        self.sess_canvas_window = self.sess_canvas.create_window((0, 0), window=self.sess_scrollable_frame, anchor="nw")
        self.sess_canvas.configure(xscrollcommand=None, yscrollcommand=self.sess_scrollbar.set)

        self.sess_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.sess_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        self.sess_canvas.bind("<Configure>", self._on_sess_canvas_configure)
        self.sess_canvas.bind_all("<MouseWheel>", self._on_mousewheel)

        self.rendered_session_widgets = []

    def _setup_session_columns(self, frame):
        frame.columnconfigure(0, weight=4, minsize=220)
        frame.columnconfigure(1, weight=1, minsize=115)
        frame.columnconfigure(2, weight=2, minsize=140)
        frame.columnconfigure(3, weight=1, minsize=55)
        frame.columnconfigure(4, weight=1, minsize=65)
        frame.columnconfigure(5, weight=1, minsize=70)
        frame.columnconfigure(6, weight=1, minsize=75)
        frame.columnconfigure(7, weight=1, minsize=65)
        frame.columnconfigure(8, weight=1, minsize=80)
        frame.columnconfigure(9, weight=1, minsize=75)

    def _on_sess_canvas_configure(self, event):
        self.sess_canvas.itemconfig(self.sess_canvas_window, width=event.width)

    def _on_mousewheel(self, event):
        if self.current_view == "sessions" and self.sess_canvas.winfo_exists():
            self.sess_canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")
        elif self.current_view == "quotas" and hasattr(self, "quotas_canvas") and self.quotas_canvas.winfo_exists():
            self.quotas_canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

    def on_session_search_change(self, event):
        self.session_search_query = self.ent_sess_search.get().strip().lower()
        self.render_sessions_table()

    # -----------------------------------------------------------------
    # View 3: LIVE QUOTAS VIEW
    # -----------------------------------------------------------------
    def setup_quotas_view(self):
        self.quotas_outer = tk.Frame(self.view_quotas, bg=self.C_CARD, highlightbackground=self.C_BORDER, highlightthickness=1, padx=16, pady=14)
        self.quotas_outer.pack(fill=tk.BOTH, expand=True)

        # Header
        self.quotas_hdr = tk.Frame(self.quotas_outer, bg=self.C_CARD)
        self.quotas_hdr.pack(fill=tk.X, pady=(0, 12))

        self.quotas_hdr_left = tk.Frame(self.quotas_hdr, bg=self.C_CARD)
        self.quotas_hdr_left.pack(side=tk.LEFT)

        self.lbl_quotas_title = tk.Label(
            self.quotas_hdr_left,
            text="Live Antigravity Quotas & Limits",
            font=("Segoe UI", 11, "bold"),
            fg=self.C_TEXT_MAIN,
            bg=self.C_CARD
        )
        self.lbl_quotas_title.pack(anchor="w")

        self.lbl_quotas_sub = tk.Label(
            self.quotas_hdr_left,
            text="Real-time RPC status directly from LanguageServerService/GetUserStatus.",
            font=("Segoe UI", 8),
            fg=self.C_TEXT_MUTED,
            bg=self.C_CARD
        )
        self.lbl_quotas_sub.pack(anchor="w", pady=(2, 0))

        # Right Summary Badges
        self.quotas_hdr_right = tk.Frame(self.quotas_hdr, bg=self.C_CARD)
        self.quotas_hdr_right.pack(side=tk.RIGHT)

        self.lbl_quotas_conn_pill = tk.Label(
            self.quotas_hdr_right,
            text="● Connected · 13 Models",
            font=("Segoe UI", 8, "bold"),
            fg=self.C_GREEN,
            bg=self.C_PILL_BG,
            padx=10,
            pady=4
        )
        self.lbl_quotas_conn_pill.pack(side=tk.LEFT, padx=(0, 8))

        self.lbl_credits_pill = tk.Label(
            self.quotas_hdr_right,
            text="Credits: --",
            font=("Segoe UI", 8, "bold"),
            fg=self.C_TEXT_MAIN,
            bg=self.C_PILL_BG,
            padx=10,
            pady=4
        )
        self.lbl_credits_pill.pack(side=tk.LEFT)

        # Scrollable Quotas Container
        self.quotas_canvas_container = tk.Frame(self.quotas_outer, bg=self.C_CARD)
        self.quotas_canvas_container.pack(fill=tk.BOTH, expand=True)

        self.quotas_canvas = tk.Canvas(self.quotas_canvas_container, bg=self.C_CARD, highlightthickness=0)
        self.quotas_scrollbar = ttk.Scrollbar(self.quotas_canvas_container, orient="vertical", command=self.quotas_canvas.yview, style="Vertical.TScrollbar")
        self.quotas_scrollable_frame = tk.Frame(self.quotas_canvas, bg=self.C_CARD)

        self.quotas_scrollable_frame.bind(
            "<Configure>",
            lambda e: self.quotas_canvas.configure(scrollregion=self.quotas_canvas.bbox("all"))
        )

        self.quotas_canvas_window = self.quotas_canvas.create_window((0, 0), window=self.quotas_scrollable_frame, anchor="nw")
        self.quotas_canvas.configure(xscrollcommand=None, yscrollcommand=self.quotas_scrollbar.set)

        self.quotas_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.quotas_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        self.quotas_canvas.bind("<Configure>", lambda e: self.quotas_canvas.itemconfig(self.quotas_canvas_window, width=e.width))

        self.rendered_quota_widgets = []

    # -----------------------------------------------------------------
    # View Switching
    # -----------------------------------------------------------------
    def switch_view(self, view_key):
        self.current_view = view_key
        self.settings["selected_view"] = view_key
        save_settings(self.settings)

        # Update tab styling
        for key, btn in self.tab_buttons.items():
            if key == view_key:
                btn.config(fg=self.C_TEXT_MAIN, bg=self.C_PILL_ACTIVE)
            else:
                btn.config(fg=self.C_TEXT_MUTED, bg=self.C_CARD)

        # Hide all views
        self.view_dashboard.pack_forget()
        self.view_sessions.pack_forget()
        self.view_quotas.pack_forget()

        # Show target view
        if view_key == "dashboard":
            self.view_dashboard.pack(fill=tk.BOTH, expand=True)
            self.render_dashboard_view()
        elif view_key == "sessions":
            self.view_sessions.pack(fill=tk.BOTH, expand=True)
            self.render_sessions_table()
        elif view_key == "quotas":
            self.view_quotas.pack(fill=tk.BOTH, expand=True)
            self.render_quotas_view()

    def select_range(self, range_key):
        self.selected_range = range_key
        self.settings["selected_range"] = range_key
        save_settings(self.settings)

        for k, btn in self.range_buttons.items():
            if k == range_key:
                btn.config(fg=self.C_TEXT_MAIN, bg=self.C_PILL_ACTIVE, font=("Segoe UI", 8, "bold"))
            else:
                btn.config(fg=self.C_TEXT_MUTED, bg=self.C_CARD, font=("Segoe UI", 8, "normal"))

        threading.Thread(target=self.poll_once, daemon=True).start()

    def select_chart_mode(self, mode_key):
        self.chart_mode = mode_key
        self.settings["chart_mode"] = mode_key
        save_settings(self.settings)

        for k, btn in self.chart_mode_btns.items():
            if k == mode_key:
                btn.config(fg=self.C_TEXT_MAIN, bg=self.C_PILL_ACTIVE, font=("Segoe UI", 8, "bold"))
            else:
                btn.config(fg=self.C_TEXT_MUTED, bg=self.C_CARD, font=("Segoe UI", 8, "normal"))

        if mode_key == "cost":
            self.lbl_chart_title.config(text="Hourly Cost")
            self.lbl_chart_sub.config(text="Model and Web Search costs for the selected range.")
        elif mode_key == "tokens":
            self.lbl_chart_title.config(text="Hourly Tokens")
            self.lbl_chart_sub.config(text="Rolling hourly token consumption across workspace sessions.")
        else:
            self.lbl_chart_title.config(text="Activity Heatmap")
            self.lbl_chart_sub.config(text="22-week contribution grid with active streak and daily token intensity.")

        self.draw_chart()

    def toggle_pin(self):
        self.is_pinned = not self.is_pinned
        self.attributes("-topmost", self.is_pinned)
        self.btn_pin.config(fg=self.C_TEXT_MAIN if self.is_pinned else self.C_TEXT_DIM)
        self.settings["is_pinned"] = self.is_pinned
        save_settings(self.settings)

    # -----------------------------------------------------------------
    # Chart Canvas Renderer (Bar Chart & GitHub/Claude Code Heatmap)
    # -----------------------------------------------------------------
    def on_chart_canvas_hover(self, event):
        mx, my = event.x, event.y
        if self.chart_mode in ("cost", "tokens"):
            found = None
            for idx, (x1, y1, x2, y2) in enumerate(self.bar_rects):
                if x1 - 2 <= mx <= x2 + 2 and y1 <= my <= y2:
                    found = idx
                    break
            if found != self.hovered_bar_idx:
                self.hovered_bar_idx = found
                self.draw_chart()
        elif self.chart_mode == "heatmap":
            found = None
            for idx, (x1, y1, x2, y2, cell_data) in enumerate(self.heat_rects):
                if x1 <= mx <= x2 and y1 <= my <= y2:
                    found = cell_data
                    break
            if found != self.hovered_heat_cell:
                self.hovered_heat_cell = found
                self.draw_chart()

    def on_chart_canvas_leave(self, event):
        if self.hovered_bar_idx is not None or self.hovered_heat_cell is not None:
            self.hovered_bar_idx = None
            self.hovered_heat_cell = None
            self.draw_chart()

    def draw_chart(self):
        c = self.canvas_chart
        c.delete("all")
        self.bar_rects = []
        self.heat_rects = []

        w = c.winfo_width()
        h = c.winfo_height()
        if w <= 20:
            w = max(800, self.winfo_width() - 80)
        if h <= 20:
            h = 138

        if self.chart_mode in ("cost", "tokens"):
            self._draw_hourly_bar_chart(c, w, h)
        else:
            self._draw_heatmap_grid(c, w, h)

    def _draw_hourly_bar_chart(self, c, w, h):
        slots = self.dash_data.get("hourly_slots", [])
        if not slots:
            return

        is_cost = (self.chart_mode == "cost")
        vals = [s["cost"] if is_cost else s["tokens"] for s in slots]
        raw_max = max(vals + [0.001])

        if is_cost:
            if raw_max <= 1.0:
                nice_max = max(0.5, math.ceil(raw_max * 2) / 2.0)
            else:
                nice_max = float(math.ceil(raw_max))
            top_label = f"${int(nice_max)}" if nice_max == int(nice_max) else f"${nice_max:.1f}"
            bot_label = "$0"
        else:
            nice_max = raw_max * 1.1
            top_label = format_tokens(raw_max)
            bot_label = "0"

        left_pad = 42
        right_pad = 12
        top_pad = 10
        bot_pad = 22
        plot_w = w - left_pad - right_pad
        plot_h = h - top_pad - bot_pad
        base_y = top_pad + plot_h

        # Y-axis labels
        c.create_text(left_pad - 8, top_pad + 2, text=top_label, font=("Segoe UI", 8), fill=self.C_TEXT_MUTED, anchor="e")
        c.create_text(left_pad - 8, base_y - 2, text=bot_label, font=("Segoe UI", 8), fill=self.C_TEXT_MUTED, anchor="e")

        # 3 Horizontal Dashed Guidelines
        for frac in [0.0, 0.5, 1.0]:
            gy = int(top_pad + frac * plot_h)
            c.create_line(left_pad, gy, w - right_pad, gy, fill=self.C_BORDER, dash=(3, 3))

        # 24 Bars
        n_bars = len(slots)
        slot_w = plot_w / max(1, n_bars)
        bar_gap = max(3, int(slot_w * 0.28))
        tick_indices = {0, 4, 8, 12, 16, 20, 23}

        for i, s in enumerate(slots):
            val = s["cost"] if is_cost else s["tokens"]
            bx1 = int(left_pad + i * slot_w + bar_gap / 2)
            bx2 = int(left_pad + (i + 1) * slot_w - bar_gap / 2)
            cx = (bx1 + bx2) / 2

            ratio = min(1.0, val / max(1e-9, nice_max))
            bh = int(ratio * plot_h) if val > 0 else 1
            if val > 0 and bh < 3:
                bh = 3

            by1 = base_y - bh
            by2 = base_y

            is_hov = (self.hovered_bar_idx == i)
            fill_col = self.C_BAR_HOVER if is_hov else (self.C_BAR if val > 0 else "#252525")

            c.create_rectangle(bx1, by1, bx2, by2, fill=fill_col, outline="")
            self.bar_rects.append((bx1, top_pad, bx2, base_y))

            if i in tick_indices:
                c.create_text(cx, base_y + 12, text=s["label"], font=("Segoe UI", 8), fill=self.C_TEXT_MUTED)

        if self.hovered_bar_idx is not None and 0 <= self.hovered_bar_idx < len(slots):
            hs = slots[self.hovered_bar_idx]
            self.lbl_chart_sub.config(
                text=f"Hour {hs['display']}  —  Cost: {format_cost(hs['cost'])}  ·  Requests: {hs['requests']:,}  ·  Tokens: {format_tokens(hs['tokens'])}",
                fg=self.C_TEXT_MAIN
            )
        else:
            self.lbl_chart_sub.config(
                text="Model and Web Search costs for the selected range." if is_cost else "Rolling hourly token consumption across workspace sessions.",
                fg=self.C_TEXT_MUTED
            )

    def _draw_heatmap_grid(self, c, w, h):
        hm = self.dash_data.get("heatmap", {})
        weeks = hm.get("weeks", [])
        if not weeks:
            return

        left_pad = 42
        top_pad = 20
        bot_pad = 26
        right_pad = 14

        # Heatmap Palette (5 Intensity Levels)
        HEAT_COLORS = [
            "#1c1c1c",  # 0: Empty
            "#1b4332",  # 1: Low
            "#2d6a4f",  # 2: Medium
            "#40916c",  # 3: High
            "#52b788",  # 4: Very High
        ]

        total_cols = len(weeks)  # 52 weeks
        avail_h = h - top_pad - bot_pad
        avail_w = w - left_pad - right_pad

        cell_gap = 3
        max_cell_h = (avail_h - 6 * cell_gap) // 7
        step_x = avail_w / max(1, total_cols)
        max_cell_w = int(step_x - cell_gap)

        cell_size = max(8, min(max_cell_h, max_cell_w, 13))

        # Day labels on the left (Mon, Wed, Fri) aligned with row centers
        day_labels = {0: "Mon", 2: "Wed", 4: "Fri"}
        for r_idx, d_name in day_labels.items():
            ly = top_pad + r_idx * (cell_size + cell_gap) + cell_size // 2
            c.create_text(left_pad - 8, ly, text=d_name, font=("Segoe UI", 7), fill=self.C_TEXT_MUTED, anchor="e")

        # Draw Cells & Month Labels
        last_month_x = -999
        for w_idx, week in enumerate(weeks):
            cx = int(left_pad + w_idx * step_x)

            # Month Labels above columns (spaced at least 34px apart)
            m_lbl = week[0].get("month_label") or ""
            if m_lbl and (cx - last_month_x >= 34):
                c.create_text(cx, top_pad - 10, text=m_lbl, font=("Segoe UI", 7, "bold"), fill=self.C_TEXT_MUTED, anchor="w")
                last_month_x = cx

            for r_idx, day_obj in enumerate(week):
                cy = top_pad + r_idx * (cell_size + cell_gap)

                is_fut = day_obj.get("is_future", False)
                is_tod = day_obj.get("is_today", False)
                intens = day_obj.get("intensity", 0)

                fill_col = "#141414" if is_fut else HEAT_COLORS[intens]
                outline_col = "#ffffff" if is_tod else (self.C_BORDER if intens == 0 else "")

                is_hov = (self.hovered_heat_cell and self.hovered_heat_cell.get("date_str") == day_obj.get("date_str"))
                if is_hov:
                    fill_col = "#ffffff"
                    outline_col = "#38c77b"

                c.create_rectangle(cx, cy, cx + cell_size, cy + cell_size, fill=fill_col, outline=outline_col, width=1)
                self.heat_rects.append((cx, cy, cx + cell_size, cy + cell_size, day_obj))

        # Bottom Heatmap Summary & Legend
        streak = hm.get("streak", 0)
        act_days = hm.get("active_days", 0)
        tot_steps = hm.get("total_steps", 0)
        tot_toks = hm.get("total_tokens", 0)

        summary_txt = f"{tot_steps:,} steps  ·  {format_tokens(tot_toks)} tokens  ·  {act_days} active days  ·  🔥 {streak}d streak"
        c.create_text(left_pad, h - 8, text=summary_txt, font=("Segoe UI", 8), fill=self.C_TEXT_MUTED, anchor="w")

        # Legend (Less ▪ ▪ ▪ ▪ ▪ More)
        leg_x = w - right_pad - 120
        c.create_text(leg_x, h - 8, text="Less", font=("Segoe UI", 7), fill=self.C_TEXT_MUTED, anchor="e")
        for i, col in enumerate(HEAT_COLORS):
            lx1 = leg_x + 6 + i * 11
            lx2 = lx1 + 8
            c.create_rectangle(lx1, h - 13, lx2, h - 5, fill=col, outline="")
        c.create_text(leg_x + 68, h - 8, text="More", font=("Segoe UI", 7), fill=self.C_TEXT_MUTED, anchor="w")

        # Update Subtitle on Cell Hover
        if self.hovered_heat_cell:
            hc = self.hovered_heat_cell
            self.lbl_chart_sub.config(
                text=f"{hc.get('display_date')}  —  Tokens: {format_tokens(hc.get('tokens', 0))}  ·  Steps: {hc.get('steps', 0):,}  ·  Cost: {format_cost(hc.get('cost', 0))}",
                fg=self.C_TEXT_MAIN
            )
        else:
            self.lbl_chart_sub.config(
                text="52-week contribution grid with active streak and daily token intensity.",
                fg=self.C_TEXT_MUTED
            )

    # -----------------------------------------------------------------
    # Rendering Views Data
    # -----------------------------------------------------------------
    def render_dashboard_view(self):
        d = self.dash_data
        q = self.quota_data

        # 1. KPI Cards
        cost_str = format_cost(d.get("total_cost", 0.0))
        reqs_str = f"{d.get('total_requests', 0):,}"
        toks_str = format_tokens(d.get("total_tokens", 0))

        self.lbl_kpi_cost.config(text=cost_str)
        self.lbl_kpi_reqs.config(text=reqs_str)
        self.lbl_kpi_toks.config(text=toks_str)

        cache_pct = d.get("cache_hit_pct", 0.0)
        self.lbl_kpi_reqs_sub.config(text=f"Cache Hit: {cache_pct:.1f}%")

        in_fmt = format_tokens(d.get("total_input", 0))
        cache_fmt = format_tokens(d.get("total_cache", 0))
        out_fmt = format_tokens(d.get("total_output", 0))
        self.lbl_kpi_toks_sub.config(text=f"In: {in_fmt} · Cache: {cache_fmt} · Out: {out_fmt}")

        # 2. Chart Rendering
        self.draw_chart()

        # 3. Usage by Model Table
        m_cnt = d.get("models_count", 0)
        p_cnt = d.get("providers_count", 1)
        self.lbl_models_pill.config(text=f"{m_cnt} models · {p_cnt} providers")

        models_table = d.get("models_table", [])
        for i, r_ui in enumerate(self.table_rows):
            if i < len(models_table):
                m = models_table[i]
                r_ui["div"].pack(fill=tk.X)
                r_ui["frame"].pack(fill=tk.X)

                badge = m.get("badge", "GO")
                badge_bg = self.C_PILL_BG
                if badge == "GO":
                    badge_fg = self.C_BLUE
                elif badge == "CL":
                    badge_fg = self.C_PURPLE
                else:
                    badge_fg = self.C_GREEN

                r_ui["badge"].config(text=badge, fg=badge_fg, bg=badge_bg)
                r_ui["name"].config(text=m.get("label", "Model")[:28])
                r_ui["req"].config(text=f"{m['requests']:,}" if m["requests"] > 0 else "0")
                r_ui["in"].config(text=format_tokens(m["input"]) if m["input"] > 0 else "-")
                r_ui["cache"].config(text=format_tokens(m["cache_read"]) if m["cache_read"] > 0 else "-")

                rem = m.get("remaining_pct")
                reset_rel = m.get("reset_relative", "")
                if rem is not None:
                    q_col = self.C_GREEN if rem >= 50 else (self.C_AMBER if rem >= 20 else self.C_RED)
                    quota_txt = f"{rem:.1f}%"
                    if reset_rel:
                        quota_txt += f" ({reset_rel})"
                    r_ui["quota"].config(text=quota_txt, fg=q_col)
                else:
                    r_ui["quota"].config(text="-", fg=self.C_TEXT_MUTED)

                r_ui["out"].config(text=format_tokens(m["output"]) if m["output"] > 0 else "-")
                r_ui["cost"].config(text=format_cost(m["cost"]))
            else:
                r_ui["div"].pack_forget()
                r_ui["frame"].pack_forget()

    def render_sessions_table(self):
        for w in self.rendered_session_widgets:
            w.destroy()
        self.rendered_session_widgets = []

        all_sessions = self.dash_data.get("sessions_list", [])
        filtered = []
        q = self.session_search_query
        for s in all_sessions:
            if not q or (q in s["title"].lower() or q in s["model"].lower() or q in s["session_id"].lower()):
                filtered.append(s)

        self.tab_buttons["sessions"].config(text=f"Sessions ({len(all_sessions)})")
        self.lbl_sess_title.config(text=f"Recent Antigravity Sessions ({len(filtered)} matches)" if q else f"Recent Antigravity Sessions ({len(all_sessions)})")

        for s in filtered:
            row_f = tk.Frame(self.sess_scrollable_frame, bg=self.C_CARD, padx=12, pady=7)
            row_f.pack(fill=tk.X)
            self._setup_session_columns(row_f)

            # Col 0: Title + ID subtitle
            c0 = tk.Frame(row_f, bg=self.C_CARD)
            c0.grid(row=0, column=0, sticky="w")
            lbl_t = tk.Label(c0, text=s["title"], font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD, anchor="w")
            lbl_t.pack(anchor="w")
            lbl_sub = tk.Label(c0, text=s["session_id"][:12] + "...", font=("Consolas", 6), fg=self.C_TEXT_MUTED, bg=self.C_CARD, anchor="w")
            lbl_sub.pack(anchor="w")

            # Col 1: Date / Time
            lbl_dt = tk.Label(row_f, text=s["last_modified_full"], font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD, anchor="w")
            lbl_dt.grid(row=0, column=1, sticky="w")

            # Col 2: Model + Badge
            c2 = tk.Frame(row_f, bg=self.C_CARD)
            c2.grid(row=0, column=2, sticky="w")
            badge = s.get("badge", "GO")
            badge_fg = self.C_BLUE if badge == "GO" else (self.C_PURPLE if badge == "CL" else self.C_GREEN)
            tk.Label(c2, text=badge, font=("Consolas", 6, "bold"), fg=badge_fg, bg=self.C_PILL_BG, padx=3, pady=0).pack(side=tk.LEFT, padx=(0, 5))
            lbl_m = tk.Label(c2, text=s["model"][:18], font=("Segoe UI", 8), fg=self.C_TEXT_MAIN, bg=self.C_CARD, anchor="w")
            lbl_m.pack(side=tk.LEFT)

            # Numerical Columns
            lbl_steps = tk.Label(row_f, text=f"{s['step_count']:,}", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD, anchor="e")
            lbl_steps.grid(row=0, column=3, sticky="e")

            lbl_reqs = tk.Label(row_f, text=f"{s['requests']:,}", font=("Segoe UI", 8), fg=self.C_TEXT_MAIN, bg=self.C_CARD, anchor="e")
            lbl_reqs.grid(row=0, column=4, sticky="e")

            lbl_in = tk.Label(row_f, text=format_tokens(s["input_tokens"]), font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD, anchor="e")
            lbl_in.grid(row=0, column=5, sticky="e")

            lbl_cache = tk.Label(row_f, text=format_tokens(s["cache_tokens"]), font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD, anchor="e")
            lbl_cache.grid(row=0, column=6, sticky="e")

            lbl_out = tk.Label(row_f, text=format_tokens(s["output_tokens"]), font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD, anchor="e")
            lbl_out.grid(row=0, column=7, sticky="e")

            lbl_tot = tk.Label(row_f, text=format_tokens(s["total_tokens"]), font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD, anchor="e")
            lbl_tot.grid(row=0, column=8, sticky="e")

            lbl_cost = tk.Label(row_f, text=format_cost(s["cost"]), font=("Segoe UI", 8, "bold"), fg=self.C_GREEN, bg=self.C_CARD, anchor="e")
            lbl_cost.grid(row=0, column=9, sticky="e")

            # 1px Divider
            div = tk.Frame(self.sess_scrollable_frame, bg=self.C_DIVIDER, height=1)
            div.pack(fill=tk.X)

            # Row Hover Effect
            w_list = [row_f, c0, lbl_t, lbl_sub, lbl_dt, c2, lbl_m, lbl_steps, lbl_reqs, lbl_in, lbl_cache, lbl_out, lbl_tot, lbl_cost]
            for w in w_list:
                w.bind("<Enter>", lambda e, rf=row_f, c_a=c0, c_b=c2: self._highlight_session_row(rf, c_a, c_b, True))
                w.bind("<Leave>", lambda e, rf=row_f, c_a=c0, c_b=c2: self._highlight_session_row(rf, c_a, c_b, False))

            self.rendered_session_widgets.extend([row_f, div])

    def _highlight_session_row(self, row_frame, c0, c2, is_hover):
        bg_col = self.C_CARD_HOVER if is_hover else self.C_CARD
        row_frame.configure(bg=bg_col)
        c0.configure(bg=bg_col)
        c2.configure(bg=bg_col)
        for child in row_frame.winfo_children():
            child.configure(bg=bg_col)
        for child in c0.winfo_children():
            child.configure(bg=bg_col)
        for child in c2.winfo_children():
            if not isinstance(child, tk.Label) or child.cget("text") not in ("GO", "CL", "OA"):
                child.configure(bg=bg_col)

    def render_quotas_view(self):
        for w in self.rendered_quota_widgets:
            w.destroy()
        self.rendered_quota_widgets = []

        q = self.quota_data
        models = q.get("models", [])

        # Connection & Tier Pill
        if q.get("connected"):
            tier_name = q.get("tier", "Google AI Pro")
            self.lbl_quotas_conn_pill.config(text=f"● Connected · {tier_name} · {len(models)} Models", fg=self.C_GREEN)
        else:
            self.lbl_quotas_conn_pill.config(text="● Reconnecting RPC...", fg=self.C_AMBER)

        # Prompt Credits
        pc = q.get("prompt_credits") or {}
        if pc.get("available") is not None:
            avail = pc.get("available", 0)
            monthly = pc.get("monthly", 50000)
            self.lbl_credits_pill.config(text=f"Credits: {avail:,} / {monthly:,}")
        else:
            self.lbl_credits_pill.config(text="Credits: Standard")

        # Group Models by Provider (Google, Anthropic, OpenAI)
        groups = {"Google": [], "Anthropic": [], "OpenAI": []}
        for m in models:
            prov = m.get("provider", "Google")
            groups.setdefault(prov, []).append(m)

        for prov_name in ["Google", "Anthropic", "OpenAI"]:
            m_list = groups.get(prov_name, [])
            if not m_list:
                continue

            sec_hdr = tk.Frame(self.quotas_scrollable_frame, bg=self.C_CARD, pady=6)
            sec_hdr.pack(fill=tk.X)
            tk.Label(sec_hdr, text=f"{prov_name.upper()} MODELS ({len(m_list)})", font=("Segoe UI", 9, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD).pack(anchor="w")

            grid_f = tk.Frame(self.quotas_scrollable_frame, bg=self.C_CARD)
            grid_f.pack(fill=tk.X, pady=(0, 10))
            grid_f.columnconfigure(0, weight=1, uniform="quota_card")
            grid_f.columnconfigure(1, weight=1, uniform="quota_card")

            for idx, m in enumerate(m_list):
                card = tk.Frame(grid_f, bg=self.C_CARD_HEADER, highlightbackground=self.C_BORDER, highlightthickness=1, padx=14, pady=10)
                card.grid(row=idx // 2, column=idx % 2, sticky="nsew", padx=5, pady=5)

                top_r = tk.Frame(card, bg=self.C_CARD_HEADER)
                top_r.pack(fill=tk.X)

                badge = m.get("badge", "GO")
                badge_fg = self.C_BLUE if badge == "GO" else (self.C_PURPLE if badge == "CL" else self.C_GREEN)
                tk.Label(top_r, text=badge, font=("Consolas", 7, "bold"), fg=badge_fg, bg=self.C_PILL_BG, padx=4, pady=1).pack(side=tk.LEFT, padx=(0, 6))

                lbl_name = tk.Label(top_r, text=m["label"], font=("Segoe UI", 9, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD_HEADER)
                lbl_name.pack(side=tk.LEFT)

                if m.get("is_recommended"):
                    tk.Label(top_r, text="★ REC", font=("Segoe UI", 6, "bold"), fg=self.C_AMBER, bg=self.C_PILL_BG, padx=4, pady=1).pack(side=tk.LEFT, padx=(6, 0))

                rem = m.get("remaining_pct", 100.0)
                q_col = self.C_GREEN if rem >= 50 else (self.C_AMBER if rem >= 20 else self.C_RED)
                lbl_pct = tk.Label(top_r, text=f"{rem:.1f}%", font=("Segoe UI", 11, "bold"), fg=q_col, bg=self.C_CARD_HEADER)
                lbl_pct.pack(side=tk.RIGHT)

                # Progress Bar with dynamic resize tracking
                pbar_canvas = tk.Canvas(card, height=6, bg="#242424", highlightthickness=0)
                pbar_canvas.pack(fill=tk.X, pady=(8, 6))

                def _draw_pbar(ev, c=pbar_canvas, pct=rem, col=q_col):
                    c.delete("all")
                    bar_w = int((pct / 100.0) * ev.width)
                    c.create_rectangle(0, 0, bar_w, 6, fill=col, outline="")

                pbar_canvas.bind("<Configure>", _draw_pbar)

                # Bottom Info (Reset timer + model id)
                bot_r = tk.Frame(card, bg=self.C_CARD_HEADER)
                bot_r.pack(fill=tk.X)

                reset_txt = f"⏳ Resets in {m['reset_relative']}" if m.get("reset_relative") else "⏳ Ready"
                tk.Label(bot_r, text=reset_txt, font=("Segoe UI", 7), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER).pack(side=tk.LEFT)

                m_id = m.get("model_id", "")
                if m_id:
                    tk.Label(bot_r, text=m_id[:20], font=("Consolas", 6), fg=self.C_TEXT_DIM, bg=self.C_CARD_HEADER).pack(side=tk.RIGHT)

            self.rendered_quota_widgets.extend([sec_hdr, grid_f])

    # -----------------------------------------------------------------
    # Copy Telemetry Markdown Report
    # -----------------------------------------------------------------
    def copy_telemetry_report(self):
        try:
            d = self.dash_data
            q = self.quota_data
            pm = q.get("primary_model") or {}
            range_name = RANGE_MAP.get(self.selected_range, "Selected Range")

            lines = [
                f"# Antigravity Telemetry Report ({range_name})",
                f"- **Generated At**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
                f"- **Total Cost**: {format_cost(d.get('total_cost', 0))} | **Requests**: {d.get('total_requests', 0):,} | **Tokens**: {format_tokens(d.get('total_tokens', 0))}",
                f"- **Cache Hit %**: {d.get('cache_hit_pct', 0.0):.1f}% (Input: {format_tokens(d.get('total_input', 0))} · Cache Read: {format_tokens(d.get('total_cache', 0))} · Output: {format_tokens(d.get('total_output', 0))})",
                f"- **Live Quota**: {pm.get('label', 'Gemini')} ({pm.get('remaining_pct', 100):.1f}% remaining · Resets: {pm.get('reset_relative', 'Ready')})",
                "",
                "### Usage by Model",
                "| Model | Provider | Requests | Input | Cache Read | Live Quota | Output | Cost |",
                "|---|---|---:|---:|---:|---:|---:|---:|"
            ]
            for m in d.get("models_table", [])[:8]:
                q_str = f"{m['remaining_pct']:.1f}%" if m.get("remaining_pct") is not None else "-"
                lines.append(
                    f"| {m['label']} | {m['provider']} | {m['requests']:,} | {format_tokens(m['input'])} | {format_tokens(m['cache_read'])} | {q_str} | {format_tokens(m['output'])} | {format_cost(m['cost'])} |"
                )

            lines.append("")
            lines.append("### Recent Sessions (Top 10)",)
            lines.append("| Title | Date | Model | Steps | Tokens | Cost |")
            lines.append("|---|---|---|---:|---:|---:|")
            for s in d.get("sessions_list", [])[:10]:
                lines.append(
                    f"| {s['title']} | {s['last_modified_full']} | {s['model']} | {s['step_count']:,} | {format_tokens(s['total_tokens'])} | {format_cost(s['cost'])} |"
                )

            report_txt = "\n".join(lines)
            self.clipboard_clear()
            self.clipboard_append(report_txt)

            self.btn_export.config(text="✓ Copied!", fg=self.C_GREEN)
            self.after(1600, lambda: self.btn_export.config(text="⤓ Export", fg=self.C_TEXT_MAIN))
        except Exception:
            pass

    # -----------------------------------------------------------------
    # Background Polling & Main Update Loop
    # -----------------------------------------------------------------
    def poll_once(self):
        try:
            self.proc_stats = AntigravityDataCollector.get_process_stats()
            self.quota_data = AntigravityDataCollector.get_real_quota_data()
            self.dash_data = AntigravityDataCollector.get_dashboard_data(
                selected_range=self.selected_range,
                quota_data=self.quota_data
            )
        except Exception:
            pass

    def data_poll_worker(self):
        while self.running:
            self.poll_once()
            time.sleep(2.0)

    def update_ui_loop(self):
        try:
            q = self.quota_data

            # 1. Top Bar Live RPC Status Pill
            if q.get("connected"):
                pm = q.get("primary_model") or {}
                sm = q.get("secondary_model") or {}
                p_pct = pm.get("remaining_pct", 100.0)
                p_rst = f" ({pm['reset_relative']})" if pm.get("reset_relative") else ""
                s_pct = sm.get("remaining_pct", 100.0)
                tier_str = q.get('tier', 'Pro').replace('Google AI ', '')
                pill_txt = f"● {tier_str} · Gem: {p_pct:.0f}%{p_rst} · Claude: {s_pct:.0f}%"
                pill_fg = self.C_GREEN if p_pct >= 40 else (self.C_AMBER if p_pct >= 15 else self.C_RED)
                self.lbl_rpc_badge.config(text=pill_txt, fg=pill_fg)
            else:
                self.lbl_rpc_badge.config(text="● Reconnecting RPC...", fg=self.C_AMBER)

            # 2. Render Active View
            if self.current_view == "dashboard":
                self.render_dashboard_view()
            elif self.current_view == "sessions":
                all_cnt = len(self.dash_data.get("sessions_list", []))
                self.tab_buttons["sessions"].config(text=f"Sessions ({all_cnt})")
            elif self.current_view == "quotas":
                self.render_quotas_view()

        except Exception:
            pass

        self.after(800, self.update_ui_loop)

    def on_window_configure(self, event):
        if event.widget == self:
            if hasattr(self, "canvas_chart") and self.current_view == "dashboard":
                self.draw_chart()

    def on_closing(self):
        self.running = False
        try:
            self.settings["window_w"] = self.winfo_width()
            self.settings["window_h"] = self.winfo_height()
            self.settings["window_x"] = self.winfo_x()
            self.settings["window_y"] = self.winfo_y()
            self.settings["selected_range"] = self.selected_range
            self.settings["selected_view"] = self.current_view
            self.settings["chart_mode"] = self.chart_mode
            self.settings["is_pinned"] = self.is_pinned
            save_settings(self.settings)
        except Exception:
            pass
        self.destroy()


if __name__ == "__main__":
    app = DesktopUsageStudio()
    app.mainloop()
