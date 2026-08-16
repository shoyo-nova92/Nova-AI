import difflib
import logging
import os
import re
import shutil
import subprocess
import winreg
from typing import Dict, List, Optional, Tuple

import psutil

logger = logging.getLogger(__name__)

# Common well-known alias mappings
KNOWN_ALIASES = {
    "vscode": ["visual studio code", "vs code", "code"],
    "code": ["visual studio code", "vscode"],
    "vsc": ["visual studio code", "vscode"],
    "chrome": ["google chrome"],
    "google chrome": ["chrome"],
    "edge": ["microsoft edge", "msedge"],
    "msedge": ["microsoft edge", "edge"],
    "calc": ["calculator"],
    "calculator": ["calc"],
    "notepad": ["notepad"],
    "explorer": ["file explorer", "windows explorer"],
    "file explorer": ["explorer"],
    "cmd": ["command prompt", "cmd.exe"],
    "terminal": ["windows terminal", "command prompt", "powershell"],
    "ps": ["powershell", "windows powershell"],
    "powershell": ["windows powershell"],
    "idm": ["internet download manager"],
    "sublime": ["sublime text"],
    "word": ["microsoft word", "winword"],
    "excel": ["microsoft excel"],
    "ppt": ["microsoft powerpoint", "powerpoint"],
    "powerpoint": ["powerpoint", "microsoft powerpoint"],
    "photos": ["microsoft photos"],
    "paint": ["mspaint", "paint"],
}

# Common process mapping helpers for close command
KNOWN_PROCESS_NAMES = {
    "vscode": ["code.exe", "code"],
    "visual studio code": ["code.exe", "code"],
    "vs code": ["code.exe", "code"],
    "chrome": ["chrome.exe"],
    "google chrome": ["chrome.exe"],
    "edge": ["msedge.exe"],
    "microsoft edge": ["msedge.exe"],
    "discord": ["discord.exe", "update.exe"],
    "spotify": ["spotify.exe"],
    "steam": ["steam.exe"],
    "notepad": ["notepad.exe"],
    "calculator": ["calculatorapp.exe", "calculator.exe", "calc.exe"],
    "calc": ["calculatorapp.exe", "calculator.exe", "calc.exe"],
    "paint": ["mspaint.exe", "paint.exe"],
    "word": ["winword.exe"],
    "excel": ["excel.exe"],
    "powerpoint": ["powerpnt.exe"],
    "obs": ["obs64.exe", "obs32.exe", "obs.exe"],
    "vlc": ["vlc.exe"],
    "telegram": ["telegram.exe"],
    "slack": ["slack.exe"],
    "cursor": ["cursor.exe"],
}


