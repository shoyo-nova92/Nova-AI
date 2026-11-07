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
import shutil
import time
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
_session_mode = "not_started"
_tab_ids = {}
_downloads = []
_download_expected = False
_watched_pages = set()
_pending_popups = []



class StaleElementError(ValueError):
    pass


def failure_category(exc):
    if isinstance(exc, StaleElementError):
        return "stale_element"
    # Playwright is loaded lazily; importing this module must work without it.
    try:
        from playwright.sync_api import TimeoutError as PlaywrightTimeoutError
        if isinstance(exc, PlaywrightTimeoutError):
            return "browser_timeout"
    except ImportError:
        pass
    return "invalid_plan" if isinstance(exc, ValueError) else "browser_unavailable"


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


def validate_cdp_endpoint(value):
    parsed = urlsplit(validate_url(value))
    if parsed.hostname not in ("localhost", "127.0.0.1", "::1"):
        raise ValueError("CDP attachment must use an explicitly configured loopback endpoint.")
    return value


def watch_page(page):
    if page in _watched_pages:
        return
    _watched_pages.add(page)
    def downloaded(item):
        record = {"id": uuid.uuid4().hex, "filename": item.suggested_filename,
                  "status": "started" if _download_expected else "cancelled_unexpected"}
        _downloads.append(record)
        del _downloads[:-10]
        if not _download_expected:
            item.cancel()
    page.on("download", downloaded)
    page.on("popup", lambda popup: _pending_popups.append((page, popup)))


def adopt_popup(source):
    """Follow pages opened by this exact source, including delayed popup events."""
    global _page
    related = [popup for opener, popup in _pending_popups if opener is source and not popup.is_closed()]
    if not related:
        return False
    # Do not switch to unrelated user tabs. Related pages remain explicit evidence.
    _page = related[-1]
    _pending_popups[:] = [(opener, popup) for opener, popup in _pending_popups if opener is not source]
    watch_page(_page)
    _page.wait_for_load_state("domcontentloaded", timeout=10000)
    return True


def action_state(page):
    return page.evaluate("() => ({url:location.href,text:(document.body?.innerText||''),values:[...document.querySelectorAll('input:not([type=password]),textarea,select')].filter(e=>!/one-time-code|cc-number|cc-csc/.test(e.autocomplete||'')).map(e=>e.value)})")


def settle_action(source, before, timeout_ms):
    """Wait for a state transition or a source-associated popup, not a fixed sleep."""
    deadline = time.monotonic() + timeout_ms / 1000
    while True:
        if adopt_popup(source):
            return "new_tab_opened"
        try:
            if action_state(source) != before:
                source.wait_for_load_state("domcontentloaded", timeout=timeout_ms)
                return "state_changed"
        except Exception:
            # A document replacement may destroy an execution context mid-read.
            if source.is_closed():
                return "source_closed"
        if time.monotonic() >= deadline:
            return "no_observed_change"
        source.wait_for_timeout(min(50, max(1, int((deadline-time.monotonic())*1000))))


def tab_id(page):
    if page not in _tab_ids:
        _tab_ids[page] = uuid.uuid4().hex
    return _tab_ids[page]


