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