class AppSearchEngine:
    """Discovers, indexes, ranks, launches, and closes Windows applications."""

    def __init__(self, auto_index: bool = True):
        self.indexed_apps: List[Dict] = []
        if auto_index:
            self.refresh_index()

    def refresh_index(self):
        """Scans Windows Start Menu, Registry, and standard system paths to build the app index."""
        discovered = {}

        # 1. Start Menu Shortcuts (User & Global)
        start_menu_dirs = [
            os.path.expandvars(r"%APPDATA%\Microsoft\Windows\Start Menu\Programs"),
            os.path.expandvars(r"%ALLUSERSPROFILE%\Microsoft\Windows\Start Menu\Programs"),
        ]

        for base_dir in start_menu_dirs:
            if not os.path.exists(base_dir):
                continue
            for root, _, files in os.walk(base_dir):
                for file_name in files:
                    if file_name.lower().endswith(".lnk"):
                        name = file_name[:-4].strip()
                        full_path = os.path.join(root, file_name)
                        # Skip uninstaller / help shortcuts
                        lower_name = name.lower()
                        if "uninstall" in lower_name or "help" in lower_name or "readme" in lower_name:
                            continue
                        if name not in discovered:
                            discovered[name] = {
                                "name": name,
                                "target": full_path,
                                "type": "lnk",
                            }

        # 2. Registry App Paths
        registry_hives = [winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER]
        for hive in registry_hives:
            for subkey_path in [r"Software\Microsoft\Windows\CurrentVersion\App Paths"]:
                try:
                    with winreg.OpenKey(hive, subkey_path) as key:
                        subkeys_count = winreg.QueryInfoKey(key)[0]
                        for i in range(subkeys_count):
                            try:
                                exe_name = winreg.EnumKey(key, i)
                                with winreg.OpenKey(key, exe_name) as app_key:
                                    raw_path, _ = winreg.QueryValueEx(app_key, "")
                                    clean_path = raw_path.strip('"').strip()
                                    clean_path = os.path.expandvars(clean_path)
                                    display_name = os.path.splitext(exe_name)[0]
                                    if display_name and display_name not in discovered:
                                        discovered[display_name] = {
                                            "name": display_name,
                                            "target": clean_path if os.path.exists(clean_path) else exe_name,
                                            "type": "exe",
                                        }
                            except Exception:
                                continue
                except Exception:
                    pass

        # 3. UWP / Store Apps via Get-StartApps (PowerShell)
        try:
            cmd = ["powershell", "-NoProfile", "-NonInteractive", "-Command", "Get-StartApps | ConvertTo-Json -Compress"]
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=4)
            if proc.returncode == 0 and proc.stdout.strip():
                import json
                apps_data = json.loads(proc.stdout)
                if isinstance(apps_data, dict):
                    apps_data = [apps_data]
                for item in apps_data:
                    app_name = item.get("Name", "").strip()
                    app_id = item.get("AppID", "").strip()
                    if app_name and app_id and app_name not in discovered:
                        discovered[app_name] = {
                            "name": app_name,
                            "target": f"shell:AppsFolder\\{app_id}",
                            "type": "uwp",
                        }
        except Exception:
            pass

        # 4. Standard System Tools
        system_tools = [
            ("Notepad", "notepad.exe"),
            ("Calculator", "calc.exe"),
            ("Command Prompt", "cmd.exe"),
            ("PowerShell", "powershell.exe"),
            ("File Explorer", "explorer.exe"),
            ("Paint", "mspaint.exe"),
            ("Task Manager", "taskmgr.exe"),
        ]
        for tool_name, tool_cmd in system_tools:
            if tool_name not in discovered:
                discovered[tool_name] = {
                    "name": tool_name,
                    "target": tool_cmd,
                    "type": "system",
                }

        self.indexed_apps = list(discovered.values())
        logger.info(f"Indexed {len(self.indexed_apps)} installed Windows applications.")

    @staticmethod
    def _extract_acronym(text: str) -> str:
        """Extracts acronym from multi-word title, e.g. 'Visual Studio Code' -> 'vsc'."""
        words = re.findall(r"[A-Za-z0-9]+", text)
        if len(words) > 1:
            return "".join(w[0].lower() for w in words)
        return ""

    def rank_matches(self, query: str) -> List[Tuple[Dict, float]]:
        """
        Ranks installed applications against the user target query.
        Returns a list of (app_entry, confidence_score) sorted descending by score.
        """
        clean_query = query.strip().lower()
        if not clean_query:
            return []

        # Check known alias expansions for the query
        expanded_queries = [clean_query]
        if clean_query in KNOWN_ALIASES:
            expanded_queries.extend(KNOWN_ALIASES[clean_query])

        results = []

        for app in self.indexed_apps:
            app_name = app["name"]
            app_name_lower = app_name.lower()
            app_acronym = self._extract_acronym(app_name)

            best_score = 0.0

            for q in expanded_queries:
                # 1. Exact Match
                if q == app_name_lower:
                    score = 100.0
                # 2. Acronym Match (e.g. 'vscode' or 'vsc' == 'vsc' or 'vscode')
                elif q == app_acronym or (clean_query == "vscode" and "visual studio code" in app_name_lower):
                    score = 95.0
                # 3. Target is exact whole word in app name (e.g. 'chrome' in 'Google Chrome')
                elif re.search(rf"\b{re.escape(q)}\b", app_name_lower):
                    score = 85.0 + (len(q) / max(len(app_name_lower), 1)) * 10.0
                # 4. App name starts with query
                elif app_name_lower.startswith(q):
                    score = 80.0 + (len(q) / max(len(app_name_lower), 1)) * 10.0
                # 5. App name contains query substring
                elif q in app_name_lower:
                    score = 70.0 + (len(q) / max(len(app_name_lower), 1)) * 10.0
                # 6. Fuzzy Sequence Matcher
                else:
                    ratio = difflib.SequenceMatcher(None, q, app_name_lower).ratio()
                    score = ratio * 70.0

                if score > best_score:
                    best_score = score

            if best_score > 40.0:
                results.append((app, best_score))

        results.sort(key=lambda x: x[1], reverse=True)
        return results

    def find_best_app(self, query: str, min_confidence: float = 50.0) -> Optional[Dict]:
        """Returns the best matching app dictionary if confidence meets the threshold.

        Returns None if:
            - no matches
            - top match confidence < min_confidence
            - top two matches are too close (ambiguous)
        """
        matches = self.rank_matches(query)
        if not matches:
            return None

        best_app, best_score = matches[0]
        if best_score < min_confidence:
            return None

        # Ambiguity guard: if 2nd candidate is within 10 points, consider it ambiguous
        if len(matches) >= 2:
            _, second_score = matches[1]
            if second_score >= max(45.0, best_score - 10.0):
                return None

        return best_app
    
    @staticmethod
    def _application_identity(app: Dict) -> str:
        """
        Returns a normalized identity for an installed application.

        Different Windows representations of the same application
        (.lnk shortcut, executable, alias) should not be treated
        as separate competing applications.
        """
        name = str(app.get("name", "")).strip().lower()

        canonical_aliases = {
            "chrome": "google chrome",
            "google chrome": "google chrome",

            "vscode": "visual studio code",
            "vs code": "visual studio code",
            "code": "visual studio code",

            "edge": "microsoft edge",
            "msedge": "microsoft edge",
            "calc": "calculator",
            "calculator": "calculator",

            "notepad": "notepad",
        }

        return canonical_aliases.get(name, name)

    def find_best_app_with_candidates(self, query: str, min_confidence: float = 50.0) -> Dict:
        """Same as find_best_app, but returns a structured dict with status + candidate list.

        Structured result:
        {
            "status": "found" | "ambiguous" | "not_found",
            "app": <best app dict or None>,
            "confidence": <float score>,
            "candidates": [ (app, score), ... up to 5 ],
        }
        """
        matches = self.rank_matches(query)

        if not matches:
            return {
                "status": "not_found",
                "app": None,
                "confidence": 0.0,
                "candidates": [],
            }

        # Collapse multiple Windows representations of the same application.
        unique_matches = {}

        for app, score in matches:
            identity = self._application_identity(app)

            existing = unique_matches.get(identity)

            if existing is None or score > existing[1]:
                unique_matches[identity] = (app, score)

        matches = sorted(
            unique_matches.values(),
            key=lambda item: item[1],
            reverse=True,
        )

        top = matches[:5]

        best_app, best_score = matches[0]
        if best_score < min_confidence:
            return {"status": "not_found", "app": None, "confidence": best_score, "candidates": top}

        if len(matches) >= 2:
            _, second_score = matches[1]

            # Only treat results as ambiguous when they are essentially tied.
            if second_score >= best_score - 2.0:
                return {
                    "status": "ambiguous",
                    "app": None,
                    "confidence": best_score,
                    "candidates": top,
                }

        return {"status": "found", "app": best_app, "confidence": best_score, "candidates": top}

    def launch_app(self, query: str) -> Dict:
        """Finds and launches the matching Windows application.

        Returns structured: success=True only for confident launches.
        Never launches if ambiguous.
        """
        clean_target = query.strip()
        if not clean_target:
            return {"success": False, "reason": "No application name specified."}

        search_result = self.find_best_app_with_candidates(clean_target)

        if search_result["status"] == "ambiguous":
            candidates = search_result.get("candidates") or []
            candidate_names = [c[0]["name"] for c in candidates[:5]]
            return {
                "success": False,
                "reason": f"Ambiguous app name '{clean_target}'.",
                "status": "ambiguous",
                "confidence": search_result.get("confidence"),
                "candidates": candidate_names,
            }

        if search_result["status"] == "not_found":
            # Fallback: check if executable exists on PATH
            resolved = shutil.which(clean_target) or shutil.which(f"{clean_target}.exe")
            if resolved:
                try:
                    subprocess.Popen([resolved], shell=True)
                    return {
                        "success": True,
                        "app_name": clean_target,
                        "action": f"open {clean_target}",
                        "target": resolved,
                        "status": "found",
                        "via": "PATH",
                    }
                except Exception as exc:
                    return {"success": False, "reason": f"Failed to launch {clean_target}: {exc}"}

            return {
                "success": False,
                "reason": f"Application '{clean_target}' not found in installed apps.",
                "status": "not_found",
                "confidence": search_result.get("confidence"),
                "candidates": [c[0]["name"] for c in (search_result.get("candidates") or [])[:5]],
            }

        app = search_result["app"]
        target = app["target"]
        app_name = app["name"]
        app_type = app["type"]

        try:
            if app_type == "uwp":
                os.system(f'start "" "{target}"')
            elif app_type == "lnk":
                os.startfile(target)
            elif app_type in ("exe", "system"):
                if os.path.exists(target):
                    os.startfile(target)
                else:
                    subprocess.Popen([target], shell=True)
            else:
                os.startfile(target)

            return {
                "success": True,
                "app_name": app_name,
                "action": f"open {app_name}",
                "target": target,
                "status": "found",
                "via": app_type,
                "confidence": search_result.get("confidence"),
            }
        except Exception as exc:
            # Try subprocess fallback
            try:
                subprocess.Popen([target], shell=True)
                return {
                    "success": True,
                    "app_name": app_name,
                    "action": f"open {app_name}",
                    "target": target,
                    "status": "found",
                    "via": f"{app_type}+fallback",
                    "confidence": search_result.get("confidence"),
                }
            except Exception as e2:
                return {
                    "success": False,
                    "reason": f"Failed to launch '{app_name}': {exc} / {e2}",
                }

    def _resolve_close_targets(self, query: str) -> Dict:
        """Build a trusted set of close-match process/exe tokens.

        Only proceeds if target is trusted (known alias) OR high-confidence app match.
        Returns: {
            "status": "ok" | "ambiguous" | "not_found",
            "keywords": set(),           # trusted process name keywords
            "exe_paths": set(),          # trusted full exe paths (if available)
            "app_name": str or None,
        }
        """
        clean_target = query.strip().lower()
        if not clean_target:
            return {"status": "not_found", "keywords": set(), "exe_paths": set(), "app_name": None}

        trusted_keywords = set()
        trusted_exe_paths = set()
        resolved_app_name = None

        if clean_target in KNOWN_ALIASES:
            for a in KNOWN_ALIASES[clean_target]:
                trusted_keywords.add(a.strip().lower())
        if clean_target in KNOWN_PROCESS_NAMES:
            for p in KNOWN_PROCESS_NAMES[clean_target]:
                p_lower = p.strip().lower()
                trusted_keywords.add(p_lower)
                if p_lower.endswith(".exe"):
                    trusted_keywords.add(p_lower[:-4])

        # High-confidence app match (not ambiguous)
        search = self.find_best_app_with_candidates(clean_target)
        matched_app = None
        if search["status"] == "found":
            matched_app = search["app"]
            resolved_app_name = matched_app["name"]
            app_name_lower = matched_app["name"].lower()
            trusted_keywords.add(app_name_lower)
            # Add whole-word tokens of matched app name
            for tok in app_name_lower.split():
                t = tok.strip()
                if len(t) >= 3:
                    trusted_keywords.add(t)
            # Add target app's process name if it matches a known process name
            if app_name_lower in KNOWN_PROCESS_NAMES:
                for p in KNOWN_PROCESS_NAMES[app_name_lower]:
                    trusted_keywords.add(p.strip().lower())
            # Add matched app target exe path (if exe/system)
            app_type = matched_app.get("type") or ""
            if app_type in {"exe", "system"}:
                target_path = matched_app.get("target")
                if target_path:
                    trusted_exe_paths.add(target_path.strip().lower())
        elif search["status"] == "ambiguous":
            return {
                "status": "ambiguous",
                "keywords": trusted_keywords,
                "exe_paths": trusted_exe_paths,
                "app_name": None,
                "candidates": [c[0]["name"] for c in (search.get("candidates") or [])[:5]],
            }

        # If nothing was added AND no known_alias, then it's not_found
        if not trusted_keywords and not trusted_exe_paths and (matched_app is None):
            # Accept raw literal only if it's a reasonable unambiguous identifier
            # (single token, min length 3, no wildcards)
            single = clean_target
            if (len(single) >= 3 and re.match(r"^[a-z0-9._-]+$", single)):
                trusted_keywords.add(single)
                if single.endswith(".exe"):
                    trusted_keywords.add(single[:-4])

        return {
            "status": "ok" if (trusted_keywords or trusted_exe_paths) else "not_found",
            "keywords": trusted_keywords,
            "exe_paths": trusted_exe_paths,
            "app_name": resolved_app_name,
        }

    def _iter_matching_processes(self, resolution: Dict):
        """Yield psutil Process objects that match trusted keywords/exe paths."""
        keywords = {k for k in resolution.get("keywords", set()) if k}
        exe_paths = {e for e in resolution.get("exe_paths", set()) if e}

        if not keywords and not exe_paths:
            return

        for proc in psutil.process_iter(["pid", "name", "exe"]):
            try:
                proc_name = (proc.info.get("name") or "").lower()
                proc_name_no_ext = proc_name[:-4] if proc_name.endswith(".exe") else proc_name
                proc_exe = (proc.info.get("exe") or "").lower()

                matched = False
                if exe_paths and proc_exe in exe_paths:
                    matched = True
                elif keywords:
                    for kw in keywords:
                        kw_no_ext = kw[:-4] if kw.endswith(".exe") else kw
                        if not kw_no_ext:
                            continue
                        # exact match on process name / process name minus ext
                        if kw_no_ext == proc_name_no_ext:
                            matched = True
                            break
                        if kw == proc_name:
                            matched = True
                            break
                        if proc_exe and kw_no_ext and kw_no_ext in proc_exe:
                            matched = True
                            break
                if matched:
                    yield proc
            except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                continue

    @staticmethod
    def _is_process_alive(proc) -> bool:
        try:
            return proc.is_running()
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
            return False

    @staticmethod
    def _wait_for_exit(pids: list, timeout_seconds: float = 2.0) -> Dict[int, bool]:
        """Wait up to timeout_seconds for processes to exit. Returns {pid: is_dead}."""
        deadline = time.time() + timeout_seconds
        alive = {pid: True for pid in pids}
        while time.time() < deadline and any(alive.values()):
            for pid in list(alive.keys()):
                if not alive[pid]:
                    continue
                try:
                    proc = psutil.Process(pid)
                    if not proc.is_running():
                        alive[pid] = False
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    alive[pid] = False
            if not any(alive.values()):
                break
            time.sleep(0.1)
        # Final pass
        for pid in list(alive.keys()):
            if not alive[pid]:
                continue
            try:
                proc = psutil.Process(pid)
                alive[pid] = proc.is_running()
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                alive[pid] = False
        return {pid: (not dead) for pid, dead in alive.items()}

    def close_app(self, query: str) -> Dict:
        """Graceful-first, confidence-gated close. Force only if graceful fails.

        Flow:
            resolve target -> verify high-confidence / known alias
                          -> (ambiguous -> do not close, return structured)
                          -> graceful terminate matched processes
                          -> wait for exit, verify
                          -> for survivors: force kill
                          -> final verify, return result
        """
        clean_target = query.strip()
        if not clean_target:
            return {"success": False, "reason": "No application name specified to close."}

        resolution = self._resolve_close_targets(clean_target)

        if resolution["status"] == "ambiguous":
            return {
                "success": False,
                "reason": f"Ambiguous target '{clean_target}' — refusing to close blindly.",
                "status": "ambiguous",
                "candidates": resolution.get("candidates", []),
            }

        if resolution["status"] != "ok":
            return {
                "success": False,
                "reason": f"No trusted match found for '{clean_target}'.",
                "status": "not_found",
            }

        matched_pids = []
        matched_names = set()
        for proc in self._iter_matching_processes(resolution):
            try:
                matched_pids.append(proc.pid)
                matched_names.add(proc.info.get("name"))
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue

        if not matched_pids:
            return {
                "success": False,
                "reason": f"No running processes found matching '{clean_target}'.",
                "status": "not_running",
                "app_name": resolution.get("app_name"),
            }

        # Step 1: Graceful termination
        graceful_closed = 0
        gracefully_closed_names = set()
        for pid in matched_pids:
            try:
                proc = psutil.Process(pid)
                if proc.is_running():
                    proc_name = proc.name()
                    proc.terminate()
                    graceful_closed += 1
                    gracefully_closed_names.add(proc_name)
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue

        # Step 2: Verify graceful exit
        still_alive_after_graceful = self._wait_for_exit(matched_pids, timeout_seconds=2.0)
        survivors_after_graceful = [pid for pid, alive in still_alive_after_graceful.items() if alive]

        # Step 3: Force-kill only survivors of graceful attempt
        forced_closed = 0
        force_failed_pids = []
        if survivors_after_graceful:
            for pid in survivors_after_graceful:
                try:
                    proc = psutil.Process(pid)
                    if proc.is_running():
                        proc.kill()
                        forced_closed += 1
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    force_failed_pids.append(pid)
                    continue
            # Step 4: Final verification after force
            post_force_alive = self._wait_for_exit(survivors_after_graceful, timeout_seconds=1.5)
            final_survivors = [pid for pid, alive in post_force_alive.items() if alive]
            force_failed_pids.extend(final_survivors)

        final_verification_alive = {pid: self._is_process_alive(psutil.Process(pid)) if True else False
                                    for pid in matched_pids}
        # Do proper final verification using psutil call
        for pid in matched_pids:
            try:
                p = psutil.Process(pid)
                final_verification_alive[pid] = p.is_running()
            except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                final_verification_alive[pid] = False

        remaining_running = [pid for pid, alive in final_verification_alive.items() if alive]
        success = len(remaining_running) == 0

        result = {
            "success": success,
            "action": f"close {clean_target}",
            "status": "completed" if success else "partial",
            "app_name": resolution.get("app_name"),
            "attempted_process_ids": matched_pids,
            "matched_process_names": list(matched_names),
            "gracefully_terminated": graceful_closed,
            "gracefully_terminated_names": list(gracefully_closed_names),
            "force_terminated": forced_closed,
            "remaining_running_pids": remaining_running,
            "verification": {
                "graceful_wait_ok": not survivors_after_graceful,
                "force_was_used": forced_closed > 0,
                "all_closed": success,
            },
        }
        if not success:
            result["reason"] = f"Could not close all processes. {len(remaining_running)} still running."
        return result
