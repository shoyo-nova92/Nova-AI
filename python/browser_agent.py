"""Local, persistent Playwright service used only by Nova's Branch 3 browser primitives.

Install once in Nova's active Python environment:
    python -m pip install playwright
The service uses the installed Edge/Chrome channel and a dedicated persistent profile.
"""
from __future__ import annotations

import json
import os
import re
import sys
import uuid
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import urlsplit

HOST = "127.0.0.1"
PORT = 47621
MAX_BODY = 64 * 1024
_browser = None
_context = None
_page = None
_channel = "msedge"
_playwright = None
_import_error = None
_observation_token = ""


def validate_url(value: str) -> str:
    if not isinstance(value, str) or not value or len(value) > 8192:
        raise ValueError("URL must be a non-empty string no longer than 8192 characters.")
    if any(ord(ch) < 0x20 or ch.isspace() for ch in value):
        raise ValueError("URL must not contain whitespace or control characters.")
    parsed = urlsplit(value)
    if parsed.scheme.lower() not in ("http", "https"):
        raise ValueError("Only absolute HTTP and HTTPS URLs are allowed.")
    if not parsed.hostname or parsed.username is not None or parsed.password is not None:
        raise ValueError("URL must contain a hostname and must not contain embedded credentials.")
    try:
        _ = parsed.port
    except ValueError as exc:
        raise ValueError("URL contains an invalid port.") from exc
    return value


def validate_timeout(value) -> int:
    if value is None:
        return 30000
    if isinstance(value, bool) or not isinstance(value, int) or not 1000 <= value <= 120000:
        raise ValueError("timeout_ms must be an integer between 1000 and 120000.")
    return value


def ensure_browser(channel: str = "msedge"):
    global _playwright, _context, _page, _channel, _import_error
    if channel not in ("msedge", "chrome"):
        raise ValueError("Browser channel must be msedge or chrome.")
    if _context is not None and _channel == channel:
        if _page is None or _page.is_closed():
            _page = _context.new_page()
        return
    if _context is not None:
        try:
            _context.close()
        except Exception:
            pass
        _context = None
        _page = None
    try:
        from playwright.sync_api import sync_playwright
        _playwright = _playwright or sync_playwright().start()
    except Exception as exc:
        _import_error = str(exc)
        raise RuntimeError(
            "Playwright is unavailable in Nova's active Python environment. "
            "Run: python -m pip install playwright"
        ) from exc

    base = os.environ.get("LOCALAPPDATA") or str(Path.home() / "AppData" / "Local")
    profile = Path(base) / "Nova" / "BrowserProfile" / channel
    profile.mkdir(parents=True, exist_ok=True)
    try:
        _context = _playwright.chromium.launch_persistent_context(
            user_data_dir=str(profile),
            channel=channel,
            headless=False,
            args=["--no-first-run", "--no-default-browser-check"],
        )
    except Exception as exc:
        raise RuntimeError(
            f"Could not launch installed browser channel '{channel}': {exc}"
        ) from exc
    _channel = channel
    _page = _context.pages[0] if _context.pages else _context.new_page()


def wait_for_condition(page, condition: str, selector: str, timeout_ms: int):
    allowed = {"domcontentloaded", "load", "networkidle", "selector"}
    if condition not in allowed:
        raise ValueError("ready_condition must be domcontentloaded, load, networkidle, or selector.")
    if condition == "selector" and not selector:
        raise ValueError("selector is required when ready_condition is selector.")
    if selector and len(selector) > 2048:
        raise ValueError("selector must be at most 2048 characters.")

    if condition == "selector":
        page.locator(selector).first.wait_for(state="visible", timeout=timeout_ms)
    else:
        page.wait_for_load_state(condition, timeout=timeout_ms)
        # Confirm a live DOM exists after the requested load state.
        page.wait_for_function(
            "() => !!document.documentElement && !!document.body",
            timeout=timeout_ms,
        )
        if selector:
            page.locator(selector).first.wait_for(state="visible", timeout=timeout_ms)

    return page.evaluate("""() => ({
        readyState: document.readyState,
        hasDocumentElement: !!document.documentElement,
        hasBody: !!document.body,
        bodyVisible: !!document.body && getComputedStyle(document.body).visibility !== 'hidden'
    })""")


