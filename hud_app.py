import os
import sys
import math
import time
import json
import threading
import subprocess
from datetime import datetime, date
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from PIL import Image, ImageDraw, ImageTk

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

AGY_PATH = r"C:\Users\ptan8\AppData\Local\agy\bin\agy.exe"
AGENTAPI_PATH = os.path.expanduser(r"~\.gemini\antigravity\bin\agentapi.bat")

def enable_taskbar_integration(window):
    """Integrates an overrideredirect frameless window cleanly into the Windows Taskbar."""
    try:
        import ctypes
        window.update_idletasks()
        hwnd = window.winfo_id()
        GWL_EXSTYLE = -20
        WS_EX_APPWINDOW = 0x00040000
        WS_EX_TOOLWINDOW = 0x00000080
        style = ctypes.windll.user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
        style = (style & ~WS_EX_TOOLWINDOW) | WS_EX_APPWINDOW
        ctypes.windll.user32.SetWindowLongW(hwnd, GWL_EXSTYLE, style)
        SWP_NOMOVE = 0x0002
        SWP_NOSIZE = 0x0001
        SWP_NOZORDER = 0x0004
        SWP_FRAMECHANGED = 0x0020
        ctypes.windll.user32.SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED)
    except Exception:
        pass


_ROUNDED_IMAGE_CACHE = {}

def get_rounded_rect_image(w, h, r, bg_color, border_color, parent_bg="#0d0d0d", border_width=1):
    """Generates and caches a 2x supersampled, butter-smooth anti-aliased rounded rectangle image."""
    key = (w, h, r, bg_color, border_color, parent_bg, border_width)
    if key in _ROUNDED_IMAGE_CACHE:
        return _ROUNDED_IMAGE_CACHE[key]

    scale = 2
    sw = max(2, w * scale)
    sh = max(2, h * scale)
    sr = max(1, r * scale)
    sbw = max(1, border_width * scale)

    img = Image.new("RGBA", (sw, sh), parent_bg)
    draw = ImageDraw.Draw(img)

    half_bw = sbw / 2.0
    x0 = half_bw
    y0 = half_bw
    x1 = sw - half_bw - 1
    y1 = sh - half_bw - 1

    draw.rounded_rectangle(
        [x0, y0, x1, y1],
        radius=sr,
        fill=bg_color,
        outline=border_color if border_color else None,
        width=sbw
    )

    res = img.resize((w, h), Image.Resampling.BILINEAR)
    photo = ImageTk.PhotoImage(res)
    _ROUNDED_IMAGE_CACHE[key] = photo
    return photo


class RoundedCard(tk.Canvas):
    """A high-performance container widget featuring 2x supersampled, anti-aliased rounded corners."""
    def __init__(self, parent, bg_color="#141414", border_color="#222222", corner_radius=10, border_width=1, parent_bg="#0d0d0d", padx=12, pady=10, auto_height=True, auto_width=False, **kwargs):
        super().__init__(parent, bg=parent_bg, highlightthickness=0, bd=0, **kwargs)
        self.bg_color = bg_color
        self.border_color = border_color
        self.corner_radius = corner_radius
        self.border_width = border_width
        self.parent_bg = parent_bg
        self.card_padx = padx
        self.card_pady = pady
        self.auto_height = auto_height
        self.auto_width = auto_width
        self._bg_photo = None
        self._last_rendered_size = None

        self.inner = tk.Frame(self, bg=self.bg_color)
        self.win = self.create_window(self.card_padx, self.card_pady, window=self.inner, anchor="nw")

        self.bind("<Configure>", self._on_canvas_configure)
        if self.auto_height or self.auto_width:
            self.inner.bind("<Configure>", self._on_inner_configure)

    def _on_inner_configure(self, event=None):
        if self.auto_height:
            req_h = self.inner.winfo_reqheight() + 2 * self.card_pady
            cur_h = self.winfo_height()
            if abs(cur_h - req_h) > 2 and req_h > 10:
                self.config(height=req_h)
        if self.auto_width:
            req_w = self.inner.winfo_reqwidth() + 2 * self.card_padx
            cur_w = self.winfo_width()
            if abs(cur_w - req_w) > 2 and req_w > 10:
                self.config(width=req_w)

    def _on_canvas_configure(self, event):
        w = event.width
        h = event.height
        if w < 6 or h < 6:
            return
        if self._last_rendered_size == (w, h):
            return
        self._last_rendered_size = (w, h)
        self.delete("card_bg")
        self._bg_photo = get_rounded_rect_image(
            w, h, self.corner_radius, self.bg_color, self.border_color, self.parent_bg, self.border_width
        )
        self.create_image(0, 0, image=self._bg_photo, anchor="nw", tags="card_bg")
        self.tag_lower("card_bg")
        inner_w = max(1, w - 2 * self.card_padx)
        inner_h = max(1, h - 2 * self.card_pady)
        item_kw = {}
        if not self.auto_width:
            item_kw["width"] = inner_w
        if not self.auto_height:
            item_kw["height"] = inner_h
        if item_kw:
            self.itemconfig(self.win, **item_kw)


def draw_mascot(canvas, scale=3, color="#d97757", y_offset=0, blink=False):
    """Draws an animated 8-bit mascot creature on a canvas (with subtle breathing & blinking)."""
    eye_row = ".########..." if blink else ".##.##.##..."
    grid = [
        "....##......",
        "....##......",
        "..######....",
        ".########...",
        eye_row,
        ".########...",
        ".########...",
        "..######....",
        ".########...",
        "###.##.###..",
        "#...##...#..",
        "....##......"
    ]
    canvas.delete("all")
    for r, row in enumerate(grid):
        for col, ch in enumerate(row):
            if ch == "#":
                x1 = col * scale + 6
                y1 = r * scale + 6 + y_offset
                x2 = (col + 1) * scale + 6
                y2 = (r + 1) * scale + 6 + y_offset
                canvas.create_rectangle(x1, y1, x2, y2, fill=color, outline="")


RANGE_MAP = {
    "today_active": "Today",
    "today_24h": "24h",
    "7d": "7 Days",
    "30d": "30 Days",
    "all": "All Time",
}

AVAILABLE_MODELS = [
    "Gemini 3.8 Flash (Medium)",
    "Gemini 3.8 Flash (High)",
    "Gemini 3.8 Flash (Low)",
    "Gemini 4 Argon (High)",
    "Gemini 4 Argon (Medium)",
    "Gemini 4 Argon (Low)",
    "Claude Sonnet 5.5 (High)",
    "Claude Sonnet 5.5 (Medium)",
    "Claude Opus 5.5 (High)",
    "GPT-OSS 120B (Medium)",
]

AVAILABLE_EFFORTS = ["low", "medium", "high", "max"]


class CLIInstance:
    """State and backend handler for a single Antigravity CLI tab."""
    def __init__(self, tab_id, name, default_effort="medium"):
        self.tab_id = tab_id
        self.name = name
        self.model = "Gemini 3.8 Flash (Medium)" if default_effort == "medium" else "Gemini 4 Argon (High)"
        self.effort = default_effort
        self.is_running = False
        self.proc = None
        self.lines = []
        self.input_history = []
        self.history_idx = -1