def ensure_browser(channel: str = "msedge"):
    global _playwright, _context, _page, _channel, _import_error, _browser, _session_mode
    if channel not in ("msedge", "chrome"):
        raise ValueError("Browser channel must be msedge or chrome.")
    if _context is not None and _channel == channel:
        if _page is None or _page.is_closed():
            _page = _context.new_page()
        watch_page(_page)
        return
    if _context is not None:
        try:
            if _session_mode == "isolated":
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

    endpoint = os.environ.get("NOVA_BROWSER_CDP_URL", "")
    if endpoint:
        _browser = _playwright.chromium.connect_over_cdp(validate_cdp_endpoint(endpoint), timeout=10000)
        if not _browser.contexts:
            raise RuntimeError("Connected browser has no accessible context.")
        _context = _browser.contexts[0]
        _session_mode = "user_attached_cdp"
        _channel = channel
        # Do not observe or navigate an arbitrary private user tab by default.
        # Existing tabs require an explicit filtered tabs query and select_tab.
        _page = _context.new_page()
        watch_page(_page)
        return
    base = os.environ.get("LOCALAPPDATA") or str(Path.home() / "AppData" / "Local")
    profile = Path(base) / "Nova" / "BrowserProfile" / channel
    profile.mkdir(parents=True, exist_ok=True)
    try:
        _context = _playwright.chromium.launch_persistent_context(
            user_data_dir=str(profile),
            channel=channel,
            headless=False,
            accept_downloads=True,
            args=["--no-first-run", "--no-default-browser-check"],
        )
    except Exception as exc:
        raise RuntimeError(
            f"Could not launch installed browser channel '{channel}': {exc}"
        ) from exc
    _channel = channel
    _page = _context.pages[0] if _context.pages else _context.new_page()
    _session_mode = "isolated"
    watch_page(_page)


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