def classify_page(url: str, snapshot: dict) -> tuple[str, bool, list[str]]:
    """Classify only from generic DOM evidence; never infer from a site/domain name."""
    if url == "about:blank":
        return "blank", False, []
    evidence = []
    for item in snapshot.get("auth_evidence", []):
        if item not in evidence:
            evidence.append(item)
    # A visible password field is strong generic evidence of an authentication form.
    if snapshot.get("visible_password_input"):
        evidence.append("visible_password_input")
    # One-time-code fields are useful evidence but are not enough alone to label a page login.
    if snapshot.get("visible_auth_form"):
        evidence.append("visible_auth_form")
    auth_required = bool(snapshot.get("visible_password_input") or snapshot.get("visible_auth_form"))
    return ("auth_required" if auth_required else "content"), auth_required, evidence


def observe_page(page):
    global _observation_token
    # Element IDs are opaque tokens attached to the exact DOM nodes observed,
    # not positional indexes that can drift when selector order or visibility changes.
    _observation_token = uuid.uuid4().hex
    snapshot = page.evaluate(r"""(token) => {
      const visible = el => {
        const r = el.getBoundingClientRect(), s = getComputedStyle(el);
        return r.width > 0 && r.height > 0 && s.visibility !== 'hidden' && s.display !== 'none';
      };
      const out = [];
      document.querySelectorAll('[data-nova-observation-token]').forEach(el => el.removeAttribute('data-nova-observation-token'));
      const nodes = document.querySelectorAll('button, a, input, textarea, select, [role], [contenteditable="true"]');
      for (const el of nodes) {
        if (out.length >= 60) break;
        if (!visible(el)) continue;
        const role = el.getAttribute('role') || ({BUTTON:'button', A:'link', INPUT: (el.type === 'checkbox' ? 'checkbox' : 'textbox'), TEXTAREA:'textbox', SELECT:'combobox'}[el.tagName] || (el.isContentEditable ? 'textbox' : 'element'));
        const label = el.getAttribute('aria-label') || el.getAttribute('placeholder') || el.getAttribute('title') || el.getAttribute('name') || '';
        const text = (el.innerText || label || el.value || '').trim().slice(0, 120);
        const active = el === document.activeElement;
        const editable = !!(el.isContentEditable || ['INPUT', 'TEXTAREA'].includes(el.tagName) && !['button', 'checkbox', 'radio', 'submit', 'reset', 'hidden'].includes((el.type || '').toLowerCase()));
        const id = token + '-' + String(out.length + 1);
        el.setAttribute('data-nova-observation-token', id);
        out.push({id, role, text, tag: el.tagName.toLowerCase(),
                  type: (el.getAttribute('type') || '').slice(0, 30),
                  placeholder: (el.getAttribute('placeholder') || '').slice(0, 100),
                  accessible_name: label.slice(0, 120), focused: active,
                  editable, enabled: !el.matches(':disabled') && el.getAttribute('aria-disabled') !== 'true',
                  value: editable ? String(el.innerText || el.value || '').slice(0, 300) : ''});
      }
      const visiblePasswords = [...document.querySelectorAll('input[type="password"]')].some(visible);
      const visibleOtp = [...document.querySelectorAll('input')].some(el => visible(el) && /one.?time|otp|verification.?code|security.?code/i.test([
        el.getAttribute('autocomplete'), el.getAttribute('name'), el.getAttribute('aria-label'), el.getAttribute('placeholder')
      ].filter(Boolean).join(' ')));
      const visibleAuthForm = [...document.querySelectorAll('form')].some(form => visible(form) &&
        [...form.querySelectorAll('input,button,[role="button"]')].some(visible) &&
        (/sign.?in|log.?in|authenticate|verify|password|one.?time.?code/i.test(form.innerText || '') ||
         [...form.querySelectorAll('input')].some(el => visible(el) && /password/i.test(el.type))));
      const active = document.activeElement;
      const activeLabel = active ? (active.getAttribute('aria-label') || active.getAttribute('placeholder') || active.getAttribute('title') || active.getAttribute('name') || active.innerText || '') : '';
      const activeEditable = !!(active && (active.isContentEditable || ['INPUT', 'TEXTAREA'].includes(active.tagName) && !['button', 'checkbox', 'radio', 'submit', 'reset', 'hidden'].includes((active.type || '').toLowerCase())));
      return {
        elements: out,
        focus: {present: !!active && active !== document.body && active !== document.documentElement,
                role: active ? (active.getAttribute('role') || ({INPUT:'textbox', TEXTAREA:'textbox'}[active.tagName] || (active.isContentEditable ? 'textbox' : 'element'))) : '',
                tag: active ? active.tagName.toLowerCase() : '', label: String(activeLabel).trim().slice(0, 120),
                focused: !!active && active === document.activeElement, editable: activeEditable,
                enabled: !!active && !active.matches(':disabled') && active.getAttribute('aria-disabled') !== 'true',
                value: activeEditable ? String(active.innerText || active.value || '').slice(0, 300) : ''},
        visible_text: ((document.body && document.body.innerText) || '').slice(0, 2400),
        ready_state: document.readyState,
        visible_password_input: visiblePasswords,
        visible_otp_input: visibleOtp,
        visible_auth_form: visibleAuthForm,
        document_has_body: !!document.body
      };
    }""", _observation_token)
    page_kind, auth_required, auth_evidence = classify_page(page.url, snapshot)
    return {"success": True, "browser_ready": True, "url": page.url, "title": page.title(),
            "page_kind": page_kind, "auth_required": auth_required,
            "auth_evidence": auth_evidence,
            "visible_text": snapshot.get("visible_text", ""),
            "ready_state": snapshot.get("ready_state", "unknown"),
            "elements": snapshot.get("elements", []),
            "focus": snapshot.get("focus", {"present": False, "focused": False, "editable": False, "enabled": False}),
            "message": "Fresh DOM observation: visible interactive elements, active keyboard focus, editability, page text, and generic authentication signals."}

