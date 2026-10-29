# Nova Branch 3 — Canonical Primitive Catalog

This document describes the **26 primitives registered in `src/primitive_registry.cpp`** and their current implementation in `src/primitive_execution_engine.cpp`. The registry schemas are the machine-readable contract used by `PrimitiveMapper`; if this document and the code disagree, treat the code as authoritative and update this document.

## Execution contract

- The Goal Decomposer converts a user goal into ordered logical steps. It does not choose primitives.
- The next-action planner receives the raw goal, complete guide, live registry and bounded observed state. It chooses one operation; the mapper validates it deterministically before execution.
- The host validates the mapped result against the registry and only then starts execution.
- The runtime owns dependency and verification state, performs bounded recovery and rejects premature completion. General steps use observed evidence or typed verification predicates; artifact steps use backend-verified postconditions.
- All production external requests use this runtime. Legacy phrase-based pipeline sources remain available only to their legacy tests and are not linked into Nova.
- Filesystem paths are resolved by a shared Windows-aware policy. It supports absolute paths and the `desktop`, `documents`, and `downloads` aliases, normalizes paths before use, checks reparse-point-aware physical destinations, and blocks mutations in protected Windows and application locations.

## Primitive reference

### 1. `filesystem.create`

- **Purpose:** Create one new directory under an existing authorized parent.
- **Required parameters:** `name` (single directory name), `location` (absolute path or supported known-folder alias).
- **Outputs described by registry:** `path`, `created`.
- **Side effects:** Creates a real directory. Fails if it already exists.
- **Limitations:** Does not create nested parent directories; rejects names containing path separators, `.` or `..`.

### 2. `filesystem.create_file`

- **Purpose:** Create a new file in an existing supported directory.
- **Required:** `name`, `location` (absolute path or a safe alias path such as `desktop/NovaWorkspace`).
- **Optional:** `content` (defaults to empty when omitted).
- **Outputs:** `path`, `created`.
- **Side effects:** Creates a real file and writes initial content. It will not overwrite an existing file.
- **Limitations:** Parent directory must already exist; file name must be a single path component.

### 3. `filesystem.delete`

- **Purpose:** Delete a file or directory.
- **Required:** `target` (an authorized absolute or alias path such as `desktop/NovaWorkspace/file.txt`).
- **Optional:** `recursive` (defaults to `false`).
- **Outputs:** `deleted_path`, `deleted`.
- **Side effects:** Destructive and potentially irreversible. A non-empty directory is rejected unless recursive deletion is explicitly enabled.
- **Limitations:** Traversal components are rejected for aliases/relative resolution; recursive deletion remains opt-in.

### 4. `filesystem.move`

- **Purpose:** Move a file or directory.
- **Required:** `source`, `destination` (authorized absolute or alias paths).
- **Outputs:** `path`, `moved`.
- **Side effects:** Changes the resource's location. Existing destinations are rejected.
- **Limitations:** Destination parent must exist; moving a directory into itself is rejected. The implementation uses filesystem rename semantics and may fail across volumes.

### 5. `filesystem.copy`

- **Purpose:** Copy a file or directory.
- **Required:** `source`, `destination` (authorized absolute or alias paths).
- **Optional:** `overwrite` (defaults to `false`).
- **Outputs:** `path`, `copied`.
- **Side effects:** Creates a copy. File overwrite must be explicitly requested; copying a directory to an existing destination is unsupported.
- **Limitations:** Destination parent must exist. Paths must remain inside the supported Desktop-relative area.

### 6. `filesystem.rename`

- **Purpose:** Rename a file or directory within its current parent.
- **Required:** `target`, `newName` (single path component).
- **Outputs:** `path`, `renamed`.
- **Side effects:** Changes the resource name. Existing destination names are rejected.
- **Limitations:** Does not move a resource to a different parent directory.

### Filesystem discovery primitives

- `filesystem.list_directory` lists up to 200 entries in one authorized directory. It returns the resolved directory and absolute entry paths.
- `filesystem.search` recursively searches an authorized root for a case-insensitive name substring, optionally limited to files or directories, with a maximum of 1000 returned matches.
- `filesystem.inspect` reports existence (including `exists=false`), resource type, byte size and a bounded UTF-8 text preview where applicable. A missing inspected path is an observable fact, not an execution failure.