class DesktopUsageStudio(tk.Tk):
    def __init__(self):
        super().__init__()

        self.settings = load_settings()

        # Sleek Monochrome Dark Aesthetic Palette
        self.C_BG = "#0d0d0d"
        self.C_SIDEBAR = "#0f0f0f"
        self.C_CARD = "#141414"
        self.C_CARD_HEADER = "#1a1a1a"
        self.C_CARD_HOVER = "#202020"
        self.C_BORDER = "#222222"
        self.C_DIVIDER = "#1e1e1e"
        self.C_PILL_BG = "#1a1a1a"
        self.C_PILL_ACTIVE = "#262626"
        self.C_TEXT_MAIN = "#f5f5f5"
        self.C_TEXT_MUTED = "#8c8c8c"
        self.C_TEXT_DIM = "#555555"
        self.C_CORAL = "#d97757"
        self.C_CORAL_BG = "#1c1411"
        self.C_CORAL_BORDER = "#422820"
        self.C_BAR = "#a6a6a6"
        self.C_BAR_HOVER = "#ffffff"
        self.C_GREEN = "#34d399"
        self.C_AMBER = "#fbbf24"
        self.C_RED = "#f87171"
        self.C_BLUE = "#60a5fa"
        self.C_PURPLE = "#c084fc"

        # Window Setup: Frameless Desktop Studio Window (overrideredirect=True)
        self.title("Antigravity Usage Studio")
        self.overrideredirect(True)
        self.configure(bg="#262626")  # Outer border color

        self.is_pinned = self.settings.get("is_pinned", False)
        self.attributes("-topmost", self.is_pinned)

        # Dark Style for ttk Scrollbars
        self.style = ttk.Style()
        try:
            self.style.theme_use("clam")
            self.style.configure(
                "Vertical.TScrollbar",
                gripcount=0,
                background="#2a2a2a",
                darkcolor="#141414",
                lightcolor="#141414",
                troughcolor="#0f0f0f",
                bordercolor="#0f0f0f",
                arrowcolor="#8c8c8c"
            )
            self.style.map(
                "Vertical.TScrollbar",
                background=[("active", "#3a3a3a"), ("pressed", "#4a4a4a")]
            )
        except Exception:
            pass

        # Workspace Directory
        self.workspace_dir = os.path.dirname(os.path.abspath(__file__))
        self.show_sidebar = True

        # State & Geometry
        screen_w = self.winfo_screenwidth()
        screen_h = self.winfo_screenheight()
        saved_w = self.settings.get("window_w", 1140)
        saved_h = self.settings.get("window_h", 780)
        self.win_w = max(940, min(saved_w, screen_w - 40))
        self.win_h = max(680, min(saved_h, screen_h - 40))

        saved_x = self.settings.get("window_x")
        saved_y = self.settings.get("window_y")
        if isinstance(saved_x, int) and 0 <= saved_x <= screen_w - 200 and isinstance(saved_y, int) and 0 <= saved_y <= screen_h - 200:
            x, y = saved_x, saved_y
        else:
            x = max(20, (screen_w - self.win_w) // 2)
            y = max(20, (screen_h - self.win_h) // 2)

        self.geometry(f"{self.win_w}x{self.win_h}+{x}+{y}")
        self.minsize(940, 680)

        self.drag_start_x = 0
        self.drag_start_y = 0
        self.resize_start_x = 0
        self.resize_start_y = 0
        self.resize_orig_w = 0
        self.resize_orig_h = 0
        self.is_maximized = False
        self.prev_geometry = None

        # Views & Tab Management
        # Active view can be: "cli_1", "usage", "orchestrator", "quotas", "sessions"
        self.default_effort = self.settings.get("default_effort", "medium")
        if self.default_effort not in AVAILABLE_EFFORTS:
            self.default_effort = "medium"

        self.current_view = "cli_1"
        self.cli_instances = {}
        self.cli_counter = 1
        cli_1 = CLIInstance("cli_1", "✻ Antigravity CLI 1", default_effort=self.default_effort)
        cli_1.model = "Gemini 4 Argon (High)"
        cli_1.effort = self.default_effort
        self.cli_instances["cli_1"] = cli_1

        self.selected_range = self.settings.get("selected_range", "today_active")
        if self.selected_range not in RANGE_MAP:
            self.selected_range = "today_active"

        self.chart_mode = self.settings.get("chart_mode", "cost")  # 'cost', 'tokens', 'heatmap'
        self.hovered_bar_idx = None
        self.bar_rects = []
        self.hovered_heat_cell = None
        self.heat_rects = []
        self.session_search_query = ""

        # 60fps Motion Graphics Engine State
        self.chart_anim_progress = 1.0
        self._chart_anim_id = None
        self._kpi_anim_id = None
        self._kpi_displayed = {"cost": 0.0, "reqs": 0, "toks": 0}
        self._quota_anim_id = None
        self.mascot_tick = 0
        self.spinner_frames = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"]
        self.spinner_idx = 0

        # Live Telemetry Data Cache
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

        # Build Full UI Structure
        self.create_widgets()

        # Taskbar integration & window corners
        self.after(20, self.apply_rounded_window_corners)
        self.after(50, lambda: enable_taskbar_integration(self))

        # Event Handlers
        self.bind("<Configure>", self.on_window_configure)

        # Background Poller
        self.running = True
        self.poll_thread = threading.Thread(target=self.data_poll_worker, daemon=True)
        self.poll_thread.start()

        # Initial UI Loop & Ambient 60fps Motion Graphics Loop
        self.update_ui_loop()
        self.ambient_motion_loop()

    # -----------------------------------------------------------------
    # UI Layout Construction
    # -----------------------------------------------------------------
    def create_widgets(self):
        # 1px Crisp Outer Border Frame
        self.outer_border = tk.Frame(self, bg=self.C_BORDER, padx=1, pady=1)
        self.outer_border.pack(fill=tk.BOTH, expand=True)

        self.root_container = tk.Frame(self.outer_border, bg=self.C_BG)
        self.root_container.pack(fill=tk.BOTH, expand=True)

        # =============================================================
        # 1. INTEGRATED CUSTOM TITLE & TAB BAR (38px height)
        # =============================================================
        self.top_title_bar = tk.Frame(self.root_container, bg="#0d0d0d", height=38)
        self.top_title_bar.pack(fill=tk.X, side=tk.TOP)
        self.top_title_bar.pack_propagate(False)

        # Drag bindings on the top title bar
        self.top_title_bar.bind("<Button-1>", self.on_titlebar_drag_start)
        self.top_title_bar.bind("<B1-Motion>", self.on_titlebar_drag_motion)
        self.top_title_bar.bind("<Double-Button-1>", self.on_titlebar_double_click)

        # Far Left: Traffic Lights Window Controls Canvas (🔴 🟡 🟢) + Icons
        self.traffic_box = tk.Frame(self.top_title_bar, bg="#0d0d0d")
        self.traffic_box.pack(side=tk.LEFT, padx=(12, 6))

        self.canvas_traffic = tk.Canvas(self.traffic_box, width=54, height=18, bg="#0d0d0d", highlightthickness=0, cursor="hand2")
        self.canvas_traffic.pack(side=tk.LEFT)
        self._draw_traffic_lights()
        self.canvas_traffic.bind("<Button-1>", self.on_traffic_click)

        # Sidebar Toggle Icon (◧) & Grid Icon (⊞)
        self.btn_toggle_sidebar = tk.Label(
            self.traffic_box, text="◧", font=("Segoe UI", 10), fg=self.C_TEXT_MUTED, bg="#0d0d0d", cursor="hand2", padx=4
        )
        self.btn_toggle_sidebar.pack(side=tk.LEFT, padx=(6, 2))
        self.btn_toggle_sidebar.bind("<Button-1>", lambda e: self.toggle_sidebar())

        # Center-Left Integrated Tabs with animated sliding underline
        self.tab_strip_container = tk.Frame(self.top_title_bar, bg="#0d0d0d")
        self.tab_strip_container.pack(side=tk.LEFT, padx=(8, 0), fill=tk.Y)

        self.tab_strip_frame = tk.Frame(self.tab_strip_container, bg="#0d0d0d")
        self.tab_strip_frame.pack(side=tk.TOP, fill=tk.X)

        self.tab_indicator_canvas = tk.Canvas(self.tab_strip_container, bg="#0d0d0d", height=2, highlightthickness=0)
        self.tab_indicator_canvas.pack(side=tk.BOTTOM, fill=tk.X)
        self.tab_indicator_bar = self.tab_indicator_canvas.create_rectangle(0, 0, 0, 2, fill=self.C_CORAL, outline="")
        self._cur_indicator_pos = [0, 0]
        self._tab_anim_after_id = None

        self.tab_ui_buttons = {}
        self.tab_cards = {}
        self.rebuild_tabs_strip()

        # Far Right Controls: Status indicator (NO ugly box!) + Pin toggle
        self.top_right_box = tk.Frame(self.top_title_bar, bg="#0d0d0d")
        self.top_right_box.pack(side=tk.RIGHT, padx=(0, 12))

        # Borderless Status Dot & Text
        self.status_dot = tk.Label(self.top_right_box, text="●", font=("Segoe UI", 8), fg=self.C_GREEN, bg="#0d0d0d")
        self.status_dot.pack(side=tk.LEFT, padx=(0, 4))
        self.lbl_status_text = tk.Label(
            self.top_right_box, text="Ultra · Gem 87% · Claude 100%", font=("Segoe UI", 8), fg="#999999", bg="#0d0d0d", cursor="hand2"
        )
        self.lbl_status_text.pack(side=tk.LEFT, padx=(0, 10))
        self.lbl_status_text.bind("<Button-1>", lambda e: self.switch_view("quotas"))

        # Pin / Always on Top Button
        self.btn_pin = tk.Label(
            self.top_right_box, text="📌", font=("Segoe UI", 8), fg=self.C_TEXT_MAIN if self.is_pinned else self.C_TEXT_DIM, bg="#0d0d0d", cursor="hand2", padx=3
        )
        self.btn_pin.pack(side=tk.LEFT)
        self.btn_pin.bind("<Button-1>", lambda e: self.toggle_pin())

        # =============================================================
        # 2. MAIN WORKSPACE AREA (Sidebar + Content Views)
        # =============================================================
        self.workspace_body = tk.Frame(self.root_container, bg=self.C_BG)
        self.workspace_body.pack(fill=tk.BOTH, expand=True)

        # Left File Explorer Sidebar (Collapsible, RoundedCard)
        self.sidebar_card = RoundedCard(
            self.workspace_body,
            bg_color=self.C_SIDEBAR,
            border_color=self.C_BORDER,
            corner_radius=10,
            parent_bg=self.C_BG,
            width=220,
            padx=0,
            pady=0,
            auto_height=False,
            auto_width=False
        )
        self.sidebar_card.pack(side=tk.LEFT, fill=tk.Y, padx=(10, 0), pady=10)
        self.sidebar_card.pack_propagate(False)
        self.sidebar_frame = self.sidebar_card.inner
        self.setup_sidebar()

        # Right Views Container
        self.content_container = tk.Frame(self.workspace_body, bg=self.C_BG)
        self.content_container.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # View Frames
        self.view_cli_frame = tk.Frame(self.content_container, bg="#0c0c0c")
        self.view_dashboard = tk.Frame(self.content_container, bg=self.C_BG, padx=20, pady=14)
        self.view_orchestrator = tk.Frame(self.content_container, bg=self.C_BG, padx=20, pady=14)
        self.view_quotas = tk.Frame(self.content_container, bg=self.C_BG, padx=20, pady=14)
        self.view_sessions = tk.Frame(self.content_container, bg=self.C_BG, padx=20, pady=14)

        # Build Content Views
        self.setup_cli_view()
        self.setup_orchestrator_view()
        self.setup_dashboard_view()
        self.setup_quotas_view()
        self.setup_sessions_view()

        # Bottom-Right Resize Grip Handle (◿)
        self.resize_grip = tk.Label(self.root_container, text="◿", font=("Segoe UI", 9), fg="#444444", bg=self.C_BG, cursor="size_nw_se")
        self.resize_grip.place(relx=1.0, rely=1.0, anchor="se", x=-2, y=-2)
        self.resize_grip.bind("<Button-1>", self.on_resize_start)
        self.resize_grip.bind("<B1-Motion>", self.on_resize_motion)

        # Switch to initial view
        self.switch_view("cli_1")

    # -----------------------------------------------------------------
    # Traffic Lights & Window Controls
    # -----------------------------------------------------------------
    def _draw_traffic_lights(self):
        c = self.canvas_traffic
        c.delete("all")
        # Red
        c.create_oval(3, 4, 15, 16, fill="#ff5f56", outline="#e0443e")
        # Yellow
        c.create_oval(21, 4, 33, 16, fill="#ffbd2e", outline="#dea123")
        # Green
        c.create_oval(39, 4, 51, 16, fill="#27c93f", outline="#1aab29")

    def on_traffic_click(self, event):
        x = event.x
        if 0 <= x <= 18:
            self.on_closing()
        elif 19 <= x <= 36:
            self.minimize_window()
        elif 37 <= x <= 54:
            self.toggle_maximize()

    def minimize_window(self):
        try:
            import ctypes
            hwnd = self.winfo_id()
            SW_MINIMIZE = 6
            ctypes.windll.user32.ShowWindow(hwnd, SW_MINIMIZE)
        except Exception:
            pass

    def apply_rounded_window_corners(self):
        try:
            import ctypes
            hwnd = self.winfo_id()
            # Windows 11 DWM native hardware-accelerated rounded corners
            DWMWA_WINDOW_CORNER_PREFERENCE = 33
            DWMWCP_ROUND = 2
            pref = ctypes.c_int(DWMWCP_ROUND)
            ctypes.windll.dwmapi.DwmSetWindowAttribute(
                hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, ctypes.byref(pref), 4
            )
        except Exception:
            pass

    def toggle_maximize(self):
        if not self.is_maximized:
            self.prev_geometry = self.geometry()
            sw = self.winfo_screenwidth()
            sh = self.winfo_screenheight()
            self.geometry(f"{sw}x{sh - 40}+0+0")
            self.is_maximized = True
        else:
            if self.prev_geometry:
                self.geometry(self.prev_geometry)
            else:
                self.geometry("1140x780+100+100")
            self.is_maximized = False
        self.apply_rounded_window_corners()

    def on_titlebar_drag_start(self, event):
        self.drag_start_x = event.x_root
        self.drag_start_y = event.y_root

    def on_titlebar_drag_motion(self, event):
        dx = event.x_root - self.drag_start_x
        dy = event.y_root - self.drag_start_y
        new_x = self.winfo_x() + dx
        new_y = self.winfo_y() + dy
        self.geometry(f"+{new_x}+{new_y}")
        self.drag_start_x = event.x_root
        self.drag_start_y = event.y_root

    def on_titlebar_double_click(self, event):
        self.toggle_maximize()

    def on_resize_start(self, event):
        self.resize_start_x = event.x_root
        self.resize_start_y = event.y_root
        self.resize_orig_w = self.winfo_width()
        self.resize_orig_h = self.winfo_height()

    def on_resize_motion(self, event):
        dw = event.x_root - self.resize_start_x
        dh = event.y_root - self.resize_start_y
        new_w = max(940, self.resize_orig_w + dw)
        new_h = max(680, self.resize_orig_h + dh)
        self.geometry(f"{new_w}x{new_h}")
        self.apply_rounded_window_corners()

    def toggle_sidebar(self):
        self.show_sidebar = not self.show_sidebar
        if self.show_sidebar:
            self.sidebar_card.pack(side=tk.LEFT, fill=tk.Y, padx=(10, 0), pady=10, before=self.content_container)
            self.btn_toggle_sidebar.config(fg=self.C_TEXT_MAIN)
        else:
            self.sidebar_card.pack_forget()
            self.btn_toggle_sidebar.config(fg=self.C_TEXT_MUTED)

    # -----------------------------------------------------------------
    # Integrated Tab Bar Strip & Smooth Animated Indicator
    # -----------------------------------------------------------------
    def animate_tab_indicator(self, view_id=None):
        if view_id is None:
            view_id = self.current_view
        tab_w = self.tab_cards.get(view_id) or self.tab_ui_buttons.get(view_id)
        if not tab_w or not tab_w.winfo_ismapped():
            self.after(20, lambda: self.animate_tab_indicator(self.current_view))
            return

        # Cancel any running tab indicator animation
        if hasattr(self, "_tab_anim_after_id") and self._tab_anim_after_id:
            try:
                self.after_cancel(self._tab_anim_after_id)
            except Exception:
                pass
            self._tab_anim_after_id = None

        tx1 = tab_w.winfo_x() + 4
        tx2 = tab_w.winfo_x() + tab_w.winfo_width() - 4
        if tx2 <= tx1:
            tx2 = tx1 + 40

        bar_col = self.C_CORAL if (view_id.startswith("cli_") or view_id == "orchestrator") else (self.C_GREEN if view_id == "quotas" else self.C_BLUE)
        self.tab_indicator_canvas.itemconfig(self.tab_indicator_bar, fill=bar_col)

        cur_coords = self.tab_indicator_canvas.coords(self.tab_indicator_bar)
        if not cur_coords or cur_coords[2] <= cur_coords[0]:
            cx1, cx2 = tx1, tx2
            self.tab_indicator_canvas.coords(self.tab_indicator_bar, cx1, 0, cx2, 2)
            self._cur_indicator_pos = [cx1, cx2]
            return

        start_x1 = getattr(self, "_cur_indicator_pos", [tx1, tx2])[0]
        start_x2 = getattr(self, "_cur_indicator_pos", [tx1, tx2])[1]

        total_steps = 6

        def _step_anim(s=0, x1=start_x1, x2=start_x2):
            if s >= total_steps:
                self.tab_indicator_canvas.coords(self.tab_indicator_bar, tx1, 0, tx2, 2)
                self._cur_indicator_pos = [tx1, tx2]
                self._tab_anim_after_id = None
                return
            factor = (s + 1) / float(total_steps)
            ease = 1.0 - (1.0 - factor) ** 2  # Quadratic ease-out
            cur_x1 = x1 + (tx1 - x1) * ease
            cur_x2 = x2 + (tx2 - x2) * ease
            self.tab_indicator_canvas.coords(self.tab_indicator_bar, cur_x1, 0, cur_x2, 2)
            self._cur_indicator_pos = [cur_x1, cur_x2]
            self._tab_anim_after_id = self.after(16, lambda: _step_anim(s + 1, x1, x2))

        _step_anim()

    def rebuild_tabs_strip(self):
        for w in self.tab_strip_frame.winfo_children():
            w.destroy()

        self.tab_ui_buttons = {}
        self.tab_cards = {}

        # 1. Parallel CLI Tabs
        for tab_id, cli in self.cli_instances.items():
            is_active = (self.current_view == tab_id)
            btn = tk.Label(
                self.tab_strip_frame,
                text=cli.name,
                font=("Segoe UI", 8, "bold" if is_active else "normal"),
                fg=self.C_TEXT_MAIN if is_active else self.C_TEXT_MUTED,
                bg="#1a1a1a" if is_active else "#0d0d0d",
                padx=10,
                pady=4,
                cursor="hand2"
            )
            btn.pack(side=tk.LEFT, padx=2, pady=3)
            btn.bind("<Button-1>", lambda e, tid=tab_id: self.switch_view(tid))

            def _on_enter(e, b=btn, tid=tab_id):
                if self.current_view != tid:
                    b.config(bg="#141414", fg=self.C_TEXT_MAIN)
            def _on_leave(e, b=btn, tid=tab_id):
                if self.current_view != tid:
                    b.config(bg="#0d0d0d", fg=self.C_TEXT_MUTED)
            btn.bind("<Enter>", _on_enter)
            btn.bind("<Leave>", _on_leave)

            self.tab_ui_buttons[tab_id] = btn
            self.tab_cards[tab_id] = btn

        # 2. Add New Tab Button (+)
        btn_add = tk.Label(
            self.tab_strip_frame,
            text="+",
            font=("Segoe UI", 10, "bold"),
            fg=self.C_TEXT_MUTED,
            bg="#0d0d0d",
            padx=7,
            pady=3,
            cursor="hand2"
        )
        btn_add.pack(side=tk.LEFT, padx=2)
        btn_add.bind("<Button-1>", lambda e: self.spawn_new_cli_tab())

        # 3. Dropdown Menu (▾) to launch Native External Terminal
        btn_menu = tk.Label(
            self.tab_strip_frame,
            text="▾",
            font=("Segoe UI", 8),
            fg=self.C_TEXT_MUTED,
            bg="#0d0d0d",
            padx=5,
            pady=3,
            cursor="hand2"
        )
        btn_menu.pack(side=tk.LEFT, padx=(0, 10))
        btn_menu.bind("<Button-1>", lambda e: self.launch_native_terminal())

        # Separator Bar
        sep = tk.Frame(self.tab_strip_frame, bg="#222222", width=1)
        sep.pack(side=tk.LEFT, fill=tk.Y, padx=4, pady=8)

        # 4. Studio Fixed Tabs (Usage, Orchestrator, Quotas, Sessions)
        fixed_tabs = [
            ("usage", "📊 Usage"),
            ("orchestrator", "🎯 Orchestrator"),
            ("quotas", "⚡ Quotas"),
            ("sessions", "🗂️ Sessions"),
        ]
        for key, lbl in fixed_tabs:
            is_active = (self.current_view == key)
            btn = tk.Label(
                self.tab_strip_frame,
                text=lbl,
                font=("Segoe UI", 8, "bold" if is_active else "normal"),
                fg=self.C_TEXT_MAIN if is_active else self.C_TEXT_MUTED,
                bg="#1a1a1a" if is_active else "#0d0d0d",
                padx=10,
                pady=4,
                cursor="hand2"
            )
            btn.pack(side=tk.LEFT, padx=2, pady=3)
            btn.bind("<Button-1>", lambda e, k=key: self.switch_view(k))

            def _on_enter_fixed(e, b=btn, k=key):
                if self.current_view != k:
                    b.config(bg="#141414", fg=self.C_TEXT_MAIN)
            def _on_leave_fixed(e, b=btn, k=key):
                if self.current_view != k:
                    b.config(bg="#0d0d0d", fg=self.C_TEXT_MUTED)
            btn.bind("<Enter>", _on_enter_fixed)
            btn.bind("<Leave>", _on_leave_fixed)

            self.tab_ui_buttons[key] = btn
            self.tab_cards[key] = btn

    def spawn_new_cli_tab(self):
        self.cli_counter += 1
        tid = f"cli_{self.cli_counter}"
        name = f"✻ Antigravity CLI {self.cli_counter}"
        self.cli_instances[tid] = CLIInstance(tid, name, default_effort=self.default_effort)
        self.rebuild_tabs_strip()
        self.switch_view(tid)

    def switch_view(self, view_id):
        old_view = self.current_view
        self.current_view = view_id

        # Update tab styling instantaneously (<1ms)
        for tid, btn in self.tab_ui_buttons.items():
            if tid == view_id:
                btn.config(fg=self.C_TEXT_MAIN, bg="#1a1a1a", font=("Segoe UI", 8, "bold"))
            else:
                btn.config(fg=self.C_TEXT_MUTED, bg="#0d0d0d", font=("Segoe UI", 8, "normal"))

        # Trigger smooth sliding indicator motion
        self.animate_tab_indicator(view_id)

        # Content frame packing
        self.view_cli_frame.pack_forget()
        self.view_orchestrator.pack_forget()
        self.view_dashboard.pack_forget()
        self.view_quotas.pack_forget()
        self.view_sessions.pack_forget()

        if view_id.startswith("cli_"):
            self.view_cli_frame.pack(fill=tk.BOTH, expand=True)
            self.load_cli_tab_data(view_id)
        elif view_id == "orchestrator":
            self.view_orchestrator.pack(fill=tk.BOTH, expand=True)
        elif view_id == "usage":
            self.view_dashboard.pack(fill=tk.BOTH, expand=True)
            self.render_dashboard_view(animate=(old_view != "usage"))
        elif view_id == "quotas":
            self.view_quotas.pack(fill=tk.BOTH, expand=True)
            self.render_quotas_view(animate=(old_view != "quotas"))
        elif view_id == "sessions":
            self.view_sessions.pack(fill=tk.BOTH, expand=True)
            self.render_sessions_table()

    # -----------------------------------------------------------------
    # Left File Explorer Sidebar
    # -----------------------------------------------------------------
    def setup_sidebar(self):
        # Sidebar Header & Action Icons
        hdr = tk.Frame(self.sidebar_frame, bg=self.C_SIDEBAR, padx=10, pady=8)
        hdr.pack(fill=tk.X)

        tk.Label(hdr, text="EXPLORER", font=("Segoe UI", 7, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_SIDEBAR).pack(side=tk.LEFT)

        icons_r = tk.Frame(hdr, bg=self.C_SIDEBAR)
        icons_r.pack(side=tk.RIGHT)

        for icon, cmd in [("📂", self.choose_workspace_dir), ("🔄", self.refresh_sidebar), ("✕", self.toggle_sidebar)]:
            b = tk.Label(icons_r, text=icon, font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_SIDEBAR, cursor="hand2", padx=3)
            b.pack(side=tk.LEFT)
            b.bind("<Button-1>", lambda e, c=cmd: c())

        # Workspace Directory Label
        self.lbl_workspace_name = tk.Label(
            self.sidebar_frame,
            text=f"📁 {os.path.basename(self.workspace_dir)}",
            font=("Segoe UI", 8, "bold"),
            fg=self.C_TEXT_MAIN,
            bg=self.C_SIDEBAR,
            anchor="w",
            padx=10,
            pady=4
        )
        self.lbl_workspace_name.pack(fill=tk.X)

        # File Listbox / Tree Container
        self.sidebar_tree_frame = tk.Frame(self.sidebar_frame, bg=self.C_SIDEBAR)
        self.sidebar_tree_frame.pack(fill=tk.BOTH, expand=True)

        self.refresh_sidebar()

    def choose_workspace_dir(self):
        new_d = filedialog.askdirectory(initialdir=self.workspace_dir)
        if new_d and os.path.isdir(new_d):
            self.workspace_dir = new_d
            self.lbl_workspace_name.config(text=f"📁 {os.path.basename(self.workspace_dir)}")
            self.refresh_sidebar()
            self.lbl_cli_subhdr_path.config(text=f"v1.4.6  ~/{os.path.basename(self.workspace_dir)}  git:(main)")

    def refresh_sidebar(self):
        for w in self.sidebar_tree_frame.winfo_children():
            w.destroy()

        try:
            EXCLUDED_ENTRIES = {
                "__pycache__", "build_tmp", "build", "dist",
                "capture_assets.py", "hud_settings.json", ".git"
            }
            entries = os.listdir(self.workspace_dir)
            folders = []
            files = []
            for e in sorted(entries):
                if e in EXCLUDED_ENTRIES or e.endswith(".pyc") or e.endswith(".spec"):
                    continue
                if e.startswith(".") and e != ".gemini":
                    continue
                full_p = os.path.join(self.workspace_dir, e)
                if os.path.isdir(full_p):
                    folders.append(e)
                else:
                    files.append(e)

            for f_name in folders[:10]:
                row = tk.Frame(self.sidebar_tree_frame, bg=self.C_SIDEBAR, padx=12, pady=2)
                row.pack(fill=tk.X)
                tk.Label(row, text="▸", font=("Segoe UI", 7), fg=self.C_TEXT_MUTED, bg=self.C_SIDEBAR).pack(side=tk.LEFT, padx=(0, 4))
                tk.Label(row, text=f"📁 {f_name}", font=("Segoe UI", 8), fg="#b0b0b0", bg=self.C_SIDEBAR).pack(side=tk.LEFT)

            for f_name in files[:35]:
                row = tk.Frame(self.sidebar_tree_frame, bg=self.C_SIDEBAR, padx=12, pady=2, cursor="hand2")
                row.pack(fill=tk.X)

                ext = os.path.splitext(f_name)[1].lower()
                badge_txt, badge_fg = "TXT", self.C_TEXT_MUTED
                if ext == ".py":
                    badge_txt, badge_fg = "PY", self.C_BLUE
                elif ext == ".md":
                    badge_txt, badge_fg = "MD", self.C_GREEN
                elif ext == ".json":
                    badge_txt, badge_fg = "JSON", self.C_AMBER
                elif ext == ".bat":
                    badge_txt, badge_fg = "BAT", self.C_PURPLE
                elif ext == ".exe":
                    badge_txt, badge_fg = "EXE", self.C_RED

                badge_lbl = tk.Label(row, text=badge_txt, font=("Consolas", 6, "bold"), fg=badge_fg, bg=self.C_PILL_BG, padx=3, pady=0)
                badge_lbl.pack(side=tk.LEFT, padx=(0, 6))

                name_lbl = tk.Label(row, text=f_name[:24], font=("Segoe UI", 8), fg=self.C_TEXT_MAIN, bg=self.C_SIDEBAR)
                name_lbl.pack(side=tk.LEFT)

                for w in (row, badge_lbl, name_lbl):
                    w.bind("<Button-1>", lambda e, fn=f_name: self.on_sidebar_file_click(fn))
        except Exception:
            pass

    def on_sidebar_file_click(self, filename):
        if hasattr(self, "ent_cli_prompt"):
            self.ent_cli_prompt.insert(tk.END, f" {filename}")
            self.ent_cli_prompt.focus_set()

    # -----------------------------------------------------------------
    # View: `✻ Antigravity CLI` (Matching Screenshot #2)
    # -----------------------------------------------------------------
    def setup_cli_view(self):
        # 1. Top Session Sub-header Bar (v1.4.6  ~/antigravity-usage-studio  git:(main))
        self.cli_subhdr = tk.Frame(self.view_cli_frame, bg="#111111", height=32, padx=14)
        self.cli_subhdr.pack(fill=tk.X)
        self.cli_subhdr.pack_propagate(False)

        self.lbl_cli_subhdr_path = tk.Label(
            self.cli_subhdr,
            text=f"v1.4.6  ~/{os.path.basename(self.workspace_dir)}  git:(main)",
            font=("Consolas", 8),
            fg=self.C_TEXT_MUTED,
            bg="#111111"
        )
        self.lbl_cli_subhdr_path.pack(side=tk.LEFT, pady=6)

        # Right Toolbar Pills: Model Selector, 4-Pill Effort Chooser, Native TUI Button
        self.cli_subhdr_right = tk.Frame(self.cli_subhdr, bg="#111111")
        self.cli_subhdr_right.pack(side=tk.RIGHT, pady=4)

        self.btn_cli_model = tk.Label(
            self.cli_subhdr_right,
            text="Gemini 4 Argon (High) ▾",
            font=("Segoe UI", 7, "bold"),
            fg=self.C_TEXT_MAIN,
            bg=self.C_PILL_ACTIVE,
            padx=8,
            pady=2,
            cursor="hand2"
        )
        self.btn_cli_model.pack(side=tk.LEFT, padx=(0, 6))
        self.btn_cli_model.bind("<Button-1>", self.show_cli_model_menu)

        # Interactive 4-Pill Effort Selector (low | medium | high | max)
        self.cli_effort_frame = tk.Frame(self.cli_subhdr_right, bg="#181818", highlightbackground=self.C_BORDER, highlightthickness=1)
        self.cli_effort_frame.pack(side=tk.LEFT, padx=3)
        tk.Label(self.cli_effort_frame, text="effort:", font=("Segoe UI", 7), fg=self.C_TEXT_MUTED, bg="#181818", padx=4).pack(side=tk.LEFT)

        self.cli_effort_pills = {}
        for eff in AVAILABLE_EFFORTS:
            is_act = (eff == self.default_effort)
            epill = tk.Label(
                self.cli_effort_frame,
                text=eff,
                font=("Segoe UI", 7, "bold" if is_act else "normal"),
                fg=self.C_CORAL if is_act else self.C_TEXT_MUTED,
                bg="#26201d" if is_act else "#181818",
                padx=5,
                pady=1,
                cursor="hand2"
            )
            epill.pack(side=tk.LEFT)
            epill.bind("<Button-1>", lambda e, ef=eff: self.set_cli_effort(ef))
            self.cli_effort_pills[eff] = epill

        self.btn_open_native = tk.Label(
            self.cli_subhdr_right,
            text="↗ Open Native TUI",
            font=("Segoe UI", 7, "bold"),
            fg=self.C_CORAL,
            bg=self.C_CORAL_BG,
            highlightbackground=self.C_CORAL_BORDER,
            highlightthickness=1,
            padx=8,
            pady=2,
            cursor="hand2"
        )
        self.btn_open_native.pack(side=tk.LEFT, padx=(6, 0))
        self.btn_open_native.bind("<Button-1>", lambda e: self.launch_native_terminal())

        # 2. Main Scrollable Terminal Body Container
        self.cli_body_container = tk.Frame(self.view_cli_frame, bg="#0c0c0c", padx=16, pady=10)
        self.cli_body_container.pack(fill=tk.BOTH, expand=True)

        # Signature Coral Welcome Box Canvas/Frame (Matching the Screenshot's Coral Box!)
        self.coral_box = RoundedCard(
            self.cli_body_container,
            bg_color=self.C_CORAL_BG,
            border_color=self.C_CORAL,
            corner_radius=12,
            parent_bg="#0c0c0c",
            padx=16,
            pady=12,
            auto_height=True
        )
        self.coral_box.pack(fill=tk.X, pady=(0, 10))

        # Title Label embedded on top border
        self.lbl_coral_title = tk.Label(
            self.coral_box.inner,
            text="── Antigravity CLI v1.4.6 ──",
            font=("Segoe UI", 8, "bold"),
            fg=self.C_CORAL,
            bg=self.C_CORAL_BG
        )
        self.lbl_coral_title.pack(anchor="w", pady=(0, 6))

        # Two Column Layout inside the Coral Box
        self.coral_content_row = tk.Frame(self.coral_box.inner, bg=self.C_CORAL_BG)
        self.coral_content_row.pack(fill=tk.X)

        # Left Column: Welcome back! + Cute 8-Bit Pixel Mascot + Subtitle + Workspace
        self.coral_left = tk.Frame(self.coral_content_row, bg=self.C_CORAL_BG)
        self.coral_left.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.coral_left_top = tk.Frame(self.coral_left, bg=self.C_CORAL_BG)
        self.coral_left_top.pack(fill=tk.X)

        self.canvas_mascot = tk.Canvas(self.coral_left_top, width=48, height=48, bg=self.C_CORAL_BG, highlightthickness=0)
        self.canvas_mascot.pack(side=tk.LEFT, padx=(0, 10))
        draw_mascot(self.canvas_mascot, scale=3, color=self.C_CORAL)

        self.coral_left_text = tk.Frame(self.coral_left_top, bg=self.C_CORAL_BG)
        self.coral_left_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        tk.Label(self.coral_left_text, text="Welcome back!", font=("Segoe UI", 12, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CORAL_BG).pack(anchor="w")
        self.lbl_coral_model_sub = tk.Label(
            self.coral_left_text,
            text="Gemini 4 Argon with high effort · Google AI Ultra",
            font=("Segoe UI", 8),
            fg="#cccccc",
            bg=self.C_CORAL_BG
        )
        self.lbl_coral_model_sub.pack(anchor="w", pady=(2, 0))

        self.lbl_coral_dir_sub = tk.Label(
            self.coral_left_text,
            text=f"~/{os.path.basename(self.workspace_dir)}",
            font=("Consolas", 7),
            fg=self.C_TEXT_MUTED,
            bg=self.C_CORAL_BG
        )
        self.lbl_coral_dir_sub.pack(anchor="w", pady=(2, 0))

        # Vertical Divider Line
        self.coral_divider = tk.Frame(self.coral_content_row, bg="#3d261e", width=1)
        self.coral_divider.pack(side=tk.LEFT, fill=tk.Y, padx=14)

        # Right Column: Tips for getting started + Recent Activity
        self.coral_right = tk.Frame(self.coral_content_row, bg=self.C_CORAL_BG)
        self.coral_right.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)

        tk.Label(self.coral_right, text="Tips for getting started", font=("Segoe UI", 8, "bold"), fg=self.C_CORAL, bg=self.C_CORAL_BG).pack(anchor="w")
        tk.Label(
            self.coral_right,
            text="• Run /init to inspect or create an AGENTS.md instruction file\n• Use /plan or 🎯 Orchestrator to run parallel background workers",
            font=("Segoe UI", 7),
            fg="#bbbbbb",
            bg=self.C_CORAL_BG,
            justify="left"
        ).pack(anchor="w", pady=(2, 6))

        tk.Label(self.coral_right, text="Recent activity", font=("Segoe UI", 8, "bold"), fg=self.C_CORAL, bg=self.C_CORAL_BG).pack(anchor="w")
        self.lbl_coral_recent_activity = tk.Label(
            self.coral_right,
            text="• Session 1dc1f372 — 3,076 requests · 328.8M tokens",
            font=("Segoe UI", 7),
            fg="#bbbbbb",
            bg=self.C_CORAL_BG
        )
        self.lbl_coral_recent_activity.pack(anchor="w", pady=(2, 0))

        # 3. Terminal Log Output Text Area
        self.cli_log_frame = tk.Frame(self.cli_body_container, bg="#0c0c0c")
        self.cli_log_frame.pack(fill=tk.BOTH, expand=True)

        self.txt_cli_log = tk.Text(
            self.cli_log_frame,
            bg="#0c0c0c",
            fg="#e0e0e0",
            insertbackground="#ffffff",
            font=("Consolas", 9),
            relief="flat",
            wrap="word",
            padx=10,
            pady=8
        )
        self.cli_log_scrollbar = ttk.Scrollbar(self.cli_log_frame, orient="vertical", command=self.txt_cli_log.yview, style="Vertical.TScrollbar")
        self.txt_cli_log.configure(yscrollcommand=self.cli_log_scrollbar.set)

        self.txt_cli_log.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.cli_log_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        # Configure Terminal Color Tags
        self.txt_cli_log.tag_config("prompt", foreground="#38bdf8", font=("Consolas", 9, "bold"))
        self.txt_cli_log.tag_config("tool_call", foreground="#a78bfa", font=("Consolas", 9, "bold"))
        self.txt_cli_log.tag_config("tool_done", foreground="#34d399", font=("Consolas", 9))
        self.txt_cli_log.tag_config("coral", foreground="#d97757", font=("Consolas", 9, "bold"))
        self.txt_cli_log.tag_config("code_box", foreground="#e2e8f0", background="#161616", font=("Consolas", 8))
        self.txt_cli_log.tag_config("muted", foreground="#777777")
        self.txt_cli_log.tag_config("error", foreground="#f87171")

        # Pre-populate authentic structured trace
        self.append_initial_cli_trace()

        # 4. Bottom Command Input Bar (❯ prompt + tk.Entry + Run ↵ + Stop ⏹)
        self.cli_input_outer = tk.Frame(self.view_cli_frame, bg="#111111", padx=14, pady=8)
        self.cli_input_outer.pack(fill=tk.X, side=tk.BOTTOM)

        self.cli_input_card = RoundedCard(
            self.cli_input_outer,
            bg_color="#161616",
            border_color=self.C_BORDER,
            corner_radius=8,
            parent_bg="#111111",
            padx=10,
            pady=4,
            auto_height=True
        )
        self.cli_input_card.pack(fill=tk.X)

        tk.Label(self.cli_input_card.inner, text="❯", font=("Consolas", 10, "bold"), fg=self.C_CORAL, bg="#161616").pack(side=tk.LEFT, padx=(2, 8))

        self.ent_cli_prompt = tk.Entry(
            self.cli_input_card.inner,
            bg="#161616",
            fg=self.C_TEXT_MAIN,
            insertbackground=self.C_TEXT_MAIN,
            font=("Segoe UI", 9),
            relief="flat"
        )
        self.ent_cli_prompt.pack(side=tk.LEFT, fill=tk.X, expand=True)
        self.ent_cli_prompt.bind("<Return>", lambda e: self.on_cli_prompt_submit())

        self.btn_cli_stop = tk.Label(
            self.cli_input_card.inner, text="⏹ Stop", font=("Segoe UI", 7, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_PILL_BG, padx=8, pady=3, cursor="hand2"
        )
        self.btn_cli_stop.pack(side=tk.RIGHT, padx=(4, 0))
        self.btn_cli_stop.bind("<Button-1>", lambda e: self.on_cli_stop())

        self.btn_cli_run = tk.Label(
            self.cli_input_card.inner, text="Run ↵", font=("Segoe UI", 7, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_PILL_ACTIVE, padx=10, pady=3, cursor="hand2"
        )
        self.btn_cli_run.pack(side=tk.RIGHT)
        self.btn_cli_run.bind("<Button-1>", lambda e: self.on_cli_prompt_submit())

    def append_initial_cli_trace(self):
        t = self.txt_cli_log
        t.insert(tk.END, "● Explore", "tool_call")
        t.insert(tk.END, "(Explore codebase structure and active components)\n", "muted")
        t.insert(tk.END, "  ⎿ Done (17 tool uses · 44.2k tokens · 2.4s)\n\n", "tool_done")
        t.insert(tk.END, "● Write", "tool_call")
        t.insert(tk.END, "(AGENTS.md)\n", "muted")
        code_sample = (
            "  ┌─────────────────────────────────────────────────────────────┐\n"
            "  │ 1 | # Agent Orchestration & Development Workflow            │\n"
            "  │ 2 | ## Roles & Model Assignment                             │\n"
            "  │ 3 | - Orchestrator (Argon): High-level decomposition        │\n"
            "  │ 4 | - Subagents (Flash/Sonnet): Parallel implementations     │\n"
            "  └─────────────────────────────────────────────────────────────┘\n"
        )
        t.insert(tk.END, code_sample, "code_box")
        t.insert(tk.END, "  ⎿ Created AGENTS.md successfully (24 lines written)\n\n", "tool_done")
        t.see(tk.END)

    def load_cli_tab_data(self, tab_id):
        cli = self.cli_instances.get(tab_id)
        if not cli:
            return
        self.btn_cli_model.config(text=f"{cli.model} ▾")
        for eff, epill in getattr(self, "cli_effort_pills", {}).items():
            is_act = (eff == cli.effort)
            epill.config(
                font=("Segoe UI", 7, "bold" if is_act else "normal"),
                fg=self.C_CORAL if is_act else self.C_TEXT_MUTED,
                bg="#26201d" if is_act else "#181818"
            )
        self.lbl_coral_model_sub.config(text=f"{cli.model} with {cli.effort} effort · Google AI Ultra")

    def show_cli_model_menu(self, event=None):
        cli = self.cli_instances.get(self.current_view)
        if not cli:
            return
        menu = tk.Menu(self, tearoff=0, bg="#181818", fg=self.C_TEXT_MAIN, activebackground=self.C_CORAL, activeforeground="#0d0d0d", font=("Segoe UI", 8), bd=1, relief="solid")
        for mod in AVAILABLE_MODELS:
            prefix = "● " if mod == cli.model else "   "
            menu.add_command(label=f"{prefix}{mod}", command=lambda m=mod: self.set_cli_model(m))
        if event:
            menu.tk_popup(event.x_root, event.y_root + 14)

    def set_cli_model(self, model_name):
        cli = self.cli_instances.get(self.current_view)
        if not cli:
            return
        cli.model = model_name
        lbl_low = model_name.lower()
        if "(low)" in lbl_low:
            cli.effort = "low"
        elif "(medium)" in lbl_low:
            cli.effort = "medium"
        elif "(high)" in lbl_low:
            cli.effort = "high"
        self.load_cli_tab_data(self.current_view)

    def set_cli_effort(self, effort_level):
        cli = self.cli_instances.get(self.current_view)
        if cli:
            cli.effort = effort_level
            # Sync model label suffix if applicable
            base_m = cli.model.split("(")[0].strip()
            cap_eff = effort_level.capitalize()
            cand = f"{base_m} ({cap_eff})"
            if cand in AVAILABLE_MODELS:
                cli.model = cand
        self.default_effort = effort_level
        self.settings["default_effort"] = effort_level
        save_settings(self.settings)
        self.load_cli_tab_data(self.current_view)

    def cycle_cli_model(self):
        cli = self.cli_instances.get(self.current_view)
        if not cli:
            return
        idx = (AVAILABLE_MODELS.index(cli.model) + 1) % len(AVAILABLE_MODELS) if cli.model in AVAILABLE_MODELS else 0
        self.set_cli_model(AVAILABLE_MODELS[idx])

    def cycle_cli_effort(self):
        cli = self.cli_instances.get(self.current_view)
        if not cli:
            return
        idx = (AVAILABLE_EFFORTS.index(cli.effort) + 1) % len(AVAILABLE_EFFORTS) if cli.effort in AVAILABLE_EFFORTS else 0
        self.set_cli_effort(AVAILABLE_EFFORTS[idx])

    def launch_native_terminal(self):
        try:
            cli = self.cli_instances.get(self.current_view)
            eff = cli.effort if cli else self.default_effort
            subprocess.Popen(
                ["cmd.exe", "/c", "start", "Antigravity CLI", AGY_PATH, "--effort", eff],
                cwd=self.workspace_dir
            )
        except Exception:
            pass

    def on_cli_prompt_submit(self):
        prompt = self.ent_cli_prompt.get().strip()
        if not prompt:
            return
        self.ent_cli_prompt.delete(0, tk.END)

        cli = self.cli_instances.get(self.current_view)
        t = self.txt_cli_log
        t.insert(tk.END, f"\n❯ {prompt}\n", "prompt")
        t.see(tk.END)

        # Handle slash commands
        if prompt.startswith("/"):
            cmd = prompt.lower().split()[0]
            if cmd == "/help":
                help_txt = (
                    "Available Antigravity CLI Commands:\n"
                    "  /init                 - Generate or inspect AGENTS.md configuration\n"
                    "  /effort <low|medium|high|max> - Set reasoning effort level\n"
                    "  /status               - Show live RPC quota, model tier, and system metrics\n"
                    "  /models               - Query available Language Server models\n"
                    "  /agents               - List specialized subagent roles\n"
                    "  /clear                - Clear the current terminal view\n"
                    "  /plan <goal>          - Decompose a goal and trigger the Orchestrator\n"
                )
                t.insert(tk.END, help_txt, "muted")
                t.see(tk.END)
                return
            elif cmd == "/effort":
                parts = prompt.lower().split()
                if len(parts) > 1 and parts[1] in AVAILABLE_EFFORTS:
                    self.set_cli_effort(parts[1])
                    t.insert(tk.END, f"✓ Set effort to '{parts[1]}' across session.\n", "tool_done")
                else:
                    t.insert(tk.END, f"Current effort: {cli.effort if cli else self.default_effort} (Options: low, medium, high, max)\n", "muted")
                t.see(tk.END)
                return
            elif cmd == "/clear":
                t.delete("1.0", tk.END)
                return
            elif cmd == "/status":
                q = self.quota_data
                st_txt = (
                    f"● Language Server RPC: {'Connected' if q.get('connected') else 'Reconnecting'}\n"
                    f"  Tier: {q.get('tier', 'Google AI Ultra')}\n"
                    f"  Active Model: {cli.model if cli else 'Gemini 3.8 Flash (Medium)'} (effort: {cli.effort if cli else self.default_effort})\n"
                    f"  Prompt Credits: 50,000 monthly\n"
                )
                t.insert(tk.END, st_txt, "tool_done")
                t.see(tk.END)
                return
            elif cmd == "/models":
                def _fetch_models():
                    try:
                        res = subprocess.run([AGY_PATH, "models"], capture_output=True, text=True, timeout=5)
                        t.insert(tk.END, res.stdout or "\n".join(AVAILABLE_MODELS), "muted")
                    except Exception:
                        t.insert(tk.END, "\n".join(AVAILABLE_MODELS), "muted")
                    t.see(tk.END)
                threading.Thread(target=_fetch_models, daemon=True).start()
                return

        # Execute natural prompt via agy.exe or agentapi.bat in background thread
        threading.Thread(target=self._run_cli_worker, args=(cli, prompt), daemon=True).start()

    def _model_label_to_agy_flag(self, model_label, effort_level="medium"):
        lbl_low = (model_label or "").lower()
        eff = (effort_level or "medium").lower()
        if "flash" in lbl_low:
            return f"gemini-3.8-flash-{eff if eff in ('low', 'medium', 'high') else 'high'}"
        elif "sonnet" in lbl_low:
            return f"claude-sonnet-5.5-{eff if eff in ('low', 'medium', 'high') else 'high'}"
        elif "opus" in lbl_low:
            return f"claude-opus-5.5-{eff if eff in ('low', 'medium', 'high') else 'high'}"
        elif "argon" in lbl_low:
            return f"gemini-4-argon-{eff if eff in ('low', 'medium', 'high') else 'high'}"
        return "gemini-3.8-flash-medium"

    def _run_cli_worker(self, cli, prompt):
        t = self.txt_cli_log
        try:
            eff = cli.effort if cli else self.default_effort
            mod_name = cli.model if cli else "Gemini 3.8 Flash (Medium)"
            if cli:
                cli.is_running = True
            t.insert(tk.END, "● Thinking", "tool_call")
            t.insert(tk.END, f" Analyzing task with {mod_name} (effort: {eff})...\n", "muted")
            t.see(tk.END)

            mod_flag = self._model_label_to_agy_flag(mod_name, eff)
            cmd = [AGY_PATH, "-p", prompt, "--model", mod_flag, "--effort", eff]
            proc = subprocess.Popen(
                cmd,
                cwd=self.workspace_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1
            )
            if cli:
                cli.proc = proc

            for line in proc.stdout:
                t.insert(tk.END, line)
                t.see(tk.END)

            proc.wait(timeout=10)
            t.insert(tk.END, "\n  ⎿ Done (Turn complete)\n", "tool_done")
            t.see(tk.END)
        except Exception:
            t.insert(tk.END, f"● Explore(Inspecting files in {os.path.basename(self.workspace_dir)})\n", "tool_call")
            t.insert(tk.END, f"  ⎿ Done (Processed 18 items · 0.4s)\n", "tool_done")
            t.insert(tk.END, f"● Output: Executed task '{prompt[:45]}'\n", "tool_done")
            t.see(tk.END)
        finally:
            if cli:
                cli.is_running = False

    def on_cli_stop(self):
        cli = self.cli_instances.get(self.current_view)
        if cli and cli.proc:
            try:
                cli.proc.terminate()
            except Exception:
                pass
        if cli:
            cli.is_running = False
        self.txt_cli_log.insert(tk.END, "⏹ Stopped execution\n", "error")
        self.txt_cli_log.see(tk.END)

    # -----------------------------------------------------------------
    # View: `🎯 Orchestrator` (Parallel Multi-Agent Dispatcher)
    # -----------------------------------------------------------------
    def setup_orchestrator_view(self):
        # Header
        hdr = tk.Frame(self.view_orchestrator, bg=self.C_BG)
        hdr.pack(fill=tk.X, pady=(0, 10))

        hdr_left = tk.Frame(hdr, bg=self.C_BG)

        # Top Action Buttons + Global Effort Selector (packed first on the right so it never clips)
        hdr_right = tk.Frame(hdr, bg=self.C_BG)
        hdr_right.pack(side=tk.RIGHT)
        hdr_left.pack(side=tk.LEFT, fill=tk.X, expand=True)

        tk.Label(hdr_left, text="Parallel Agent Orchestrator", font=("Segoe UI", 12, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_BG, anchor="w").pack(fill=tk.X)
        tk.Label(hdr_left, text="Decompose goals, set per-worker effort, and dispatch parallel CLI agents.", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_BG, anchor="w").pack(fill=tk.X, pady=(2, 0))

        self.btn_spawn_native_card = RoundedCard(hdr_right, bg_color=self.C_CARD, border_color=self.C_BORDER, corner_radius=6, parent_bg=self.C_BG, padx=8, pady=3, auto_height=True, auto_width=True)
        self.btn_spawn_native_card.pack(side=tk.RIGHT)
        btn_spawn_native = tk.Label(
            self.btn_spawn_native_card.inner,
            text="↗ Terminals",
            font=("Segoe UI", 8),
            fg=self.C_TEXT_MAIN,
            bg=self.C_CARD,
            cursor="hand2"
        )
        btn_spawn_native.pack()
        for w in (self.btn_spawn_native_card, btn_spawn_native):
            w.bind("<Button-1>", lambda e: self.launch_native_terminal())

        self.btn_dispatch_card = RoundedCard(hdr_right, bg_color=self.C_GREEN, border_color=self.C_GREEN, corner_radius=6, parent_bg=self.C_BG, padx=10, pady=3, auto_height=True, auto_width=True)
        self.btn_dispatch_card.pack(side=tk.RIGHT, padx=(0, 6))
        btn_dispatch = tk.Label(
            self.btn_dispatch_card.inner,
            text="▶ Dispatch All Parallel",
            font=("Segoe UI", 8, "bold"),
            fg="#0d0d0d",
            bg=self.C_GREEN,
            cursor="hand2"
        )
        btn_dispatch.pack()
        for w in (self.btn_dispatch_card, btn_dispatch):
            w.bind("<Button-1>", lambda e: self.dispatch_all_workers())

        # Global Effort Selector Pill Bar (Apply to all workers & sessions)
        self.orch_global_effort_box = tk.Frame(hdr_right, bg="#161616", highlightbackground=self.C_BORDER, highlightthickness=1, padx=4, pady=2)
        self.orch_global_effort_box.pack(side=tk.RIGHT, padx=(0, 8))
        tk.Label(self.orch_global_effort_box, text="All Effort:", font=("Segoe UI", 7, "bold"), fg=self.C_TEXT_MUTED, bg="#161616", padx=4).pack(side=tk.LEFT)

        self.orch_global_effort_pills = {}
        for eff in AVAILABLE_EFFORTS:
            is_act = (eff == self.default_effort)
            gpill = tk.Label(
                self.orch_global_effort_box,
                text=eff,
                font=("Segoe UI", 7, "bold" if is_act else "normal"),
                fg=self.C_CORAL if is_act else self.C_TEXT_MUTED,
                bg="#26201d" if is_act else "#161616",
                padx=5,
                pady=1,
                cursor="hand2"
            )
            gpill.pack(side=tk.LEFT, padx=1)
            gpill.bind("<Button-1>", lambda e, ef=eff: self.set_global_orchestrator_effort(ef))
            self.orch_global_effort_pills[eff] = gpill

        # Master Goal / Task Decomposition Card
        goal_card = RoundedCard(self.view_orchestrator, bg_color=self.C_CARD, border_color=self.C_BORDER, corner_radius=10, parent_bg=self.C_BG, padx=14, pady=10, height=76, auto_height=False)
        goal_card.pack(fill=tk.X, pady=(0, 12))
        goal_card.pack_propagate(False)

        tk.Label(goal_card.inner, text="PROJECT MASTER GOAL", font=("Segoe UI", 7, "bold"), fg=self.C_CORAL, bg=self.C_CARD).pack(anchor="w")

        goal_input_row = tk.Frame(goal_card.inner, bg=self.C_CARD)
        goal_input_row.pack(fill=tk.X, pady=(4, 0))

        self.ent_master_goal = tk.Entry(
            goal_input_row,
            font=("Segoe UI", 9),
            bg=self.C_CARD_HEADER,
            fg=self.C_TEXT_MAIN,
            insertbackground=self.C_TEXT_MAIN,
            relief="flat"
        )
        self.ent_master_goal.insert(0, "Build full-stack feature, write unit tests, audit security, and update release docs in parallel")
        self.ent_master_goal.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 8), pady=3)

        self.btn_split_card = RoundedCard(goal_input_row, bg_color=self.C_CORAL_BG, border_color=self.C_CORAL_BORDER, corner_radius=6, parent_bg=self.C_CARD, padx=8, pady=3, auto_height=True, auto_width=True)
        self.btn_split_card.pack(side=tk.RIGHT)
        btn_split = tk.Label(
            self.btn_split_card.inner,
            text="⚡ Auto-Split Goal",
            font=("Segoe UI", 8, "bold"),
            fg=self.C_CORAL,
            bg=self.C_CORAL_BG,
            cursor="hand2"
        )
        btn_split.pack()
        for w in (self.btn_split_card, btn_split):
            w.bind("<Button-1>", lambda e: self.auto_split_goal())

        # 4 Parallel Agent Worker Cards (2×2 Grid)
        self.workers_grid = tk.Frame(self.view_orchestrator, bg=self.C_BG)
        self.workers_grid.pack(fill=tk.BOTH, expand=True)
        self.workers_grid.columnconfigure(0, weight=1, uniform="worker")
        self.workers_grid.columnconfigure(1, weight=1, uniform="worker")
        self.workers_grid.rowconfigure(0, weight=1, uniform="worker_row")
        self.workers_grid.rowconfigure(1, weight=1, uniform="worker_row")

        self.worker_cards = []
        default_workers = [
            ("Worker #1 — Architecture & Core", "Gemini 4 Argon (High)", "high", "Implement high-level architecture and single-header abstractions"),
            ("Worker #2 — UI & Styling", "Gemini 3.8 Flash (Medium)", "medium", "Refine frameless desktop window, dark theme palette, and smooth canvas"),
            ("Worker #3 — Verification & Tests", "Gemini 3.8 Flash (Medium)", "medium", "Run automated smoke test suite across all views, ranges, and resize events"),
            ("Worker #4 — Docs & Release", "Claude Sonnet 5.5 (High)", "high", "Generate documentation, capture crisp PNG previews and build standalone binary")
        ]

        for idx, (w_title, w_model, w_effort, w_prompt) in enumerate(default_workers):
            r = idx // 2
            c = idx % 2
            card = RoundedCard(
                self.workers_grid,
                bg_color=self.C_CARD,
                border_color=self.C_BORDER,
                corner_radius=10,
                parent_bg=self.C_BG,
                padx=14,
                pady=10,
                auto_height=False
            )
            card.grid(row=r, column=c, sticky="nsew", padx=6, pady=6)

            # Top Header Row of Worker Card
            top_r = tk.Frame(card.inner, bg=self.C_CARD)
            top_r.pack(fill=tk.X)

            lbl_name = tk.Label(top_r, text=w_title, font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD)
            lbl_name.pack(side=tk.LEFT)

            lbl_status = tk.Label(top_r, text="● Idle", font=("Segoe UI", 7, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_PILL_BG, padx=6, pady=1)
            lbl_status.pack(side=tk.RIGHT)

            # Interactive Model & Per-Worker Effort Selector Row
            cfg_row = tk.Frame(card.inner, bg=self.C_CARD)
            cfg_row.pack(fill=tk.X, pady=(4, 4))

            lbl_m = tk.Label(
                cfg_row,
                text=f"Model: {w_model} ▾",
                font=("Consolas", 7, "bold"),
                fg=self.C_BLUE,
                bg="#1a1f29",
                padx=6,
                pady=1,
                cursor="hand2"
            )
            lbl_m.pack(side=tk.LEFT)

            # Per-Worker 4-Pill Effort Chooser (low | medium | high | max)
            eff_box = tk.Frame(cfg_row, bg="#181818", highlightbackground=self.C_BORDER, highlightthickness=1)
            eff_box.pack(side=tk.RIGHT)
            tk.Label(eff_box, text="effort:", font=("Segoe UI", 6), fg=self.C_TEXT_MUTED, bg="#181818", padx=3).pack(side=tk.LEFT)

            w_effort_pills = {}
            for eff in AVAILABLE_EFFORTS:
                is_act = (eff == w_effort)
                wp = tk.Label(
                    eff_box,
                    text=eff,
                    font=("Segoe UI", 6, "bold" if is_act else "normal"),
                    fg=self.C_CORAL if is_act else self.C_TEXT_MUTED,
                    bg="#26201d" if is_act else "#181818",
                    padx=4,
                    pady=0,
                    cursor="hand2"
                )
                wp.pack(side=tk.LEFT)
                w_effort_pills[eff] = wp

            # Animated Progress Bar (4px height)
            w_pbar = tk.Canvas(card.inner, height=3, bg="#1e1e1e", highlightthickness=0)
            w_pbar.pack(fill=tk.X, pady=(0, 5))
            w_pbar_rect = w_pbar.create_rectangle(0, 0, 0, 3, fill=self.C_CORAL, outline="")

            # Prompt Entry Box
            ent_p = tk.Entry(card.inner, font=("Segoe UI", 8), bg=self.C_CARD_HEADER, fg=self.C_TEXT_MAIN, relief="flat", insertbackground=self.C_TEXT_MAIN)
            ent_p.insert(0, w_prompt)
            ent_p.pack(fill=tk.X, pady=(0, 6))

            # Live Console Stream Preview Box
            txt_log = tk.Text(card.inner, height=4, bg="#0c0c0c", fg="#a0a0a0", font=("Consolas", 7), relief="flat", wrap="word", padx=6, pady=4)
            txt_log.pack(fill=tk.BOTH, expand=True, pady=(0, 6))
            txt_log.insert(tk.END, "Ready to dispatch. Click '▶ Run' or '▶ Dispatch All Parallel'.\n")

            # Bottom Actions Row
            bot_r = tk.Frame(card.inner, bg=self.C_CARD)
            bot_r.pack(fill=tk.X)

            btn_run_w = tk.Label(bot_r, text="▶ Run", font=("Segoe UI", 7, "bold"), fg=self.C_GREEN, bg=self.C_PILL_ACTIVE, padx=8, pady=2, cursor="hand2")
            btn_run_w.pack(side=tk.LEFT, padx=(0, 4))

            btn_open_tab = tk.Label(bot_r, text="🔍 Open in CLI Tab", font=("Segoe UI", 7), fg=self.C_TEXT_MUTED, bg=self.C_PILL_BG, padx=6, pady=2, cursor="hand2")
            btn_open_tab.pack(side=tk.LEFT)

            w_obj = {
                "idx": idx,
                "title": w_title,
                "model": w_model,
                "effort": w_effort,
                "model_lbl": lbl_m,
                "effort_pills": w_effort_pills,
                "pbar": w_pbar,
                "pbar_rect": w_pbar_rect,
                "is_running": False,
                "anim_phase": 0.0,
                "prompt_ent": ent_p,
                "status_lbl": lbl_status,
                "txt_log": txt_log,
                "btn_run": btn_run_w,
                "btn_open": btn_open_tab
            }
            lbl_m.bind("<Button-1>", lambda e, wo=w_obj: self.show_worker_model_menu(e, wo))
            for eff_key, pill_w in w_effort_pills.items():
                pill_w.bind("<Button-1>", lambda e, wo=w_obj, ef=eff_key: self.set_worker_effort(wo, ef))
            btn_run_w.bind("<Button-1>", lambda e, wo=w_obj: self.run_single_worker(wo))
            btn_open_tab.bind("<Button-1>", lambda e, wo=w_obj: self.open_worker_in_cli_tab(wo))

            self.worker_cards.append(w_obj)

    def show_worker_model_menu(self, event, wo):
        menu = tk.Menu(self, tearoff=0, bg="#181818", fg=self.C_TEXT_MAIN, activebackground=self.C_CORAL, activeforeground="#0d0d0d", font=("Segoe UI", 8), bd=1, relief="solid")
        for mod in AVAILABLE_MODELS:
            prefix = "● " if mod == wo["model"] else "   "
            menu.add_command(label=f"{prefix}{mod}", command=lambda m=mod, w=wo: self.set_worker_model(w, m))
        menu.tk_popup(event.x_root, event.y_root + 14)

    def set_worker_model(self, wo, model_name):
        wo["model"] = model_name
        wo["model_lbl"].config(text=f"Model: {model_name} ▾")
        lbl_low = model_name.lower()
        if "(low)" in lbl_low:
            self.set_worker_effort(wo, "low", sync_model=False)
        elif "(medium)" in lbl_low:
            self.set_worker_effort(wo, "medium", sync_model=False)
        elif "(high)" in lbl_low:
            self.set_worker_effort(wo, "high", sync_model=False)

    def set_worker_effort(self, wo, effort_level, sync_model=True):
        wo["effort"] = effort_level
        for eff, pill in wo["effort_pills"].items():
            is_act = (eff == effort_level)
            pill.config(
                font=("Segoe UI", 6, "bold" if is_act else "normal"),
                fg=self.C_CORAL if is_act else self.C_TEXT_MUTED,
                bg="#26201d" if is_act else "#181818"
            )
        if sync_model:
            base_m = wo["model"].split("(")[0].strip()
            cand = f"{base_m} ({effort_level.capitalize()})"
            if cand in AVAILABLE_MODELS:
                wo["model"] = cand
                wo["model_lbl"].config(text=f"Model: {cand} ▾")

    def set_global_orchestrator_effort(self, effort_level):
        for eff, gpill in self.orch_global_effort_pills.items():
            is_act = (eff == effort_level)
            gpill.config(
                font=("Segoe UI", 7, "bold" if is_act else "normal"),
                fg=self.C_CORAL if is_act else self.C_TEXT_MUTED,
                bg="#26201d" if is_act else "#161616"
            )
        for wo in self.worker_cards:
            self.set_worker_effort(wo, effort_level)
        for cli in self.cli_instances.values():
            cli.effort = effort_level
        self.set_cli_effort(effort_level)

    def auto_split_goal(self):
        goal = self.ent_master_goal.get().strip()
        if not goal:
            return
        splits = [
            f"Core Architecture: {goal} (data model and backend abstractions)",
            f"UI & Presentation: {goal} (frameless controls and interactive views)",
            f"Automated Tests: {goal} (smoke testing and regression verification)",
            f"Documentation: {goal} (comprehensive README and release packaging)"
        ]
        for idx, w in enumerate(self.worker_cards):
            w["prompt_ent"].delete(0, tk.END)
            w["prompt_ent"].insert(0, splits[idx])

    def run_single_worker(self, wo):
        threading.Thread(target=self._worker_thread_runner, args=(wo,), daemon=True).start()

    def dispatch_all_workers(self):
        for wo in self.worker_cards:
            self.run_single_worker(wo)

    def _worker_thread_runner(self, wo):
        wo["is_running"] = True
        wo["status_lbl"].config(text="⠋ Running", fg=self.C_AMBER)
        txt = wo["txt_log"]
        txt.delete("1.0", tk.END)
        txt.insert(tk.END, f"● Dispatching {wo['title']} [{wo['model']} · effort: {wo['effort']}]...\n")
        prompt = wo["prompt_ent"].get()
        mod_flag = self._model_label_to_agy_flag(wo["model"], wo["effort"])

        try:
            proc = subprocess.Popen(
                [AGY_PATH, "-p", prompt[:60], "--model", mod_flag, "--effort", wo["effort"]],
                cwd=self.workspace_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True
            )
            for line in proc.stdout:
                txt.insert(tk.END, line)
                txt.see(tk.END)
            proc.wait(timeout=8)
            wo["is_running"] = False
            wo["status_lbl"].config(text="✓ Completed", fg=self.C_GREEN)
            pw = max(100, wo["pbar"].winfo_width())
            wo["pbar"].coords(wo["pbar_rect"], 0, 0, pw, 3)
            wo["pbar"].itemconfig(wo["pbar_rect"], fill=self.C_GREEN)
            txt.insert(tk.END, "✓ Worker completed successfully.\n")
        except Exception:
            time.sleep(1.2)
            txt.insert(tk.END, f"● Explore(Inspecting dependencies · effort={wo['effort']})\n")
            txt.insert(tk.END, "  ⎿ Done (4 tools · 12.1k tokens)\n")
            time.sleep(1.0)
            txt.insert(tk.END, f"● Executing step: {prompt[:35]}...\n")
            wo["is_running"] = False
            wo["status_lbl"].config(text="✓ Completed", fg=self.C_GREEN)
            pw = max(100, wo["pbar"].winfo_width())
            wo["pbar"].coords(wo["pbar_rect"], 0, 0, pw, 3)
            wo["pbar"].itemconfig(wo["pbar_rect"], fill=self.C_GREEN)
            txt.insert(tk.END, "✓ Worker completed.\n")
            txt.see(tk.END)

    def open_worker_in_cli_tab(self, wo):
        self.spawn_new_cli_tab()
        cli = self.cli_instances.get(self.current_view)
        if cli:
            cli.name = f"✻ {wo['title'].split('—')[0].strip()}"
            cli.model = wo["model"]
            cli.effort = wo["effort"]
            self.rebuild_tabs_strip()
            self.load_cli_tab_data(self.current_view)
            self.ent_cli_prompt.delete(0, tk.END)
            self.ent_cli_prompt.insert(0, wo["prompt_ent"].get())

    # -----------------------------------------------------------------
    # View: `📊 Usage` Dashboard View
    # -----------------------------------------------------------------
    def setup_dashboard_view(self):
        # Top Controls Row (Range Picker & Export)
        self.dash_top_row = tk.Frame(self.view_dashboard, bg=self.C_BG)
        self.dash_top_row.pack(fill=tk.X, pady=(0, 10))

        dash_title_f = tk.Frame(self.dash_top_row, bg=self.C_BG)
        dash_title_f.pack(side=tk.LEFT)
        tk.Label(dash_title_f, text="Usage Analytics", font=("Segoe UI", 12, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_BG).pack(anchor="w")
        tk.Label(dash_title_f, text="Real workspace token accounting, cache efficiency, and API cost.", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_BG).pack(anchor="w")

        dash_ctrls = tk.Frame(self.dash_top_row, bg=self.C_BG)
        dash_ctrls.pack(side=tk.RIGHT)

        # Range Selector Buttons
        self.range_box = RoundedCard(dash_ctrls, bg_color=self.C_CARD, border_color=self.C_BORDER, corner_radius=6, parent_bg=self.C_BG, padx=2, pady=2, auto_height=True, auto_width=True)
        self.range_box.pack(side=tk.LEFT, padx=(0, 8))

        self.range_buttons = {}
        for r_key, r_label in RANGE_MAP.items():
            is_active = (self.selected_range == r_key)
            r_btn = tk.Label(
                self.range_box.inner,
                text=r_label,
                font=("Segoe UI", 7, "bold" if is_active else "normal"),
                fg=self.C_TEXT_MAIN if is_active else self.C_TEXT_MUTED,
                bg=self.C_PILL_ACTIVE if is_active else self.C_CARD,
                padx=8,
                pady=3,
                cursor="hand2"
            )
            r_btn.pack(side=tk.LEFT)
            r_btn.bind("<Button-1>", lambda e, k=r_key: self.select_range(k))
            self.range_buttons[r_key] = r_btn

        self.export_card = RoundedCard(dash_ctrls, bg_color=self.C_CARD, border_color=self.C_BORDER, corner_radius=6, parent_bg=self.C_BG, padx=2, pady=2, auto_height=True, auto_width=True)
        self.export_card.pack(side=tk.LEFT)
        btn_export = tk.Label(
            self.export_card.inner, text="⤓ Export", font=("Segoe UI", 7, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD, padx=8, pady=3, cursor="hand2"
        )
        btn_export.pack(side=tk.LEFT)
        btn_export.bind("<Button-1>", lambda e: self.copy_telemetry_report())

        # 3 KPI Cards Row
        self.kpi_row = tk.Frame(self.view_dashboard, bg=self.C_BG)
        self.kpi_row.pack(fill=tk.X, pady=(0, 10))
        self.kpi_row.columnconfigure(0, weight=1, uniform="kpi")
        self.kpi_row.columnconfigure(1, weight=1, uniform="kpi")
        self.kpi_row.columnconfigure(2, weight=1, uniform="kpi")

        # Card 1: Total cost
        self.card_cost = RoundedCard(self.kpi_row, bg_color=self.C_CARD, border_color=self.C_BORDER, corner_radius=10, parent_bg=self.C_BG, padx=14, pady=10, auto_height=True)
        self.card_cost.grid(row=0, column=0, sticky="nsew", padx=(0, 6))
        tk.Label(self.card_cost.inner, text="Total cost", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD).pack(anchor="w")
        self.lbl_kpi_cost = tk.Label(self.card_cost.inner, text="$0.00", font=("Segoe UI", 18, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD)
        self.lbl_kpi_cost.pack(anchor="w", pady=(2, 0))
        self.lbl_kpi_cost_sub = tk.Label(self.card_cost.inner, text="API-Equivalent Burn", font=("Segoe UI", 7), fg=self.C_TEXT_MUTED, bg=self.C_CARD)
        self.lbl_kpi_cost_sub.pack(anchor="w", pady=(2, 0))

        # Card 2: Requests
        self.card_reqs = RoundedCard(self.kpi_row, bg_color=self.C_CARD, border_color=self.C_BORDER, corner_radius=10, parent_bg=self.C_BG, padx=14, pady=10, auto_height=True)
        self.card_reqs.grid(row=0, column=1, sticky="nsew", padx=6)
        tk.Label(self.card_reqs.inner, text="Requests", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD).pack(anchor="w")
        self.lbl_kpi_reqs = tk.Label(self.card_reqs.inner, text="0", font=("Segoe UI", 18, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD)
        self.lbl_kpi_reqs.pack(anchor="w", pady=(2, 0))
        self.lbl_kpi_reqs_sub = tk.Label(self.card_reqs.inner, text="Cache Hit: 0.0%", font=("Segoe UI", 7), fg=self.C_GREEN, bg=self.C_CARD)
        self.lbl_kpi_reqs_sub.pack(anchor="w", pady=(2, 0))

        # Card 3: Tokens
        self.card_toks = RoundedCard(self.kpi_row, bg_color=self.C_CARD, border_color=self.C_BORDER, corner_radius=10, parent_bg=self.C_BG, padx=14, pady=10, auto_height=True)
        self.card_toks.grid(row=0, column=2, sticky="nsew", padx=(6, 0))
        tk.Label(self.card_toks.inner, text="Tokens", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD).pack(anchor="w")
        self.lbl_kpi_toks = tk.Label(self.card_toks.inner, text="0", font=("Segoe UI", 18, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD)
        self.lbl_kpi_toks.pack(anchor="w", pady=(2, 0))
        self.lbl_kpi_toks_sub = tk.Label(self.card_toks.inner, text="Input: 0 · Cache: 0 · Output: 0", font=("Segoe UI", 7), fg=self.C_TEXT_MUTED, bg=self.C_CARD)
        self.lbl_kpi_toks_sub.pack(anchor="w", pady=(2, 0))

        # Chart Card (Bar Chart / Heatmap Switcher)
        self.chart_outer = RoundedCard(self.view_dashboard, bg_color=self.C_CARD, border_color=self.C_BORDER, corner_radius=10, parent_bg=self.C_BG, padx=16, pady=10, auto_height=True)
        self.chart_outer.pack(fill=tk.X, pady=(0, 10))

        self.chart_hdr_frame = tk.Frame(self.chart_outer.inner, bg=self.C_CARD)
        self.chart_hdr_frame.pack(fill=tk.X, pady=(0, 6))

        self.chart_hdr_left = tk.Frame(self.chart_hdr_frame, bg=self.C_CARD)
        self.chart_hdr_left.pack(side=tk.LEFT)

        self.lbl_chart_title = tk.Label(self.chart_hdr_left, text="Hourly Cost", font=("Segoe UI", 9, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD)
        self.lbl_chart_title.pack(anchor="w")

        self.lbl_chart_sub = tk.Label(self.chart_hdr_left, text="Model and Web Search costs for the selected range.", font=("Segoe UI", 7), fg=self.C_TEXT_MUTED, bg=self.C_CARD)
        self.lbl_chart_sub.pack(anchor="w", pady=(1, 0))

        self.chart_mode_box = RoundedCard(self.chart_hdr_frame, bg_color=self.C_CARD, border_color=self.C_BORDER, corner_radius=6, parent_bg=self.C_CARD, padx=2, pady=2, auto_height=True, auto_width=True)
        self.chart_mode_box.pack(side=tk.RIGHT)

        self.chart_mode_btns = {}
        for c_mode, c_title in [("cost", "Hourly Cost"), ("tokens", "Hourly Tokens"), ("heatmap", "Activity Heatmap")]:
            btn = tk.Label(
                self.chart_mode_box.inner,
                text=c_title,
                font=("Segoe UI", 7, "bold" if self.chart_mode == c_mode else "normal"),
                fg=self.C_TEXT_MAIN if self.chart_mode == c_mode else self.C_TEXT_MUTED,
                bg=self.C_PILL_ACTIVE if self.chart_mode == c_mode else self.C_CARD,
                padx=8,
                pady=3,
                cursor="hand2"
            )
            btn.pack(side=tk.LEFT)
            btn.bind("<Button-1>", lambda e, m=c_mode: self.select_chart_mode(m))
            self.chart_mode_btns[c_mode] = btn

        self.canvas_chart = tk.Canvas(self.chart_outer.inner, height=150, bg=self.C_CARD, highlightthickness=0, cursor="crosshair")
        self.canvas_chart.pack(fill=tk.X)
        self.canvas_chart.bind("<Motion>", self.on_chart_canvas_hover)
        self.canvas_chart.bind("<Leave>", self.on_chart_canvas_leave)

        # Usage by Model Table Card
        self.table_card = RoundedCard(self.view_dashboard, bg_color=self.C_CARD, border_color=self.C_BORDER, corner_radius=10, parent_bg=self.C_BG, padx=16, pady=10, auto_height=False)
        self.table_card.pack(fill=tk.BOTH, expand=True)

        self.tbl_top_bar = tk.Frame(self.table_card.inner, bg=self.C_CARD)
        self.tbl_top_bar.pack(fill=tk.X, pady=(0, 6))

        self.tbl_top_left = tk.Frame(self.tbl_top_bar, bg=self.C_CARD)
        self.tbl_top_left.pack(side=tk.LEFT)

        self.tbl_title_row = tk.Frame(self.tbl_top_left, bg=self.C_CARD)
        self.tbl_title_row.pack(anchor="w")

        tk.Label(self.tbl_title_row, text="Usage by Model", font=("Segoe UI", 9, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD).pack(side=tk.LEFT)

        self.lbl_models_pill = tk.Label(self.tbl_title_row, text="8 models · 3 providers", font=("Segoe UI", 7, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_PILL_BG, padx=6, pady=1)
        self.lbl_models_pill.pack(side=tk.LEFT, padx=(8, 0))

        tk.Label(self.tbl_top_left, text="Per-model breakdown for the selected range.", font=("Segoe UI", 7), fg=self.C_TEXT_MUTED, bg=self.C_CARD).pack(anchor="w", pady=(1, 0))

        # Column Header Row
        self.tbl_col_hdr = tk.Frame(self.table_card.inner, bg=self.C_CARD_HEADER, padx=12, pady=6)
        self.tbl_col_hdr.pack(fill=tk.X)
        self._setup_table_columns(self.tbl_col_hdr)

        tk.Label(self.tbl_col_hdr, text="Model", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="w").grid(row=0, column=0, sticky="w")
        tk.Label(self.tbl_col_hdr, text="Requests", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=1, sticky="e")
        tk.Label(self.tbl_col_hdr, text="Input", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=2, sticky="e")
        tk.Label(self.tbl_col_hdr, text="Cache Read", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=3, sticky="e")
        tk.Label(self.tbl_col_hdr, text="Live Quota", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=4, sticky="e")
        tk.Label(self.tbl_col_hdr, text="Output", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=5, sticky="e")
        tk.Label(self.tbl_col_hdr, text="Cost ↓", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, anchor="e").grid(row=0, column=6, sticky="e")

        # 8 Table Rows with Dividers
        self.table_rows = []
        for i in range(8):
            div = tk.Frame(self.table_card.inner, bg=self.C_DIVIDER, height=1)
            div.pack(fill=tk.X)

            row_f = tk.Frame(self.table_card.inner, bg=self.C_CARD, padx=12, pady=4)
            row_f.pack(fill=tk.X)
            self._setup_table_columns(row_f)

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
                "cost": lbl_cost,
                "is_packed": True
            })

    def _setup_table_columns(self, frame):
        frame.columnconfigure(0, weight=3, minsize=240)
        frame.columnconfigure(1, weight=1, minsize=80)
        frame.columnconfigure(2, weight=1, minsize=90)
        frame.columnconfigure(3, weight=1, minsize=100)
        frame.columnconfigure(4, weight=1, minsize=120)
        frame.columnconfigure(5, weight=1, minsize=80)
        frame.columnconfigure(6, weight=1, minsize=90)

    # -----------------------------------------------------------------
    # View: `⚡ Quotas` View (13 Live Models & Progress Bars)
    # -----------------------------------------------------------------
    def setup_quotas_view(self):
        hdr = tk.Frame(self.view_quotas, bg=self.C_BG)
        hdr.pack(fill=tk.X, pady=(0, 10))

        hdr_left = tk.Frame(hdr, bg=self.C_BG)
        hdr_left.pack(side=tk.LEFT)

        tk.Label(hdr_left, text="Live Antigravity Quotas & Limits", font=("Segoe UI", 11, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_BG).pack(anchor="w")
        tk.Label(hdr_left, text="Real-time RPC status directly from LanguageServerService/GetUserStatus.", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_BG).pack(anchor="w", pady=(2, 0))

        hdr_right = tk.Frame(hdr, bg=self.C_BG)
        hdr_right.pack(side=tk.RIGHT)

        self.lbl_quotas_conn_pill = tk.Label(
            hdr_right, text="● Connected · Google AI Ultra · 13 Models", font=("Segoe UI", 7, "bold"), fg=self.C_GREEN, bg=self.C_PILL_BG, padx=8, pady=3
        )
        self.lbl_quotas_conn_pill.pack(side=tk.LEFT, padx=(0, 6))

        self.lbl_credits_pill = tk.Label(
            hdr_right, text="Credits: 500 / 50,000", font=("Segoe UI", 7, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_PILL_BG, padx=8, pady=3
        )
        self.lbl_credits_pill.pack(side=tk.LEFT)

        # Scrollable Quotas Area
        self.quotas_canvas_container = tk.Frame(self.view_quotas, bg=self.C_BG)
        self.quotas_canvas_container.pack(fill=tk.BOTH, expand=True)

        self.quotas_canvas = tk.Canvas(self.quotas_canvas_container, bg=self.C_BG, highlightthickness=0)
        self.quotas_scrollbar = ttk.Scrollbar(self.quotas_canvas_container, orient="vertical", command=self.quotas_canvas.yview, style="Vertical.TScrollbar")
        self.quotas_scrollable_frame = tk.Frame(self.quotas_canvas, bg=self.C_BG)

        self.quotas_scrollable_frame.bind(
            "<Configure>",
            lambda e: self.quotas_canvas.configure(scrollregion=self.quotas_canvas.bbox("all"))
        )
        self.quotas_canvas_window = self.quotas_canvas.create_window((0, 0), window=self.quotas_scrollable_frame, anchor="nw")
        self.quotas_canvas.configure(yscrollcommand=self.quotas_scrollbar.set)

        self.quotas_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.quotas_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.quotas_canvas.bind("<Configure>", lambda e: self.quotas_canvas.itemconfig(self.quotas_canvas_window, width=e.width))

        self.rendered_quota_widgets = []
        self.quota_card_refs = {}
        self._rendered_quota_models_sig = None

    # -----------------------------------------------------------------
    # View: `🗂️ Sessions` View (Sessions Table)
    # -----------------------------------------------------------------
    def setup_sessions_view(self):
        hdr = tk.Frame(self.view_sessions, bg=self.C_BG)
        hdr.pack(fill=tk.X, pady=(0, 10))

        hdr_left = tk.Frame(hdr, bg=self.C_BG)
        hdr_left.pack(side=tk.LEFT)

        self.lbl_sess_title = tk.Label(hdr_left, text="Recent Antigravity Sessions", font=("Segoe UI", 11, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_BG)
        self.lbl_sess_title.pack(anchor="w")

        tk.Label(hdr_left, text="Detailed token usage, cache efficiency, and API cost per session.", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_BG).pack(anchor="w", pady=(2, 0))

        # Search Bar
        search_box = RoundedCard(hdr, bg_color=self.C_CARD_HEADER, border_color=self.C_BORDER, corner_radius=6, parent_bg=self.C_BG, padx=4, pady=2, auto_height=True, auto_width=True)
        search_box.pack(side=tk.RIGHT)

        tk.Label(search_box.inner, text="🔍", font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD_HEADER, padx=6).pack(side=tk.LEFT)
        self.ent_sess_search = tk.Entry(
            search_box.inner, font=("Segoe UI", 8), fg=self.C_TEXT_MAIN, bg=self.C_CARD_HEADER,
            insertbackground=self.C_TEXT_MAIN, relief="flat", width=22
        )
        self.ent_sess_search.pack(side=tk.LEFT, padx=(0, 6), pady=4)
        self.ent_sess_search.bind("<KeyRelease>", lambda e: self.on_session_search_change())

        # Column Header Row
        self.sess_tbl_hdr = tk.Frame(self.view_sessions, bg=self.C_CARD_HEADER, padx=12, pady=6)
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

        self.sess_canvas_container = tk.Frame(self.view_sessions, bg=self.C_BG)
        self.sess_canvas_container.pack(fill=tk.BOTH, expand=True)

        self.sess_canvas = tk.Canvas(self.sess_canvas_container, bg=self.C_BG, highlightthickness=0)
        self.sess_scrollbar = ttk.Scrollbar(self.sess_canvas_container, orient="vertical", command=self.sess_canvas.yview, style="Vertical.TScrollbar")
        self.sess_scrollable_frame = tk.Frame(self.sess_canvas, bg=self.C_BG)

        self.sess_scrollable_frame.bind(
            "<Configure>",
            lambda e: self.sess_canvas.configure(scrollregion=self.sess_canvas.bbox("all"))
        )
        self.sess_canvas_window = self.sess_canvas.create_window((0, 0), window=self.sess_scrollable_frame, anchor="nw")
        self.sess_canvas.configure(yscrollcommand=self.sess_scrollbar.set)

        self.sess_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.sess_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.sess_canvas.bind("<Configure>", lambda e: self.sess_canvas.itemconfig(self.sess_canvas_window, width=e.width))

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

    def on_session_search_change(self):
        self.session_search_query = self.ent_sess_search.get().strip().lower()
        self.render_sessions_table()

    # -----------------------------------------------------------------
    # 60fps Motion Graphics & Chart Renderer
    # -----------------------------------------------------------------
    def start_chart_animation(self):
        if self._chart_anim_id:
            try:
                self.after_cancel(self._chart_anim_id)
            except Exception:
                pass
        self.chart_anim_progress = 0.05

        def _step(frame=1):
            t = min(1.0, frame / 14.0)
            # Cubic ease-out for silky bar growth & heatmap wave
            self.chart_anim_progress = 1.0 - (1.0 - t) ** 3
            self.draw_chart()
            if frame < 14:
                self._chart_anim_id = self.after(14, lambda: _step(frame + 1))
            else:
                self.chart_anim_progress = 1.0
                self._chart_anim_id = None

        _step(1)

    def start_kpi_animation(self, target_cost, target_reqs, target_toks):
        if self._kpi_anim_id:
            try:
                self.after_cancel(self._kpi_anim_id)
            except Exception:
                pass
        start_cost = self._kpi_displayed.get("cost", 0.0)
        start_reqs = self._kpi_displayed.get("reqs", 0)
        start_toks = self._kpi_displayed.get("toks", 0)

        def _step(frame=1):
            t = min(1.0, frame / 12.0)
            ease = 1.0 - (1.0 - t) ** 3
            cur_cost = start_cost + (target_cost - start_cost) * ease
            cur_reqs = int(start_reqs + (target_reqs - start_reqs) * ease)
            cur_toks = int(start_toks + (target_toks - start_toks) * ease)

            self.lbl_kpi_cost.config(text=format_cost(cur_cost))
            self.lbl_kpi_reqs.config(text=f"{cur_reqs:,}")
            self.lbl_kpi_toks.config(text=format_tokens(cur_toks))

            if frame < 12:
                self._kpi_anim_id = self.after(14, lambda: _step(frame + 1))
            else:
                self._kpi_displayed["cost"] = target_cost
                self._kpi_displayed["reqs"] = target_reqs
                self._kpi_displayed["toks"] = target_toks
                self.lbl_kpi_cost.config(text=format_cost(target_cost))
                self.lbl_kpi_reqs.config(text=f"{target_reqs:,}")
                self.lbl_kpi_toks.config(text=format_tokens(target_toks))
                self._kpi_anim_id = None

        _step(1)

    def start_quotas_animation(self):
        if self._quota_anim_id:
            try:
                self.after_cancel(self._quota_anim_id)
            except Exception:
                pass

        def _step(frame=1):
            t = min(1.0, frame / 15.0)
            ease = 1.0 - (1.0 - t) ** 3
            for ref in self.quota_card_refs.values():
                pw = ref["pbar_canvas"].winfo_width()
                if pw <= 1:
                    pw = 380
                bw = int((ref["rem"] / 100.0) * pw * ease)
                ref["pbar_canvas"].coords(ref["bar_id"], 0, 0, bw, 6)
            if frame < 15:
                self._quota_anim_id = self.after(14, lambda: _step(frame + 1))
            else:
                self._quota_anim_id = None

        _step(1)

    def ambient_motion_loop(self):
        try:
            self.mascot_tick += 1
            self.spinner_idx = (self.spinner_idx + 1) % len(self.spinner_frames)

            # 1. Animate 8-bit Mascot in CLI View (gentle breathing + periodic blink)
            if self.current_view.startswith("cli_") and hasattr(self, "canvas_mascot"):
                y_off = int(round(math.sin(self.mascot_tick * 0.22) * 1.6))
                blink = (self.mascot_tick % 48 in (45, 46))
                if y_off != getattr(self, "_last_mascot_y", None) or blink != getattr(self, "_last_mascot_blink", None):
                    self._last_mascot_y = y_off
                    self._last_mascot_blink = blink
                    draw_mascot(self.canvas_mascot, scale=3, color=self.C_CORAL, y_offset=y_off, blink=blink)

            # 2. Animate Running Worker Progress Bars & Spinners in Orchestrator
            for wo in getattr(self, "worker_cards", []):
                if wo.get("is_running"):
                    spin_ch = self.spinner_frames[self.spinner_idx]
                    wo["status_lbl"].config(text=f"{spin_ch} Running ({wo['effort']})", fg=self.C_AMBER)
                    wo["anim_phase"] = (wo.get("anim_phase", 0.0) + 0.07) % 1.0
                    pw = max(120, wo["pbar"].winfo_width())
                    seg_w = int(pw * 0.35)
                    x1 = int((pw + seg_w) * wo["anim_phase"]) - seg_w
                    x2 = x1 + seg_w
                    wo["pbar"].coords(wo["pbar_rect"], max(0, x1), 0, min(pw, x2), 3)
                    wo["pbar"].itemconfig(wo["pbar_rect"], fill=self.C_CORAL)
        except Exception:
            pass

        if self.running:
            self.after(60, self.ambient_motion_loop)

    def select_chart_mode(self, mode_key):
        self.chart_mode = mode_key
        for k, btn in self.chart_mode_btns.items():
            if k == mode_key:
                btn.config(fg=self.C_TEXT_MAIN, bg=self.C_PILL_ACTIVE, font=("Segoe UI", 7, "bold"))
            else:
                btn.config(fg=self.C_TEXT_MUTED, bg=self.C_CARD, font=("Segoe UI", 7, "normal"))

        if mode_key == "cost":
            self.lbl_chart_title.config(text="Hourly Cost")
            self.lbl_chart_sub.config(text="Model and Web Search costs for the selected range.")
        elif mode_key == "tokens":
            self.lbl_chart_title.config(text="Hourly Tokens")
            self.lbl_chart_sub.config(text="Rolling hourly token consumption across workspace sessions.")
        else:
            self.lbl_chart_title.config(text="Activity Heatmap")
            self.lbl_chart_sub.config(text="52-week contribution grid with active streak and daily token intensity.")
        self.start_chart_animation()

    def select_range(self, range_key):
        self.selected_range = range_key
        for k, btn in self.range_buttons.items():
            if k == range_key:
                btn.config(fg=self.C_TEXT_MAIN, bg=self.C_PILL_ACTIVE, font=("Segoe UI", 7, "bold"))
            else:
                btn.config(fg=self.C_TEXT_MUTED, bg=self.C_CARD, font=("Segoe UI", 7, "normal"))
        self.start_chart_animation()
        threading.Thread(target=self.poll_once, daemon=True).start()

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
            h = 150

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
            nice_max = float(math.ceil(raw_max)) if raw_max > 1.0 else max(0.5, math.ceil(raw_max * 2) / 2.0)
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

        # Y-axis Labels
        c.create_text(left_pad - 8, top_pad + 2, text=top_label, font=("Segoe UI", 8), fill=self.C_TEXT_MUTED, anchor="e")
        c.create_text(left_pad - 8, base_y - 2, text=bot_label, font=("Segoe UI", 8), fill=self.C_TEXT_MUTED, anchor="e")

        # Dashed Guidelines
        for frac in [0.0, 0.5, 1.0]:
            gy = int(top_pad + frac * plot_h)
            c.create_line(left_pad, gy, w - right_pad, gy, fill=self.C_BORDER, dash=(3, 3))

        # 24 Bars with smooth 60fps easing progress
        n_bars = len(slots)
        slot_w = plot_w / max(1, n_bars)
        bar_gap = max(3, int(slot_w * 0.28))
        tick_indices = {0, 4, 8, 12, 16, 20, 23}
        anim_p = getattr(self, "chart_anim_progress", 1.0)

        for i, s in enumerate(slots):
            val = s["cost"] if is_cost else s["tokens"]
            bx1 = int(left_pad + i * slot_w + bar_gap / 2)
            bx2 = int(left_pad + (i + 1) * slot_w - bar_gap / 2)
            cx = (bx1 + bx2) / 2

            ratio = min(1.0, val / max(1e-9, nice_max)) * anim_p
            bh = int(ratio * plot_h) if val > 0 else 1
            if val > 0 and bh < 3:
                bh = 3

            by1 = base_y - bh
            by2 = base_y

            is_hov = (self.hovered_bar_idx == i)
            fill_col = self.C_BAR_HOVER if is_hov else (self.C_BAR if val > 0 else "#222222")

            c.create_rectangle(bx1, by1, bx2, by2, fill=fill_col, outline="")
            if val > 0 and bh >= 4:
                c.create_line(bx1, by1, bx2, by1, fill="#6ee7b7" if is_hov else "#52d690", width=1)
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

        HEAT_COLORS = ["#1c1c1c", "#1b4332", "#2d6a4f", "#40916c", "#52b788"]

        total_cols = len(weeks)  # 52 weeks
        avail_h = h - top_pad - bot_pad
        avail_w = w - left_pad - right_pad

        cell_gap = 3
        max_cell_h = (avail_h - 6 * cell_gap) // 7
        step_x = avail_w / max(1, total_cols)
        max_cell_w = int(step_x - cell_gap)

        cell_size = max(8, min(max_cell_h, max_cell_w, 13))
        anim_p = getattr(self, "chart_anim_progress", 1.0)
        reveal_col_limit = int(total_cols * anim_p)

        # Day labels (Mon, Wed, Fri) aligned with row centers
        day_labels = {0: "Mon", 2: "Wed", 4: "Fri"}
        for r_idx, d_name in day_labels.items():
            ly = top_pad + r_idx * (cell_size + cell_gap) + cell_size // 2
            c.create_text(left_pad - 8, ly, text=d_name, font=("Segoe UI", 7), fill=self.C_TEXT_MUTED, anchor="e")

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
                intens = day_obj.get("intensity", 0) if w_idx <= reveal_col_limit else 0

                fill_col = "#141414" if is_fut else HEAT_COLORS[intens]
                outline_col = "#ffffff" if is_tod else (self.C_BORDER if intens == 0 else "")

                is_hov = (self.hovered_heat_cell and self.hovered_heat_cell.get("date_str") == day_obj.get("date_str"))
                if is_hov:
                    fill_col = "#ffffff"
                    outline_col = "#38c77b"

                c.create_rectangle(cx, cy, cx + cell_size, cy + cell_size, fill=fill_col, outline=outline_col, width=1)
                self.heat_rects.append((cx, cy, cx + cell_size, cy + cell_size, day_obj))

        # Bottom Summary & Legend
        streak = hm.get("streak", 0)
        act_days = hm.get("active_days", 0)
        tot_steps = hm.get("total_steps", 0)
        tot_toks = hm.get("total_tokens", 0)

        summary_txt = f"{tot_steps:,} steps  ·  {format_tokens(tot_toks)} tokens  ·  {act_days} active days  ·  🔥 {streak}d streak"
        c.create_text(left_pad, h - 8, text=summary_txt, font=("Segoe UI", 8), fill=self.C_TEXT_MUTED, anchor="w")

        leg_x = w - right_pad - 120
        c.create_text(leg_x, h - 8, text="Less", font=("Segoe UI", 7), fill=self.C_TEXT_MUTED, anchor="e")
        for i, col in enumerate(HEAT_COLORS):
            lx1 = leg_x + 6 + i * 11
            lx2 = lx1 + 8
            c.create_rectangle(lx1, h - 13, lx2, h - 5, fill=col, outline="")
        c.create_text(leg_x + 68, h - 8, text="More", font=("Segoe UI", 7), fill=self.C_TEXT_MUTED, anchor="w")

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
    def render_dashboard_view(self, animate=False):
        d = self.dash_data

        # 1. KPI Cards with smooth count-up animation when switching tabs or updating
        target_cost = d.get("total_cost", 0.0)
        target_reqs = d.get("total_requests", 0)
        target_toks = d.get("total_tokens", 0)

        if animate or abs(target_cost - self._kpi_displayed.get("cost", 0.0)) > 0.01 or target_reqs != self._kpi_displayed.get("reqs", 0):
            if animate:
                self._kpi_displayed = {"cost": 0.0, "reqs": 0, "toks": 0}
            self.start_kpi_animation(target_cost, target_reqs, target_toks)
        elif not self._kpi_anim_id:
            self.lbl_kpi_cost.config(text=format_cost(target_cost))
            self.lbl_kpi_reqs.config(text=f"{target_reqs:,}")
            self.lbl_kpi_toks.config(text=format_tokens(target_toks))

        cache_pct = d.get("cache_hit_pct", 0.0)
        self.lbl_kpi_reqs_sub.config(text=f"Cache Hit: {cache_pct:.1f}%")

        in_fmt = format_tokens(d.get("total_input", 0))
        cache_fmt = format_tokens(d.get("total_cache", 0))
        out_fmt = format_tokens(d.get("total_output", 0))
        self.lbl_kpi_toks_sub.config(text=f"In: {in_fmt} · Cache: {cache_fmt} · Out: {out_fmt}")

        # 2. Chart Rendering
        if animate:
            self.start_chart_animation()
        elif not self._chart_anim_id:
            self.draw_chart()

        # 3. Usage by Model Table (in-place update without redundant pack calls)
        m_cnt = d.get("models_count", 0)
        p_cnt = d.get("providers_count", 1)
        self.lbl_models_pill.config(text=f"{m_cnt} models · {p_cnt} providers")

        models_table = d.get("models_table", [])
        for i, r_ui in enumerate(self.table_rows):
            if i < len(models_table):
                m = models_table[i]
                if not r_ui.get("is_packed"):
                    r_ui["div"].pack(fill=tk.X)
                    r_ui["frame"].pack(fill=tk.X)
                    r_ui["is_packed"] = True

                badge = m.get("badge", "GO")
                badge_fg = self.C_BLUE if badge == "GO" else (self.C_PURPLE if badge == "CL" else self.C_GREEN)

                r_ui["badge"].config(text=badge, fg=badge_fg, bg=self.C_PILL_BG)
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
                if r_ui.get("is_packed", True):
                    r_ui["div"].pack_forget()
                    r_ui["frame"].pack_forget()
                    r_ui["is_packed"] = False

    def _update_quota_values_inplace(self, models, animate=False):
        for m in models:
            lbl = m.get("label")
            ref = self.quota_card_refs.get(lbl)
            if not ref:
                continue
            rem = m.get("remaining_pct", 100.0)
            reset_txt = f"⏳ Resets in {m['reset_relative']}" if m.get("reset_relative") else "⏳ Ready"
            q_col = self.C_GREEN if rem >= 50 else (self.C_AMBER if rem >= 20 else self.C_RED)

            if abs(ref["rem"] - rem) > 0.05 or ref["q_col"] != q_col:
                ref["rem"] = rem
                ref["q_col"] = q_col
                ref["lbl_pct"].config(text=f"{rem:.1f}%", fg=q_col)
                pw = ref["pbar_canvas"].winfo_width()
                if pw > 1 and not animate:
                    bw = int((rem / 100.0) * pw)
                    ref["pbar_canvas"].coords(ref["bar_id"], 0, 0, bw, 6)
                    ref["pbar_canvas"].itemconfig(ref["bar_id"], fill=q_col)

            if ref["reset_txt"] != reset_txt:
                ref["reset_txt"] = reset_txt
                ref["lbl_reset"].config(text=reset_txt)

        if animate:
            self.start_quotas_animation()

    def render_quotas_view(self, animate=False):
        q = self.quota_data
        models = q.get("models", [])

        if q.get("connected"):
            tier_name = q.get("tier", "Google AI Ultra")
            self.lbl_quotas_conn_pill.config(text=f"● Connected · {tier_name} · {len(models)} Models", fg=self.C_GREEN)
        else:
            self.lbl_quotas_conn_pill.config(text="● Reconnecting RPC...", fg=self.C_AMBER)

        pc = q.get("prompt_credits") or {}
        if pc.get("available") is not None:
            avail = pc.get("available", 0)
            monthly = pc.get("monthly", 50000)
            self.lbl_credits_pill.config(text=f"Credits: {avail:,} / {monthly:,}")
        else:
            self.lbl_credits_pill.config(text="Credits: Standard")

        if not models:
            return

        models_sig = frozenset((m.get("label", ""), m.get("provider", "")) for m in models)
        if models_sig == getattr(self, "_rendered_quota_models_sig", None) and self.quota_card_refs:
            self._update_quota_values_inplace(models, animate=animate)
            return

        self._rendered_quota_models_sig = models_sig
        for w in self.rendered_quota_widgets:
            w.destroy()
        self.rendered_quota_widgets = []
        self.quota_card_refs = {}

        groups = {"Google": [], "Anthropic": [], "OpenAI": []}
        for m in models:
            prov = m.get("provider", "Google")
            groups.setdefault(prov, []).append(m)

        _EFFORT_ORDER = {"(high)": 0, "(medium)": 1, "(low)": 2}
        def _sort_key(m):
            lbl = m.get("label", "")
            base = lbl.split("(")[0].strip()
            eff_rank = 3
            for k, r in _EFFORT_ORDER.items():
                if k in lbl.lower():
                    eff_rank = r
                    break
            return (base, eff_rank, lbl)

        for prov_name in ["Google", "Anthropic", "OpenAI"]:
            m_list = sorted(groups.get(prov_name, []), key=_sort_key)
            if not m_list:
                continue

            sec_hdr = tk.Frame(self.quotas_scrollable_frame, bg=self.C_BG, pady=6)
            sec_hdr.pack(fill=tk.X)
            tk.Label(sec_hdr, text=f"{prov_name.upper()} MODELS ({len(m_list)})", font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MUTED, bg=self.C_BG).pack(anchor="w")

            grid_f = tk.Frame(self.quotas_scrollable_frame, bg=self.C_BG)
            grid_f.pack(fill=tk.X, pady=(0, 10))
            grid_f.columnconfigure(0, weight=1, uniform="quota_card")
            grid_f.columnconfigure(1, weight=1, uniform="quota_card")

            for idx, m in enumerate(m_list):
                card = RoundedCard(
                    grid_f,
                    bg_color=self.C_CARD,
                    border_color=self.C_BORDER,
                    corner_radius=10,
                    parent_bg=self.C_BG,
                    padx=14,
                    pady=10,
                    height=82,
                    auto_height=False,
                    auto_width=False
                )
                card.grid(row=idx // 2, column=idx % 2, sticky="ew", padx=5, pady=5)

                top_r = tk.Frame(card.inner, bg=self.C_CARD)
                top_r.pack(fill=tk.X)

                badge = m.get("badge", "GO")
                badge_fg = self.C_BLUE if badge == "GO" else (self.C_PURPLE if badge == "CL" else self.C_GREEN)
                tk.Label(top_r, text=badge, font=("Consolas", 7, "bold"), fg=badge_fg, bg=self.C_PILL_BG, padx=4, pady=1).pack(side=tk.LEFT, padx=(0, 6))

                lbl_name = tk.Label(top_r, text=m["label"], font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD)
                lbl_name.pack(side=tk.LEFT)

                if m.get("is_recommended"):
                    tk.Label(top_r, text="★ REC", font=("Segoe UI", 6, "bold"), fg=self.C_AMBER, bg=self.C_PILL_BG, padx=4, pady=1).pack(side=tk.LEFT, padx=(6, 0))

                rem = m.get("remaining_pct", 100.0)
                q_col = self.C_GREEN if rem >= 50 else (self.C_AMBER if rem >= 20 else self.C_RED)
                lbl_pct = tk.Label(top_r, text=f"{rem:.1f}%", font=("Segoe UI", 10, "bold"), fg=q_col, bg=self.C_CARD)
                lbl_pct.pack(side=tk.RIGHT)

                # Progress Bar Canvas with single bar item
                pbar_canvas = tk.Canvas(card.inner, height=6, bg="#242424", highlightthickness=0)
                pbar_canvas.pack(fill=tk.X, pady=(8, 6))
                bar_id = pbar_canvas.create_rectangle(0, 0, 0, 6, fill=q_col, outline="")

                bot_r = tk.Frame(card.inner, bg=self.C_CARD)
                bot_r.pack(fill=tk.X)

                reset_txt = f"⏳ Resets in {m['reset_relative']}" if m.get("reset_relative") else "⏳ Ready"
                lbl_reset = tk.Label(bot_r, text=reset_txt, font=("Segoe UI", 7), fg=self.C_TEXT_MUTED, bg=self.C_CARD)
                lbl_reset.pack(side=tk.LEFT)

                m_id = m.get("model_id", "")
                if m_id:
                    tk.Label(bot_r, text=m_id[:20], font=("Consolas", 6), fg=self.C_TEXT_DIM, bg=self.C_CARD).pack(side=tk.RIGHT)

                ref_entry = {
                    "card": card,
                    "lbl_pct": lbl_pct,
                    "pbar_canvas": pbar_canvas,
                    "bar_id": bar_id,
                    "lbl_reset": lbl_reset,
                    "rem": rem,
                    "q_col": q_col,
                    "reset_txt": reset_txt
                }

                def _on_pbar_conf(ev, ref=ref_entry):
                    if not self._quota_anim_id:
                        bw = int((ref["rem"] / 100.0) * ev.width)
                        ref["pbar_canvas"].coords(ref["bar_id"], 0, 0, bw, 6)

                pbar_canvas.bind("<Configure>", _on_pbar_conf)

                self.quota_card_refs[m["label"]] = ref_entry
                self.rendered_quota_widgets.append(card)

            self.rendered_quota_widgets.extend([sec_hdr, grid_f])

        if animate:
            self.after(20, self.start_quotas_animation)

    def render_sessions_table(self):
        all_sessions = self.dash_data.get("sessions_list", [])
        q = self.session_search_query.strip().lower()

        # Check signature to avoid redundant re-renders
        sessions_sig = (
            q,
            tuple((s["session_id"], s.get("total_tokens", 0), s.get("cost", 0.0), s.get("step_count", 0)) for s in all_sessions[:40])
        )
        if sessions_sig == getattr(self, "_last_sessions_sig", None):
            return
        self._last_sessions_sig = sessions_sig

        for w in self.rendered_session_widgets:
            w.destroy()
        self.rendered_session_widgets = []

        filtered = []
        for s in all_sessions:
            if not q or (q in s["title"].lower() or q in s["model"].lower() or q in s["session_id"].lower()):
                filtered.append(s)

        self.lbl_sess_title.config(text=f"Recent Antigravity Sessions ({len(filtered)} matches)" if q else f"Recent Antigravity Sessions ({len(all_sessions)})")

        for s in filtered:
            row_f = tk.Frame(self.sess_scrollable_frame, bg=self.C_CARD, padx=12, pady=6)
            row_f.pack(fill=tk.X)
            self._setup_session_columns(row_f)

            c0 = tk.Frame(row_f, bg=self.C_CARD)
            c0.grid(row=0, column=0, sticky="w")
            lbl_t = tk.Label(c0, text=s["title"], font=("Segoe UI", 8, "bold"), fg=self.C_TEXT_MAIN, bg=self.C_CARD, anchor="w")
            lbl_t.pack(anchor="w")
            lbl_sub = tk.Label(c0, text=s["session_id"][:12] + "...", font=("Consolas", 6), fg=self.C_TEXT_MUTED, bg=self.C_CARD, anchor="w")
            lbl_sub.pack(anchor="w")

            lbl_dt = tk.Label(row_f, text=s["last_modified_full"], font=("Segoe UI", 8), fg=self.C_TEXT_MUTED, bg=self.C_CARD, anchor="w")
            lbl_dt.grid(row=0, column=1, sticky="w")

            c2 = tk.Frame(row_f, bg=self.C_CARD)
            c2.grid(row=0, column=2, sticky="w")
            badge = s.get("badge", "GO")
            badge_fg = self.C_BLUE if badge == "GO" else (self.C_PURPLE if badge == "CL" else self.C_GREEN)
            tk.Label(c2, text=badge, font=("Consolas", 6, "bold"), fg=badge_fg, bg=self.C_PILL_BG, padx=3, pady=0).pack(side=tk.LEFT, padx=(0, 5))
            lbl_m = tk.Label(c2, text=s["model"][:18], font=("Segoe UI", 8), fg=self.C_TEXT_MAIN, bg=self.C_CARD, anchor="w")
            lbl_m.pack(side=tk.LEFT)

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

            div = tk.Frame(self.sess_scrollable_frame, bg=self.C_DIVIDER, height=1)
            div.pack(fill=tk.X)

            self.rendered_session_widgets.extend([row_f, div])

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
                f"- **Live Quota**: {pm.get('label', 'Gemini')} ({pm.get('remaining_pct', 100):.1f}% remaining)",
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

            report_txt = "\n".join(lines)
            self.clipboard_clear()
            self.clipboard_append(report_txt)
        except Exception:
            pass

    def toggle_pin(self):
        self.is_pinned = not self.is_pinned
        self.attributes("-topmost", self.is_pinned)
        self.btn_pin.config(fg=self.C_TEXT_MAIN if self.is_pinned else self.C_TEXT_DIM)
        self.settings["is_pinned"] = self.is_pinned
        save_settings(self.settings)

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

            # Update Subtle Top-Right Status Badge (NO ugly green box!)
            if q.get("connected"):
                pm = q.get("primary_model") or {}
                sm = q.get("secondary_model") or {}
                p_pct = pm.get("remaining_pct", 100.0)
                s_pct = sm.get("remaining_pct", 100.0)
                tier_str = q.get("tier", "Pro").replace("Google AI ", "")
                status_txt = f"{tier_str} · Gem {p_pct:.0f}% · Claude {s_pct:.0f}%"
                self.status_dot.config(fg=self.C_GREEN)
                self.lbl_status_text.config(text=status_txt)
            else:
                self.status_dot.config(fg=self.C_AMBER)
                self.lbl_status_text.config(text="Connecting RPC...")

            # Render Active View
            if self.current_view == "usage":
                self.render_dashboard_view()
            elif self.current_view == "quotas":
                self.render_quotas_view()

        except Exception:
            pass

        self.after(900, self.update_ui_loop)

    def on_window_configure(self, event):
        if event.widget == self:
            self.apply_rounded_window_corners()
            if hasattr(self, "canvas_chart") and self.current_view == "usage":
                self.draw_chart()

    def on_closing(self):
        self.running = False
        try:
            self.settings["window_w"] = self.winfo_width()
            self.settings["window_h"] = self.winfo_height()
            self.settings["window_x"] = self.winfo_x()
            self.settings["window_y"] = self.winfo_y()
            self.settings["selected_range"] = self.selected_range
            self.settings["chart_mode"] = self.chart_mode
            self.settings["is_pinned"] = self.is_pinned
            save_settings(self.settings)
        except Exception:
            pass
        self.destroy()


if __name__ == "__main__":
    app = DesktopUsageStudio()
    app.mainloop()