def execute(payload):
    global _page
    if not isinstance(payload, dict):
        raise ValueError("Request body must be a JSON object.")
    command = payload.get("command")
    if command == "health":
        return {"success": True, "service": "NovaBrowserAgent", "browser_ready": _context is not None}
    if command == "launch":
        requested = payload.get("browser", "msedge")
        channel = {"edge": "msedge", "microsoft edge": "msedge", "msedge": "msedge",
                   "chrome": "chrome", "google chrome": "chrome"}.get(str(requested).lower())
        if not channel:
            raise ValueError("Only installed Edge or Chrome channels are supported by the browser agent.")
        ensure_browser(channel)
        return {"success": True, "browser": channel, "url": _page.url,
                "message": f"Controlled {channel} browser is ready."}
    if command == "navigate":
        url = validate_url(payload.get("url"))
        timeout = validate_timeout(payload.get("timeout_ms"))
        condition = payload.get("ready_condition", "domcontentloaded")
        selector = payload.get("selector", "")
        if not isinstance(selector, str):
            raise ValueError("selector must be a string.")
        ensure_browser(payload.get("browser", _channel))
        # Navigation always waits for DOMContentLoaded before checking the requested condition.
        _page.goto(url, wait_until="domcontentloaded", timeout=timeout)
        dom = wait_for_condition(_page, condition, selector, timeout)
        return {"success": True, "url": _page.url, "title": _page.title(), "dom": dom,
                "selector": selector or None, "ready_condition": condition,
                "message": "Navigation completed and DOM readiness condition satisfied."}
    if command == "wait_for":
        timeout = validate_timeout(payload.get("timeout_ms"))
        condition = payload.get("ready_condition", "domcontentloaded")
        selector = payload.get("selector", "")
        if not isinstance(selector, str):
            raise ValueError("selector must be a string.")
        ensure_browser(payload.get("browser", _channel))
        dom = wait_for_condition(_page, condition, selector, timeout)
        return {"success": True, "url": _page.url, "title": _page.title(), "dom": dom,
                "selector": selector or None, "ready_condition": condition,
                "message": "Current page DOM readiness condition satisfied."}
    if command == "observe":
        if _context is None:
            return {"success": True, "browser_ready": False, "url": "", "title": "",
                    "page_kind": "browser_not_started", "auth_required": False,
                    "auth_evidence": [], "elements": [],
                    "message": "Nova's controlled browser has not been started."}
        if _page is None or _page.is_closed():
            _page = _context.new_page()
        return observe_page(_page)
    if command in ("click", "type"):
        if _context is None or _page is None or _page.is_closed():
            raise RuntimeError("Nova's controlled browser is not open. Launch it first.")
        element_id = payload.get("element_id", "")
        role = payload.get("role", "")
        text = payload.get("text", "")
        if not all(isinstance(v, str) for v in (element_id, role, text)):
            raise ValueError("element_id, role and text must be strings.")
        if len(text) > 10000 or len(role) > 40 or len(element_id) > 80:
            raise ValueError("Element selector or text exceeds the allowed size.")
        if element_id:
            # Resolve the exact node tagged by the latest observation. Never
            # reinterpret an observed ID as an index in a newly enumerated list.
            if not re.fullmatch(r"[0-9a-f]{32}-[1-9][0-9]*", element_id):
                raise ValueError("element_id is invalid or stale; observe again.")
            locator = _page.locator(f'[data-nova-observation-token="{element_id}"]')
            if locator.count() != 1:
                raise ValueError("Observed element ID is stale or no longer unique; observe again.")
            if not locator.is_visible():
                raise ValueError("Observed target is no longer visible; observe again.")
        else:
            if not role or not text:
                raise ValueError("Provide element_id or both role and text.")
            locator = _page.get_by_role(role, name=text, exact=True)
            count = locator.count()
            if count != 1:
                raise ValueError(f"Expected exactly one element matching role={role!r}, text={text!r}; found {count}. Observe and clarify if ambiguous.")
        if command == "click":
            locator.click(timeout=10000)
            post_action = observe_page(_page)
            return {"success": True, "action": "click", "url": _page.url, "title": _page.title(),
                    "post_action_observation": post_action,
                    "message": "Click dispatched; fresh DOM observation captured for state-transition verification."}
        content = payload.get("content", "")
        if not isinstance(content, str) or len(content) > 10000:
            raise ValueError("content must be a string of at most 10000 characters.")
        # Fail closed if a stale/ambiguous semantic locator resolves to a button,
        # submit control, or other non-editable element.
        try:
            editable = locator.evaluate("el => !!(el.isContentEditable || ((el.tagName === 'INPUT' || el.tagName === 'TEXTAREA') && !['button','checkbox','radio','submit','reset','hidden','image','file'].includes((el.type || '').toLowerCase())))")
            enabled = locator.is_enabled()
        except Exception as exc:
            raise ValueError(f"Could not validate typing target: {exc}") from exc
        if not editable:
            raise ValueError("Typing target is not an editable input; observe again and select an editable element.")
        if not enabled:
            raise ValueError("Typing target is disabled; observe again.")
        locator.fill(content, timeout=10000)
        post_action = observe_page(_page)
        return {"success": True, "action": "type", "url": _page.url, "title": _page.title(),
                "post_action_observation": post_action,
                "message": "Input filled; fresh DOM observation captured for state-transition verification."}
    raise ValueError("Unsupported browser-agent command.")


class Handler(BaseHTTPRequestHandler):
    server_version = "NovaBrowserAgent/1.0"

    def log_message(self, *_args):
        return

    def send_json(self, status, body):
        raw = json.dumps(body, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        if self.path != "/health":
            self.send_json(404, {"success": False, "error": "Not found."})
            return
        self.send_json(200, {"success": True, "service": "NovaBrowserAgent"})

    def do_POST(self):
        if self.path != "/command":
            self.send_json(404, {"success": False, "error": "Not found."})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0 or length > MAX_BODY:
                raise ValueError("Request body size is invalid.")
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            result = execute(payload)
            self.send_json(200, result)
        except Exception as exc:
            self.send_json(500, {"success": False, "error": str(exc)})


def main():
    # Loopback-only service: no LAN exposure. The C++ client starts it on demand.
    server = HTTPServer((HOST, PORT), Handler)
    server.serve_forever(poll_interval=0.25)


if __name__ == "__main__":
    main()