Discovery does not guess a location. When multiple candidates remain viable, the planner must ask the user. Successful filesystem operations expose their resolved paths to later state-aware planning.

### 7. `application.launch`

- **Purpose:** Launch a registered application or open a verified file/folder using Windows associations.
- **Parameters:** Supply `application`, `target`, or both. Target must exist and pass filesystem policy.
- **Discovery:** Application names/AppIDs are resolved from Windows registrations. Ambiguous matches require clarification; there is no hand-written app-name allowlist.
- **File activation:** Desktop and packaged application contracts are selected from OS metadata. A requested target is passed through rather than silently ignored.
- **Side effects:** Windows launch/activation is requested. Acceptance is not proof that installation or application loading completed.

### 8. `application.close`

- **Purpose:** Request a window to close.
- **Required:** `application` (an executable identity observed by `environment.observe`).
- **Optional:** `target` (window-title substring).
- **Outputs:** `closed`, `application`.
- **Side effects:** Sends `WM_CLOSE` to a matching visible window. It does not forcibly terminate the process.
- **Limitations:** Success means the close message was sent, not that the process has exited. Unsaved-work dialogs may remain open. The current implementation selects a matching visible window.

### 9. `application.focus`

- **Purpose:** Bring a matching visible window to the foreground.
- **Required:** `application` (an executable identity observed by `environment.observe`).
- **Optional:** `target` (window-title substring).
- **Outputs:** `focused`, `application`.
- **Side effects:** Attempts to restore and foreground a real window.
- **Limitations:** Windows may deny foreground activation; in that case the primitive returns an execution error. The target must already have a visible window.

### 10. `text.write`

- **Purpose:** Replace the contents of an existing text file.
- **Required:** `target` (authorized existing file path), `content` (exact text).
- **Outputs:** `written`, `target`.
- **Side effects:** Opens the file with truncation, replacing its previous contents.
- **Limitations:** Target must already exist and be a regular file. Despite the generic registry description, the current implementation does not write to arbitrary GUI text fields; use `keyboard.type` for GUI input.

### 11. `keyboard.press`

- **Purpose:** Send a key press, optionally with modifiers and repetition.
- **Required:** `key`.
- **Optional:** `modifiers` (comma-separated `CTRL`, `ALT`, `SHIFT`, `WIN`), `repeatCount` (1–100; defaults to 1).
- **Outputs:** `pressed`, `key`.
- **Supported key families:** A–Z, 0–9, F1–F24, Enter/Return, Tab, Escape, Space, Backspace, Delete, Insert, Home, End, PageUp/PageDown, arrow keys, and modifier keys.
- **Side effects:** Sends real system-wide keyboard input to the current foreground input target.
- **Limitations:** GUI focus is stateful; the wrong window can receive input if the intended target is not foreground.

### 12. `keyboard.type`

- **Purpose:** Type literal text as Unicode keystrokes.
- **Required:** `content`.
- **Optional:** `target` (visible window title substring; when supplied, the engine tries to focus a matching window first).
- **Outputs:** `typed`, `character_count`.
- **Side effects:** Sends real system-wide keyboard input.
- **Limitations:** Input can be lost or redirected if Windows does not grant focus. The engine's success response confirms input events were accepted, not that a target application persisted the text.

### 13. `mouse.click`

- **Purpose:** Move the cursor and click screen coordinates.
- **Required:** `x`, `y` (integer screen coordinates).
- **Optional:** `button` (`left`, `right`, `middle`; defaults to left), `clickCount` (1 or 2; defaults to 1 in the engine).
- **Outputs:** `clicked`, `x`, `y`.
- **Side effects:** Moves the real pointer and sends real mouse events.
- **Limitations:** Coordinates must be inside the current virtual desktop bounds. The registry schema currently declares clickCount maximum 3 while the execution engine accepts only 1 or 2; the engine's stricter runtime check applies.

### 14. `mouse.move`

- **Purpose:** Move the pointer without clicking.
- **Required:** `x`, `y` (integer screen coordinates).
- **Outputs:** `moved`, `x`, `y`.
- **Side effects:** Moves the real pointer.
- **Limitations:** Coordinates must be inside the current virtual desktop bounds. The registry currently declares non-negative coordinates even though Windows virtual desktops can use negative coordinates on some multi-monitor layouts.