def observe_page(page, query="", role_filter="", offset=0, max_results=100):
    global _observation_token
    # Element IDs are opaque tokens attached to the exact DOM nodes observed,
    # not positional indexes that can drift when selector order or visibility changes.
    _observation_token = uuid.uuid4().hex
    snapshot = page.evaluate(r"""({token, query, role_filter, offset, max_results}) => {
      const visible = el => {
        const r = el.getBoundingClientRect(), s = getComputedStyle(el);
        return r.width > 0 && r.height > 0 && s.visibility !== 'hidden' && s.display !== 'none';
      };
      const out = [];
      const targets = new Map();
      const roleOf = el => {
        if (el.getAttribute('role')) return el.getAttribute('role');
        if (el.tagName === 'INPUT') {
          const type=(el.type||'text').toLowerCase();
          if (['submit','button','reset','image'].includes(type)) return 'button';
          if (['checkbox','radio'].includes(type)) return type;
          return ({search:'searchbox',number:'spinbutton',range:'slider'}[type] || 'textbox');
        }
        return ({BUTTON:'button',A:el.hasAttribute('href')?'link':'element',TEXTAREA:'textbox',SELECT:el.multiple?'listbox':'combobox',SUMMARY:'button'}[el.tagName] || (el.isContentEditable?'textbox':'element'));
      };
      const nameOf = el => {
        const referenced=(el.getAttribute('aria-labelledby')||'').split(/\s+/).filter(Boolean).map(id=>document.getElementById(id)?.textContent||'').join(' ').trim();
        const labels=[...(el.labels||[])].map(label=>label.textContent||'').join(' ').trim();
        return el.getAttribute('aria-label') || referenced || labels || el.getAttribute('placeholder') || el.getAttribute('alt') || el.getAttribute('title') || '';
      };
      const words=query.toLocaleLowerCase().split(/\s+/).filter(Boolean);
      document.querySelectorAll('[data-nova-observation-token]').forEach(el => el.removeAttribute('data-nova-observation-token'));
      const nodes = document.querySelectorAll('button, a, input, textarea, select, summary, [role], [contenteditable="true"]');
      for (const el of nodes) {
        if (!visible(el)) continue;
        const role = roleOf(el);
        const label = nameOf(el);
        const sensitive = el.type === 'password' || /one-time-code|cc-number|cc-csc/.test(el.getAttribute('autocomplete') || '');
        const fullText = (el.innerText || label || (!sensitive && el.value) || '').trim();
        const form=el.form || el.closest('form,[role="search"],fieldset,[role="group"]');
        const form_context=form ? ((nameOf(form)+' '+(form.innerText||'')+' '+[...form.querySelectorAll('input,textarea,select')].map(nameOf).join(' ')).trim().slice(0,1500)) : '';
        const row=el.closest('[role="row"],tr,[role="listitem"],article');
        const region=el.closest('[role="dialog"],[aria-modal="true"],dialog[open],aside,[role="complementary"]');
        const row_context=row ? (row.innerText||'').trim().slice(0,2000) : '';
        const region_context=region ? (nameOf(region)||region.tagName.toLowerCase()) : '';
        const searchable = (fullText + ' ' + label + ' ' + form_context + ' ' + row_context + ' ' + region_context + ' ' + (el.href || '')).toLocaleLowerCase();
        const query_matches=words.filter(word=>searchable.includes(word)).length;
        const text = fullText.slice(0, 1000);
        const active = el === document.activeElement;
        const editable = !!(el.isContentEditable || ['INPUT', 'TEXTAREA'].includes(el.tagName) && !['button', 'checkbox', 'radio', 'submit', 'reset', 'hidden', 'password', 'file'].includes((el.type || '').toLowerCase()));
        const id = token + '-' + String(out.length + 1);
        targets.set(id,el);
        out.push({id, role, text, tag: el.tagName.toLowerCase(),
                  href: el.tagName === 'A' ? el.href : '', download: el.hasAttribute('download'),
                  target: el.getAttribute('target')||'', form_context, row_context, region_context,
                  checked: el.matches('input[type="checkbox"],input[type="radio"]') ? el.checked : el.getAttribute('aria-checked'),
                  expanded: el.getAttribute('aria-expanded'), has_popup: el.getAttribute('aria-haspopup'),
                  form_action: el.form?.action||'', form_method: el.form?.method||'',
                  accessible_name_source: label?'label_or_aria':'content',
                  role_hint_match: !role_filter || role===role_filter, query_matches,
                  blocked: (() => {const r=el.getBoundingClientRect(), top=document.elementFromPoint(r.x+r.width/2,r.y+r.height/2); return !!top && top!==el && !el.contains(top);})(),
                  type: (el.getAttribute('type') || '').slice(0, 30),
                  placeholder: (el.getAttribute('placeholder') || '').slice(0, 100),
                  accessible_name: (label || (role==='button'||role==='link'?fullText:'')).slice(0, 1000), focused: active,
                  editable, visible: true, enabled: !el.matches(':disabled') && el.getAttribute('aria-disabled') !== 'true',
                  value: editable ? String(el.innerText || el.value || '').slice(0, 300) : ''});
      }
      let candidates=words.length ? out.filter(e=>e.query_matches===words.length) : out;
      let retrieval_fallback=false;
      if (!candidates.length && out.length) { retrieval_fallback=true; candidates=out; }
      if (query || role_filter) candidates.sort((a,b)=>b.query_matches-a.query_matches || Number(b.role_hint_match)-Number(a.role_hint_match));
      const matched=candidates.length;
      const selected=candidates.slice(offset,offset+max_results);
      // Tokens continue to refer to the exact selected nodes, even after ranking.
      for(const item of selected) targets.get(item.id).setAttribute('data-nova-observation-token',item.id);
      const visiblePasswords = [...document.querySelectorAll('input[type="password"]')].some(visible);
      const visibleOtp = [...document.querySelectorAll('input')].some(el => visible(el) && /one.?time|otp|verification.?code|security.?code/i.test([
        el.getAttribute('autocomplete'), el.getAttribute('name'), el.getAttribute('aria-label'), el.getAttribute('placeholder')
      ].filter(Boolean).join(' ')));
      const visibleAuthForm = [...document.querySelectorAll('form')].some(form => visible(form) &&
        [...form.querySelectorAll('input,button,[role="button"]')].some(visible) &&
        (/sign.?in|log.?in|authenticate|verify|password|one.?time.?code/i.test(form.innerText || '') ||
         [...form.querySelectorAll('input')].some(el => visible(el) && /password/i.test(el.type))));
      const active = document.activeElement;
      const readable = [];
      const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
      let current, size = 0;
      while ((current = walker.nextNode()) && size < 8000) {
        const parent = current.parentElement;
        if (!parent || parent.closest('input,textarea,select,[contenteditable],script,style') || !visible(parent)) continue;
        const text = (current.textContent || '').trim();
        if (text) { readable.push(text); size += text.length + 1; }
      }
      const activeLabel = active ? (active.getAttribute('aria-label') || active.getAttribute('placeholder') || active.getAttribute('title') || active.getAttribute('name') || active.innerText || '') : '';
      const activeEditable = !!(active && (active.isContentEditable || ['INPUT', 'TEXTAREA'].includes(active.tagName) && !['button', 'checkbox', 'radio', 'submit', 'reset', 'hidden', 'password', 'file'].includes((active.type || '').toLowerCase())));
      return {
        elements: selected,
        matched_count: matched, offset, has_more: offset + selected.length < matched,
        next_offset: offset + selected.length < matched ? offset + selected.length : null,
        retrieval_fallback, role_is_hint: true,
        observation_query: query, observation_role: role_filter,
        scroll: {x: scrollX, y: scrollY, height: document.documentElement.scrollHeight, viewport_height: innerHeight},
        focus: {present: !!active && active !== document.body && active !== document.documentElement,
                role: active ? roleOf(active) : '',
                tag: active ? active.tagName.toLowerCase() : '', label: String(activeLabel).trim().slice(0, 120),
                focused: !!active && active === document.activeElement, editable: activeEditable,
                enabled: !!active && !active.matches(':disabled') && active.getAttribute('aria-disabled') !== 'true',
                value: activeEditable ? String(active.innerText || active.value || '').slice(0, 300) : ''},
        visible_text: ((document.body && document.body.innerText) || '').slice(0, 8000),
        readable_text: readable.join('\n').slice(0,8000),
        dialogs: [...document.querySelectorAll('[role="dialog"],[aria-modal="true"],dialog[open]')].filter(visible).slice(0,5).map(el => ({text:(el.innerText||'').slice(0,8000), modal:el.matches('[aria-modal="true"],dialog[open]')})),
        regions: [...document.querySelectorAll('[role="dialog"],[aria-modal="true"],dialog[open],aside,[role="complementary"]')].filter(visible).map(el=>({role:roleOf(el),label:nameOf(el),checkboxes:[...el.querySelectorAll('input[type=checkbox],[role=checkbox]')].filter(visible).map(c=>({label:nameOf(c)||(c.parentElement?.innerText||'').trim().slice(0,300),checked:c.matches('input')?c.checked:c.getAttribute('aria-checked')})),text:(el.innerText||'').slice(0,8000),truncated:(el.innerText||'').length>8000})),
        ready_state: document.readyState,
        visible_password_input: visiblePasswords,
        visible_otp_input: visibleOtp,
        visible_auth_form: visibleAuthForm,
        document_has_body: !!document.body
      };
    }""", {"token": _observation_token, "query": query, "role_filter": role_filter, "offset": offset, "max_results": max_results})
    page_kind, auth_required, auth_evidence = classify_page(page.url, snapshot)
    return {"success": True, "browser_ready": True, "url": page.url, "title": page.title(), "active_tab_id": tab_id(page),
            "page_kind": page_kind, "auth_required": auth_required,
            "auth_evidence": auth_evidence,
            "visible_text": snapshot.get("visible_text", ""),
            "readable_text": snapshot.get("readable_text", ""),
            "session_mode": _session_mode,
            "dialogs": snapshot.get("dialogs", []),
            "regions": snapshot.get("regions", []),
            "downloads": list(_downloads[-10:]),
            "ready_state": snapshot.get("ready_state", "unknown"),
            "elements": snapshot.get("elements", []),
            **{key: snapshot[key] for key in ("matched_count", "offset", "has_more", "next_offset", "observation_query", "observation_role", "scroll", "retrieval_fallback", "role_is_hint")},
            "focus": snapshot.get("focus", {"present": False, "focused": False, "editable": False, "enabled": False}),
            "message": "Fresh DOM observation: visible interactive elements, active keyboard focus, editability, page text, and generic authentication signals."}

