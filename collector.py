import os
import sys
import re
import ssl
import json
import time
import sqlite3
import urllib.request
from datetime import datetime, timezone, timedelta, date
import psutil

AGY_DIR = os.path.expanduser(r"~\.gemini\antigravity")
BRAIN_DIR = os.path.join(AGY_DIR, "brain")
SUMMARIES_DB = os.path.join(AGY_DIR, "conversation_summaries.db")

def _get_app_dir():
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))

SETTINGS_FILE = os.path.join(_get_app_dir(), "hud_settings.json")

DEFAULT_SETTINGS = {
    "selected_view": "cli_1",          # 'cli_1', 'usage', 'orchestrator', 'quotas', 'sessions'
    "selected_range": "today_active",  # 'today_active', 'today_24h', '7d', '30d', 'all'
    "chart_mode": "cost",              # 'cost', 'tokens', 'heatmap'
    "default_effort": "medium",        # 'low', 'medium', 'high', 'max'
    "is_pinned": False,
    "window_w": 1140,
    "window_h": 780,
    "window_x": None,
    "window_y": None,
}

def load_settings():
    if os.path.exists(SETTINGS_FILE):
        try:
            with open(SETTINGS_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
                res = DEFAULT_SETTINGS.copy()
                res.update(data)
                return res
        except Exception:
            pass
    return DEFAULT_SETTINGS.copy()

def save_settings(settings):
    try:
        with open(SETTINGS_FILE, "w", encoding="utf-8") as f:
            json.dump(settings, f, indent=2)
    except Exception:
        pass

def format_tokens(n):
    if not isinstance(n, (int, float)) or n <= 0:
        return "0"
    if n >= 1_000_000_000:
        return f"{n / 1_000_000_000:.3f}B"
    if n >= 1_000_000:
        return f"{n / 1_000_000:.1f}M"
    elif n >= 1_000:
        return f"{n / 1_000:.1f}k"
    return f"{int(n):,}"

def format_cost(c):
    if not isinstance(c, (int, float)) or c <= 0:
        return "$0.00"
    if c < 0.01:
        return "<$0.01"
    return f"${c:,.2f}"

def get_model_provider_and_badge(model_label):
    lbl_low = (model_label or "").lower()
    if "claude" in lbl_low or "opus" in lbl_low or "sonnet" in lbl_low:
        return "Anthropic", "CL"
    elif "gpt" in lbl_low or "oss" in lbl_low:
        return "OpenAI", "OA"
    else:
        return "Google", "GO"

def compute_model_cost(model_label, inp_tokens, cache_tokens, out_tokens):
    """Calculates API-equivalent cost in USD based on standard provider pricing per 1M tokens."""
    lbl = (model_label or "").lower()
    if "opus" in lbl:
        in_rate, cache_rate, out_rate = 5.00, 0.50, 25.00
    elif "sonnet" in lbl:
        in_rate, cache_rate, out_rate = 3.00, 0.30, 15.00
    elif "argon" in lbl:
        if "high" in lbl:
            in_rate, cache_rate, out_rate = 1.25, 0.3125, 5.00
        elif "medium" in lbl:
            in_rate, cache_rate, out_rate = 0.80, 0.20, 3.20
        else:
            in_rate, cache_rate, out_rate = 0.50, 0.125, 2.00
    elif "gpt-oss" in lbl:
        in_rate, cache_rate, out_rate = 0.60, 0.15, 2.40
    else:
        # Gemini 3.8 Flash default
        in_rate, cache_rate, out_rate = 0.30, 0.075, 1.20

    cost = (
        (inp_tokens / 1_000_000.0) * in_rate
        + (cache_tokens / 1_000_000.0) * cache_rate
        + (out_tokens / 1_000_000.0) * out_rate
    )
    return cost

def clean_session_title(raw_text, cid):
    """Generates a clean, readable title from raw session texts or prompts."""
    if not raw_text:
        return f"Session {cid[:8]}"
    txt = raw_text
    m_req = re.search(r'<USER_REQUEST>\s*(.*?)(?:</USER_REQUEST>|\n)', txt, re.DOTALL | re.IGNORECASE)
    if m_req:
        txt = m_req.group(1)
    m_orig = re.search(r'<original_task>\s*(.*?)(?:</original_task>|\n)', txt, re.DOTALL | re.IGNORECASE)
    if m_orig:
        txt = m_orig.group(1)
    m_msg = re.search(r'\[Message\][^\n]*\bcontent=(.*?)(?:\n|$)', txt, re.DOTALL | re.IGNORECASE)
    if m_msg:
        txt = m_msg.group(1)

    txt = re.sub(r'[*_#`]', '', txt)
    lines = [l.strip() for l in txt.splitlines() if l.strip()]
    if lines:
        cleaned = lines[0]
        cleaned = re.sub(r'^(?:Task|Objective|Prompt):\s*', '', cleaned, flags=re.IGNORECASE).strip()
        if len(cleaned) > 52:
            cleaned = cleaned[:49].strip() + "..."
        if cleaned:
            return cleaned
    return f"Session {cid[:8]}"


class AntigravityLiveRPC:
    _cached_port = None
    _cached_proto = "https"
    _cached_token = None
    _ssl_ctx = None
    _last_successful_data = None
    _last_success_time = 0

    @classmethod
    def get_ssl_context(cls):
        if cls._ssl_ctx is None:
            ctx = ssl.create_default_context()
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
            cls._ssl_ctx = ctx
        return cls._ssl_ctx

    @classmethod
    def find_server_and_token(cls):
        csrf_token = None
        pids = []

        for p in psutil.process_iter(['pid', 'name', 'cmdline']):
            try:
                name = (p.info['name'] or '').lower()
                cmdline = " ".join(p.info['cmdline'] or [])
                cmdline_lower = cmdline.lower()

                if 'antigravity' in name or 'language_server' in name or 'antigravity' in cmdline_lower or 'language_server' in cmdline_lower:
                    pids.append(p.info['pid'])
                    m = re.search(r'--csrf_token[=\s]([^\s]+)', cmdline)
                    if m and not csrf_token:
                        csrf_token = m.group(1).strip('"\'')
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass

        ports = set()
        for conn in psutil.net_connections(kind='inet'):
            try:
                if conn.pid in pids and conn.status == 'LISTEN':
                    ports.add(conn.laddr.port)
            except Exception:
                pass

        return list(ports), csrf_token, pids

    @classmethod
    def call_endpoint(cls, proto, port, csrf_token):
        ctx = cls.get_ssl_context()
        url = f"{proto}://127.0.0.1:{port}/exa.language_server_pb.LanguageServerService/GetUserStatus"
        headers = {
            "Content-Type": "application/json",
            "Connect-Protocol-Version": "1"
        }
        if csrf_token:
            headers["X-Codeium-Csrf-Token"] = csrf_token

        payload = json.dumps({
            "metadata": {
                "ideName": "antigravity",
                "extensionName": "antigravity",
                "locale": "en"
            }
        }).encode("utf-8")

        req = urllib.request.Request(url, data=payload, headers=headers)
        with urllib.request.urlopen(req, context=ctx, timeout=2.0) as resp:
            if resp.status == 200:
                return json.loads(resp.read().decode("utf-8"))
        return None

    @classmethod
    def fetch_live_user_status(cls):
        if cls._cached_port and cls._cached_token:
            try:
                data = cls.call_endpoint(cls._cached_proto, cls._cached_port, cls._cached_token)
                if data:
                    cls._last_successful_data = data
                    cls._last_success_time = time.time()
                    return data
            except Exception:
                alt_proto = "http" if cls._cached_proto == "https" else "https"
                try:
                    data = cls.call_endpoint(alt_proto, cls._cached_port, cls._cached_token)
                    if data:
                        cls._cached_proto = alt_proto
                        cls._last_successful_data = data
                        cls._last_success_time = time.time()
                        return data
                except Exception:
                    pass

        ports, token, _ = cls.find_server_and_token()
        if token:
            cls._cached_token = token

        csrf_token = cls._cached_token

        for port in sorted(ports):
            for proto in ["https", "http"]:
                try:
                    data = cls.call_endpoint(proto, port, csrf_token)
                    if data:
                        cls._cached_port = port
                        cls._cached_proto = proto
                        cls._last_successful_data = data
                        cls._last_success_time = time.time()
                        return data
                except Exception:
                    pass

        if cls._last_successful_data and (time.time() - cls._last_success_time < 30):
            return cls._last_successful_data

        return None


class AntigravityDataCollector:
    _session_cache = {}
    _latest_detected_model = None

    @staticmethod
    def get_process_stats():
        total_mem = 0
        total_cpu = 0.0
        count = 0
        pids = []
        for p in psutil.process_iter(['pid', 'name', 'cmdline', 'cpu_percent', 'memory_info']):
            try:
                name = (p.info['name'] or '').lower()
                cmdline = " ".join(p.info['cmdline'] or []).lower()
                if 'antigravity' in name or 'language_server' in name or 'antigravity' in cmdline:
                    count += 1
                    pids.append(p.info['pid'])
                    mem_info = p.info.get('memory_info')
                    if mem_info:
                        total_mem += mem_info.rss
                    if p.info.get('cpu_percent') is not None:
                        total_cpu += p.info['cpu_percent']
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass
        return {
            "running": count > 0,
            "process_count": count,
            "memory_mb": round(total_mem / (1024 * 1024), 1),
            "cpu_percent": round(total_cpu, 1),
            "pids": pids
        }

    @classmethod
    def get_real_quota_data(cls, selected_model_pref=None):
        raw = AntigravityLiveRPC.fetch_live_user_status()
        if not raw:
            return {
                "connected": False,
                "tier": "Reconnecting...",
                "user_name": "",
                "email": "",
                "models": [],
                "models_by_label": {},
                "primary_model": None,
                "secondary_model": None,
                "prompt_credits": None
            }

        user_status = raw.get("userStatus", {})
        tier_info = user_status.get("userTier", {})
        tier_name = tier_info.get("name", "Google AI Pro")
        user_name = user_status.get("name", "")
        email = user_status.get("email", "")

        plan_status = user_status.get("planStatus", {})
        plan_info = plan_status.get("planInfo", {})
        avail_prompt = plan_status.get("availablePromptCredits", 0)
        monthly_prompt = plan_info.get("monthlyPromptCredits", 50000)

        cascade_data = user_status.get("cascadeModelConfigData", {})
        configs = cascade_data.get("clientModelConfigs", [])

        models = []
        models_by_label = {}
        now_dt = datetime.now(timezone.utc)

        for cfg in configs:
            label = cfg.get("label", "")
            model_id = cfg.get("modelId", "")
            quota_info = cfg.get("quotaInfo", {})
            rem_frac = quota_info.get("remainingFraction", 1.0)
            reset_time_str = quota_info.get("resetTime", "")

            rem_pct = round(rem_frac * 100.0, 1)
            used_pct = round((1.0 - rem_frac) * 100.0, 1)

            reset_rel = ""
            if reset_time_str:
                try:
                    clean_iso = reset_time_str.replace("Z", "+00:00")
                    reset_dt = datetime.fromisoformat(clean_iso)
                    diff_sec = int((reset_dt - now_dt).total_seconds())
                    if diff_sec > 0:
                        hours = diff_sec // 3600
                        mins = (diff_sec % 3600) // 60
                        if hours > 0:
                            reset_rel = f"{hours}h {mins}m"
                        else:
                            reset_rel = f"{mins}m"
                    else:
                        reset_rel = "soon"
                except Exception:
                    reset_rel = ""

            provider, badge = get_model_provider_and_badge(label)
            lbl_low = label.lower()
            if "gemini" in lbl_low or "argon" in lbl_low or "flash" in lbl_low:
                family = "gemini"
            elif "claude" in lbl_low or "sonnet" in lbl_low or "opus" in lbl_low:
                family = "claude"
            else:
                family = "other"

            m_obj = {
                "label": label,
                "model_id": model_id,
                "family": family,
                "provider": provider,
                "badge": badge,
                "remaining_pct": rem_pct,
                "used_pct": used_pct,
                "remaining_fraction": rem_frac,
                "reset_time_raw": reset_time_str,
                "reset_relative": reset_rel,
                "is_recommended": cfg.get("isRecommended", False)
            }
            models.append(m_obj)
            models_by_label[label] = m_obj

        # Deterministic sort order so the RPC's random dict/map iteration never shuffles the UI
        _EFFORT_ORDER = {"(high)": 0, "(medium)": 1, "(low)": 2}
        def _model_sort_key(m):
            lbl = m["label"]
            base = lbl.split("(")[0].strip()
            eff_rank = 3
            for k, r in _EFFORT_ORDER.items():
                if k in lbl.lower():
                    eff_rank = r
                    break
            return (m["provider"], base, eff_rank, lbl)

        models.sort(key=_model_sort_key)

        primary = None
        if cls._latest_detected_model and cls._latest_detected_model in models_by_label:
            primary = models_by_label[cls._latest_detected_model]
        if not primary:
            consumed = [m for m in models if m["remaining_fraction"] < 0.9999]
            if consumed:
                primary = consumed[0]
            elif models:
                primary = models[0]

        secondary = None
        if primary:
            target_fam = "claude" if primary["family"] == "gemini" else "gemini"
            sec_cands = [m for m in models if m["family"] == target_fam]
            if sec_cands:
                secondary = sec_cands[0]

        return {
            "connected": True,
            "tier": tier_name,
            "user_name": user_name,
            "email": email,
            "models": models,
            "models_by_label": models_by_label,
            "primary_model": primary,
            "secondary_model": secondary,
            "prompt_credits": {
                "available": avail_prompt,
                "monthly": monthly_prompt,
            }
        }

    @staticmethod
    def get_db_session_titles():
        titles_map = {}
        if os.path.exists(SUMMARIES_DB):
            try:
                conn = sqlite3.connect(f"file:{SUMMARIES_DB}?mode=ro", uri=True)
                cur = conn.cursor()
                cur.execute("SELECT conversation_id, title, preview FROM conversation_summaries")
                for cid, title, preview in cur.fetchall():
                    t = (title or "").strip()
                    if not t:
                        t = (preview or "").strip()
                    if t:
                        titles_map[cid] = t
                conn.close()
            except Exception:
                pass
        return titles_map

    @classmethod
    def _parse_session_dir(cls, sdir):
        cid = os.path.basename(sdir)
        transcript_path = os.path.join(sdir, ".system_generated", "logs", "transcript.jsonl")
        dir_mtime = os.path.getmtime(sdir) if os.path.exists(sdir) else 0
        has_transcript = os.path.exists(transcript_path)
        t_mtime = os.path.getmtime(transcript_path) if has_transcript else 0
        t_size = os.path.getsize(transcript_path) if has_transcript else 0
        effective_mtime = max(dir_mtime, t_mtime)

        cached = cls._session_cache.get(sdir)
        if cached and cached["t_mtime"] == t_mtime and cached["t_size"] == t_size:
            cached["data"]["mtime"] = effective_mtime
            cached["data"]["is_active_recent"] = (time.time() - effective_mtime) < 60
            return cached["data"]

        dt_fallback = datetime.fromtimestamp(effective_mtime)
        fallback_date_str = dt_fallback.strftime("%Y-%m-%d")

        step_count = 0
        current_model = "Gemini 3.8 Flash (Medium)"
        detected_model = None
        first_prompt = ""
        events = []  # List of (timestamp_epoch, date_str, hour_int, model_label, inp, cache, out, cost)

        sess_input = 0
        sess_cache = 0
        sess_output = 0
        sess_requests = 0
        sess_cost = 0.0
        sess_by_model = {}
        sess_dates = set()

        if has_transcript:
            try:
                with open(transcript_path, "r", encoding="utf-8", errors="ignore") as f:
                    for line in f:
                        line_str = line.strip()
                        if not line_str:
                            continue
                        step_count += 1
                        try:
                            entry = json.loads(line_str)
                            content = entry.get("content", "")
                            
                            # Grab prompt for session title fallback
                            if not first_prompt:
                                if "<USER_REQUEST>" in content:
                                    first_prompt = content
                                elif entry.get("source") == "USER_EXPLICIT" or entry.get("type") == "USER_INPUT":
                                    first_prompt = content
                                elif "[Message]" in content and "content=" in content:
                                    first_prompt = content

                            if "<USER_SETTINGS_CHANGE>" in content:
                                m_mod = re.search(r"Model Selection` from .*? to ([^\n]+?)\.(?:\s|$)", content)
                                if m_mod:
                                    val = m_mod.group(1).strip()
                                    if val and val.lower() != "none":
                                        current_model = val
                                        detected_model = val

                            created_at = entry.get("created_at")
                            step_ts = effective_mtime
                            step_date_str = fallback_date_str
                            step_hour = dt_fallback.hour
                            if created_at:
                                try:
                                    step_dt = datetime.fromisoformat(created_at.replace("Z", "+00:00")).astimezone()
                                    step_ts = step_dt.timestamp()
                                    step_date_str = step_dt.strftime("%Y-%m-%d")
                                    step_hour = step_dt.hour
                                except Exception:
                                    pass

                            sess_dates.add(step_date_str)

                            inp = entry.get("input_tokens")
                            out = entry.get("output_tokens")
                            cache = entry.get("cache_read_tokens")
                            if inp is not None or out is not None or cache is not None:
                                inp_v = int(inp or 0)
                                cache_v = int(cache or 0)
                                out_v = int(out or 0)
                                c_val = compute_model_cost(current_model, inp_v, cache_v, out_v)

                                sess_requests += 1
                                sess_input += inp_v
                                sess_cache += cache_v
                                sess_output += out_v
                                sess_cost += c_val

                                m_agg = sess_by_model.setdefault(current_model, {"req": 0, "in": 0, "cache": 0, "out": 0, "cost": 0.0})
                                m_agg["req"] += 1
                                m_agg["in"] += inp_v
                                m_agg["cache"] += cache_v
                                m_agg["out"] += out_v
                                m_agg["cost"] += c_val

                                events.append((step_ts, step_date_str, step_hour, current_model, inp_v, cache_v, out_v, c_val))
                        except Exception:
                            pass
            except Exception:
                pass

        data = {
            "id": cid,
            "mtime": effective_mtime,
            "last_modified_full": dt_fallback.strftime("%Y-%m-%d %H:%M"),
            "date_str": fallback_date_str,
            "sess_dates": sess_dates,
            "detected_model": detected_model or current_model,
            "first_prompt": first_prompt,
            "is_active_recent": (time.time() - effective_mtime) < 60,
            "step_count": step_count,
            "requests": sess_requests,
            "input_tokens": sess_input,
            "cache_tokens": sess_cache,
            "output_tokens": sess_output,
            "total_tokens": sess_input + sess_cache + sess_output,
            "cost": sess_cost,
            "by_model": sess_by_model,
            "events": events,
        }

        cls._session_cache[sdir] = {
            "t_mtime": t_mtime,
            "t_size": t_size,
            "data": data
        }
        return data

    @classmethod
    def get_dashboard_data(cls, selected_range="today_active", quota_data=None):
        now_dt = datetime.now()
        today_date = now_dt.date()
        today_str = today_date.strftime("%Y-%m-%d")
        now_ts = time.time()

        session_dirs = []
        if os.path.exists(BRAIN_DIR):
            try:
                for d in os.listdir(BRAIN_DIR):
                    full_p = os.path.join(BRAIN_DIR, d)
                    if os.path.isdir(full_p) and d != "tempmediaStorage":
                        session_dirs.append(full_p)
            except Exception:
                pass

        parsed_sessions = []
        for sdir in session_dirs:
            try:
                parsed_sessions.append(cls._parse_session_dir(sdir))
            except Exception:
                pass

        parsed_sessions.sort(key=lambda s: s["mtime"], reverse=True)
        active_session = parsed_sessions[0] if parsed_sessions else None
        if active_session and active_session.get("detected_model"):
            cls._latest_detected_model = active_session["detected_model"]

        # Aggregate according to selected_range:
        # - 'today_active': Sessions active today (mtime is today OR has steps today)
        # - 'today_24h': Strictly events within the last 24 hours
        # - '7d': Events within the last 7 days
        # - '30d': Events within the last 30 days
        # - 'all': All logged events across all sessions
        total_cost = 0.0
        total_requests = 0
        total_tokens = 0
        total_input = 0
        total_cache = 0
        total_output = 0

        models_agg = {}
        daily_stats = {}  # date_str -> {"tokens": N, "steps": N, "cost": C}

        # 24 hourly buckets
        current_hour_dt = now_dt.replace(minute=0, second=0, microsecond=0)
        hourly_slots = []
        slot_index_by_hour = {}
        for i in range(23, -1, -1):
            slot_dt = current_hour_dt - timedelta(hours=i)
            h_int = slot_dt.hour
            slot = {
                "hour": h_int,
                "label": f"{h_int}:00",
                "display": slot_dt.strftime("%H:00"),
                "cost": 0.0,
                "tokens": 0,
                "requests": 0
            }
            slot_index_by_hour[h_int] = len(hourly_slots)
            hourly_slots.append(slot)

        cutoff_30d = now_ts - (30 * 86400)
        cutoff_7d = now_ts - (7 * 86400)
        cutoff_24h = now_ts - 86400

        for s in parsed_sessions:
            s_dt_date = datetime.fromtimestamp(s["mtime"]).date()
            is_today_session = (s_dt_date == today_date) or (today_str in s["sess_dates"])

            # Accumulate global daily statistics for the Activity Heatmap
            for (ev_ts, ev_date, ev_hour, m_label, inp_v, cache_v, out_v, c_val) in s["events"]:
                tok_v = inp_v + cache_v + out_v
                d_entry = daily_stats.setdefault(ev_date, {"tokens": 0, "steps": 0, "cost": 0.0})
                d_entry["tokens"] += tok_v
                d_entry["steps"] += 1
                d_entry["cost"] += c_val

            # Filter for KPI / Range views
            if selected_range == "today_active" and not is_today_session:
                continue

            for (ev_ts, ev_date, ev_hour, m_label, inp_v, cache_v, out_v, c_val) in s["events"]:
                if selected_range == "today_24h" and ev_ts < cutoff_24h:
                    continue
                elif selected_range == "7d" and ev_ts < cutoff_7d:
                    continue
                elif selected_range == "30d" and ev_ts < cutoff_30d:
                    continue

                tok_v = inp_v + cache_v + out_v
                total_cost += c_val
                total_requests += 1
                total_input += inp_v
                total_cache += cache_v
                total_output += out_v
                total_tokens += tok_v

                m_entry = models_agg.setdefault(m_label, {
                    "label": m_label,
                    "requests": 0,
                    "input": 0,
                    "cache_read": 0,
                    "output": 0,
                    "total_tokens": 0,
                    "cost": 0.0
                })
                m_entry["requests"] += 1
                m_entry["input"] += inp_v
                m_entry["cache_read"] += cache_v
                m_entry["output"] += out_v
                m_entry["total_tokens"] += tok_v
                m_entry["cost"] += c_val

                idx = slot_index_by_hour.get(ev_hour)
                if idx is not None:
                    hourly_slots[idx]["cost"] += c_val
                    hourly_slots[idx]["tokens"] += tok_v
                    hourly_slots[idx]["requests"] += 1

        # Real Cache Hit Percentage calculation
        cache_denom = total_input + total_cache
        cache_hit_pct = round((total_cache / cache_denom * 100.0), 1) if cache_denom > 0 else 0.0

        # Build 52-week Contribution Heatmap Grid (Full Year)
        current_week_monday = today_date - timedelta(days=today_date.weekday())
        heatmap_start_date = current_week_monday - timedelta(weeks=51)

        heatmap_weeks = []
        grid_total_tokens = 0
        grid_total_steps = 0
        active_days_count = 0
        max_daily_tok = 0

        # Determine max daily tokens for relative scaling
        for w_i in range(52):
            w_start = heatmap_start_date + timedelta(weeks=w_i)
            for d_i in range(7):
                cur_d = w_start + timedelta(days=d_i)
                d_str = cur_d.strftime("%Y-%m-%d")
                d_info = daily_stats.get(d_str, {"tokens": 0, "steps": 0, "cost": 0.0})
                if cur_d <= today_date:
                    if d_info["tokens"] > max_daily_tok:
                        max_daily_tok = d_info["tokens"]

        for w_i in range(52):
            week_days = []
            w_start = heatmap_start_date + timedelta(weeks=w_i)
            for d_i in range(7):
                cur_d = w_start + timedelta(days=d_i)
                d_str = cur_d.strftime("%Y-%m-%d")
                d_info = daily_stats.get(d_str, {"tokens": 0, "steps": 0, "cost": 0.0})

                is_future = (cur_d > today_date)
                is_today = (cur_d == today_date)

                t_count = d_info["tokens"]
                s_count = d_info["steps"]
                c_count = d_info["cost"]

                if not is_future:
                    grid_total_tokens += t_count
                    grid_total_steps += s_count
                    if t_count > 0 or s_count > 0:
                        active_days_count += 1

                # Calculate Intensity (0 to 4)
                if is_future or (t_count == 0 and s_count == 0):
                    intensity = 0
                elif max_daily_tok > 0:
                    pct = t_count / max_daily_tok
                    if pct < 0.15:
                        intensity = 1
                    elif pct < 0.40:
                        intensity = 2
                    elif pct < 0.75:
                        intensity = 3
                    else:
                        intensity = 4
                else:
                    intensity = 1

                month_lbl = ""
                if cur_d.day == 1 or (d_i == 0 and cur_d.day <= 7):
                    month_lbl = cur_d.strftime("%b")

                week_days.append({
                    "date": cur_d,
                    "date_str": d_str,
                    "display_date": cur_d.strftime("%b %d, %Y"),
                    "weekday": d_i,
                    "tokens": t_count,
                    "steps": s_count,
                    "cost": c_count,
                    "intensity": intensity,
                    "is_future": is_future,
                    "is_today": is_today,
                    "month_label": month_lbl
                })
            heatmap_weeks.append(week_days)

        # Calculate Active Streak
        streak = 0
        check_d = today_date
        today_stat = daily_stats.get(today_str, {"tokens": 0, "steps": 0})
        if today_stat["tokens"] > 0 or today_stat["steps"] > 0:
            streak += 1
            check_d -= timedelta(days=1)
        else:
            check_d -= timedelta(days=1)

        while True:
            c_str = check_d.strftime("%Y-%m-%d")
            c_st = daily_stats.get(c_str, {"tokens": 0, "steps": 0})
            if c_st["tokens"] > 0 or c_st["steps"] > 0:
                streak += 1
                check_d -= timedelta(days=1)
            else:
                break

        heatmap_data = {
            "weeks": heatmap_weeks,
            "total_steps": grid_total_steps,
            "total_tokens": grid_total_tokens,
            "active_days": active_days_count,
            "streak": streak,
            "max_daily_tokens": max_daily_tok
        }

        # Enrich Models Table with live RPC quota info
        rpc_models_map = (quota_data or {}).get("models_by_label", {})
        models_table = []
        providers_seen = set()

        for m_label, m_stat in models_agg.items():
            rpc_m = rpc_models_map.get(m_label, {})
            provider, badge = get_model_provider_and_badge(m_label)
            providers_seen.add(provider)

            models_table.append({
                "label": m_label,
                "provider": provider,
                "badge": badge,
                "requests": m_stat["requests"],
                "input": m_stat["input"],
                "cache_read": m_stat["cache_read"],
                "output": m_stat["output"],
                "total_tokens": m_stat["total_tokens"],
                "cost": m_stat["cost"],
                "remaining_pct": rpc_m.get("remaining_pct"),
                "reset_relative": rpc_m.get("reset_relative", "")
            })

        # Sort by Cost descending, then Tokens descending
        models_table.sort(key=lambda x: (x["cost"], x["total_tokens"]), reverse=True)

        # Include key active RPC models up to 8 if fewer are active
        key_models = [
            "Gemini 4 Argon (High)",
            "Gemini 4 Argon (Medium)",
            "Gemini 3.8 Flash (Medium)",
            "Claude Sonnet 5.5 (High)",
            "Claude Opus 5.5 (High)",
            "GPT-OSS 120B (Medium)"
        ]
        for fallback_lbl in key_models:
            if len(models_table) >= 8:
                break
            if all(m["label"] != fallback_lbl for m in models_table) and fallback_lbl in rpc_models_map:
                rm = rpc_models_map[fallback_lbl]
                providers_seen.add(rm["provider"])
                models_table.append({
                    "label": fallback_lbl,
                    "provider": rm["provider"],
                    "badge": rm["badge"],
                    "requests": 0,
                    "input": 0,
                    "cache_read": 0,
                    "output": 0,
                    "total_tokens": 0,
                    "cost": 0.0,
                    "remaining_pct": rm.get("remaining_pct"),
                    "reset_relative": rm.get("reset_relative", "")
                })

        # Build Sessions List (Top 40 Recent Sessions)
        db_titles = cls.get_db_session_titles()
        sessions_list = []
        for s in parsed_sessions[:40]:
            cid = s["id"]
            raw_title = db_titles.get(cid) or s.get("first_prompt") or f"Session {cid[:8]}"
            clean_title = clean_session_title(raw_title, cid)
            m_used = s.get("detected_model") or "Gemini"
            provider, badge = get_model_provider_and_badge(m_used)

            sessions_list.append({
                "session_id": cid,
                "title": clean_title,
                "model": m_used,
                "provider": provider,
                "badge": badge,
                "step_count": s["step_count"],
                "requests": s["requests"],
                "input_tokens": s["input_tokens"],
                "cache_tokens": s["cache_tokens"],
                "output_tokens": s["output_tokens"],
                "total_tokens": s["total_tokens"],
                "cost": s["cost"],
                "last_modified_full": s["last_modified_full"],
                "is_active_recent": s["is_active_recent"]
            })

        return {
            "selected_range": selected_range,
            "active_session": active_session,
            "total_cost": total_cost,
            "total_requests": total_requests,
            "total_tokens": total_tokens,
            "total_input": total_input,
            "total_cache": total_cache,
            "total_output": total_output,
            "cache_hit_pct": cache_hit_pct,
            "hourly_slots": hourly_slots,
            "heatmap": heatmap_data,
            "models_table": models_table[:8],
            "models_count": len(models_table),
            "providers_count": max(1, len(providers_seen)),
            "sessions_list": sessions_list
        }