## Required mapper behavior

1. Use only primitive names from the live catalog passed into the prompt.
2. Preserve step IDs and order; dependencies remain host-owned.
3. Supply all required schema parameters and no unknown parameters.
4. Do not invent resource names, paths, text, or applications.
5. Return strict JSON only. An unavailable required value must fail closed rather than be guessed.
6. Mapping is planning only. It must never trigger execution.

## Known contract gaps to keep visible

- Registry output definitions describe expected outputs, but `PrimitiveExecutionEngine` currently returns a status/message structure rather than structured output values.
- `text.write` has a generic description in the registry, but its current implementation is file-only.
- `application.launch` success does not verify target readiness; `application.close` success does not verify process exit.
- `keyboard.type` and `keyboard.press` are global input events, so focus must be handled and verified carefully.
- Keep registry schema, engine validation, tests, and this document synchronized when any primitive changes.

### 15. `browser.navigate`

Navigates the Branch 3 controlled browser to a validated absolute HTTP(S) URL. Only `http` and `https` schemes are accepted; embedded URL credentials, whitespace/control characters, and non-web schemes are rejected. The operation waits for a DOM readiness condition before succeeding.

Parameters:

- `url` (required): absolute HTTP(S) URL.
- `ready_condition` (required): `domcontentloaded`, `load`, `networkidle`, or `selector`.
- `selector` (optional): CSS selector to wait for visibility; required when `ready_condition` is `selector`.
- `timeout_ms` (optional): integer from 1000 to 120000; default 30000.

### 16. `browser.wait_for`

Waits for a DOM readiness condition on the current controlled browser page without navigating. It accepts `ready_condition`, optional `selector`, and optional `timeout_ms` with the same constraints as `browser.navigate`.

#### Runtime dependency and behavior

Branch 3 starts `python/browser_agent.py` on loopback only (`127.0.0.1:47621`). It uses Playwright's installed browser channel (`msedge` or `chrome`) and a dedicated persistent Nova profile under `%LOCALAPPDATA%\Nova\BrowserProfile`. The profile keeps site authentication separate from the user's everyday browser profile. Install the dependency in the same Python environment visible on PATH to Nova:

```powershell
python -m pip install playwright
```

The helper does not install browser binaries; it uses the installed Edge or Chrome channel. A readiness timeout fails the primitive and stops subsequent Branch 3 steps rather than continuing blindly. This browser agent is currently for Branch 3; Branch 2 code paths are unchanged.

## State-aware browser primitives (Branch 3)

- `browser.observe`: returns the controlled browser URL, title, page-kind hint, and bounded visible interactive elements and link destinations with observed IDs, roles, and accessible text. IDs are transient and must be refreshed after navigation or UI changes.
- `browser.click`: clicks an element by an ID from the most recent observation or by an exact accessible role and name. Ambiguous role/name matches fail rather than guessing.
- `browser.type`: fills an identified input by the same ID or exact role/name matching. Text is bounded by the browser agent.

These primitives operate in the explicitly connected browser session. The default is a labeled isolated profile; optional loopback CDP attachment and observed tab selection can reuse a configured user session. Ordinary unrelated browser windows are not automatically controllable. See runtime-configuration.md.


## Environment, sessions and download effects

- `environment.observe`: bounded title-free visible application inventory, foreground application, locale/region and known-folder paths. It observes, not launches.
- `browser.tabs`: optional local `target` filter; returns observed IDs and session mode. Unfiltered results omit full URL/title.
- `browser.select_tab`: requires a returned tab ID in `target`; returns a fresh observation of the selected tab.
- `browser.download`: observed element ID or exact role/name, optional authorized existing `location` and bounded timeout. Captures transfer completion, refuses overwrite, verifies the file and returns exact path/bytes. It never executes the file automatically.

Ordinary browser clicks refuse declared download controls and cancel unexpected transfers. Browser state includes readable text outside editable controls, dialogs, link destinations, popup and download effects. A successful click dispatch alone cannot verify a submitted message or completed transfer. Copy verifies source/destination bytes; move/rename verify their resulting paths, allowing their effects to satisfy matching artifact postconditions.