def execute(payload):
    global _page, _download_expected
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
    if command == "tabs":
        ensure_browser(_channel)
        target = payload.get("target", "")
        if not isinstance(target, str) or len(target)>500:
            raise ValueError("Tab filter must be a bounded string.")
        items=[]
        for page in _context.pages[:100]:
            if page.is_closed():
                continue
            url=page.url
            if target and target.casefold() not in (url+" "+page.title()).casefold():
                continue
            origin=urlsplit(url)
            item={"id":tab_id(page), "origin":origin.scheme+"://"+origin.netloc}
            if target:
                item.update(url=url,title=page.title())
            items.append(item)
            if len(items)>=20:
                break
        return {"success":True,"session_mode":_session_mode,"tabs":items,"message":"Observed connected browser tabs, filtered locally."}
    if command == "select_tab":
        if _context is None:
            raise RuntimeError("No browser session is connected.")
        matches=[page for page in _context.pages if not page.is_closed() and tab_id(page)==payload.get("target")]
        if len(matches)!=1:
            raise ValueError("Select exactly one tab ID from browser.tabs.")
        _page=matches[0]
        watch_page(_page)
        _page.bring_to_front()
        return observe_page(_page)
    if command == "navigate":
        url = validate_url(payload.get("url"))
        timeout = validate_timeout(payload.get("timeout_ms"))
        condition = payload.get("ready_condition", "domcontentloaded")
        selector = payload.get("selector", "")
        if not isinstance(selector, str):
            raise ValueError("selector must be a string.")
        ensure_browser(payload.get("browser", _channel))
        # Navigation always waits for DOMContentLoaded before checking the requested condition.
        if _session_mode == "user_attached_cdp":
            matches=[page for page in _context.pages if not page.is_closed() and page.url == url]
            if len(matches)==1:
                _page=matches[0]
            elif _page.url != "about:blank" and _page.url != url:
                _page=_context.new_page()
            watch_page(_page)
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
        # Pump pending page events before choosing the page to inspect.
        _page.evaluate("() => document.readyState")
        adopt_popup(_page)
        query, role = payload.get("query", ""), payload.get("role", "")
        offset, max_results = payload.get("offset", 0), payload.get("max_results", 100)
        if not isinstance(query, str) or not isinstance(role, str):
            raise ValueError("Observation query and role must be strings.")
        if type(offset) is not int or offset < 0 or type(max_results) is not int or not 1 <= max_results <= 1000:
            raise ValueError("Observation offset must be nonnegative and max_results from 1 to 1000; use next_offset for more.")
        return observe_page(_page, query, role, offset, max_results)
    if command == "scroll":
        if _page is None or _page.is_closed():
            raise RuntimeError("Observe or navigate a browser page before scrolling.")
        x, y = payload.get("x", 0), payload.get("y", 0)
        if type(x) not in (int, float) or type(y) not in (int, float):
            raise ValueError("Scroll coordinates must be numeric.")
        _page.evaluate("({x,y}) => window.scrollBy(x,y)", {"x": x, "y": y})
        return {"success": True, "post_action_observation": observe_page(_page), "message": "Scrolled and observed the current page."}
    if command == "evaluate_approved":
        # Only generated code matching the concrete, approved temporary proposal.
        import hashlib
        approval_path = Path(payload.get("approval_path", ""))
        approval = json.loads(approval_path.read_text(encoding="utf-8"))
        proposal_path = Path(approval["proposal_path"])
        if approval.get("approved") is not True or approval.get("language") != "browser_javascript":
            raise ValueError("A confirmed browser-JavaScript proposal is required.")
        if hashlib.sha256(proposal_path.read_bytes()).hexdigest() != approval["proposal_sha256"]:
            raise ValueError("Proposal changed after confirmation; confirm the revised code first.")
        if _page is None or _page.is_closed():
            raise RuntimeError("No controlled browser page is open.")
        value = _page.evaluate(approval["code"])
        return {"success": True, "value": value, "post_action_observation": observe_page(_page)}
    if command in ("click", "type", "download", "press", "hover"):
        if _context is None or _page is None or _page.is_closed():
            raise RuntimeError("Nova's controlled browser is not open. Launch it first.")
        _page.evaluate("() => document.readyState")
        if adopt_popup(_page):
            raise StaleElementError("A related popup became the active tab before this action was sent. Observe the new page and choose its current target; the old-page action was not dispatched.")
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
                raise StaleElementError("element_id is invalid or stale; observe again.")
            locator = _page.locator(f'[data-nova-observation-token="{element_id}"]')
            if locator.count() != 1:
                raise StaleElementError("Observed element ID is stale or no longer unique; observe again.")
            if not locator.is_visible():
                raise StaleElementError("Observed target is no longer visible; observe again.")
        else:
            if not role or not text:
                raise ValueError("Provide element_id or both role and text.")
            locator = _page.get_by_role(role, name=text, exact=True)
            count = locator.count()
            if count != 1:
                raise ValueError(f"Expected exactly one element matching role={role!r}, text={text!r}; found {count}. Observe and clarify if ambiguous.")
        if command == "download":
            directory=Path(payload.get("location", ""))
            if not directory.is_absolute() or not directory.is_dir():
                raise ValueError("Download location must be an existing absolute directory.")
            _download_expected=True
            temporary=directory/(".nova-"+uuid.uuid4().hex+".part")
            try:
                with _page.expect_download(timeout=validate_timeout(payload.get("timeout_ms"))) as transfer:
                    locator.click(timeout=10000)
                download=transfer.value
                name=download.suggested_filename
                if not name or name in (".","..") or any(ch in name for ch in ('/', '\\', ':')):
                    download.cancel()
                    raise ValueError("Download supplied an unsafe filename.")
                destination=directory/name
                if destination.exists():
                    download.cancel()
                    raise ValueError("Download destination already exists; no overwrite performed.")
                download.save_as(str(temporary))
                if download.failure():
                    raise RuntimeError("Browser reported download transfer failure.")
                # Same-directory atomic non-overwriting publication.
                os.link(temporary,destination)
                record={"id":uuid.uuid4().hex,"filename":name,"path":str(destination),"bytes":destination.stat().st_size,"status":"completed"}
                _downloads.append(record)
                del _downloads[:-10]
                return {"success":True,"completed":True,"path":str(destination),"bytes":record["bytes"],
                        "effects":["download_completed"],"post_action_observation":observe_page(_page),
                        "message":"Download completed and persisted without overwriting."}
            finally:
                _download_expected=False
                if temporary.exists():
                    temporary.unlink()
        if command == "hover":
            locator.hover(timeout=10000)
            return {"success":True,"post_action_observation":observe_page(_page),"message":"Hovered observed control and captured fresh contextual actions."}
        if command in ("click", "press"):
            if locator.evaluate("el => el.tagName === 'A' && el.hasAttribute('download')"):
                raise ValueError("Observed control is a download. Use browser.download only for a user-requested download; read-only tasks should use page evidence.")
            source=_page
            before=action_state(source)
            if command=="press":
                key=payload.get("key", "")
                if not isinstance(key,str) or not key or len(key)>100:
                    raise ValueError("A bounded Playwright keyboard key is required.")
                locator.press(key,timeout=10000)
            else:
                locator.click(timeout=10000,no_wait_after=True)
            outcome=settle_action(source,before,validate_timeout(payload.get("timeout_ms",3000)))
            effects=["new_tab_opened"] if outcome=="new_tab_opened" else []
            post_action = observe_page(_page)
            return {"success": True, "action": command, "url": _page.url, "title": _page.title(),
                    "source_tab_id":tab_id(source),"active_tab_id":tab_id(_page),
                    "action_outcome":outcome,"effect_verified":outcome in ("state_changed","new_tab_opened"),
                    "effects":effects,"post_action_observation": post_action,
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
            self.send_json(500, {"success": False, "error": str(exc), "failure_category": failure_category(exc)})


def main():
    # Loopback-only service: no LAN exposure. The C++ client starts it on demand.
    server = HTTPServer((HOST, PORT), Handler)
    server.serve_forever(poll_interval=0.25)


if __name__ == "__main__":
    main()
