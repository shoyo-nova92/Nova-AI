# Nova Branch 3 — Canonical Primitive Catalog

This document describes the **16 primitives registered in `src/primitive_registry.cpp`** and their current implementation in `src/primitive_execution_engine.cpp`. The registry schemas are the machine-readable contract used by `PrimitiveMapper`; if this document and the code disagree, treat the code as authoritative and update this document.

## Execution contract

- The Goal Decomposer converts a user goal into ordered logical steps. It does not choose primitives.
- The Primitive Mapper receives the **entire live registry catalog and the complete decomposed plan in one LLM call**. It maps all steps without executing anything.
- The host validates the mapped result against the registry and only then starts execution.
- Branch 3 executes steps in plan order and operations within each step in listed order. It stops on the first failed operation.
- Branch 2's single-action pipeline remains separate and unchanged.
- Current filesystem targets are intentionally restricted to the Windows Desktop or `desktop/...` paths. The runtime does not support arbitrary absolute paths for filesystem primitives.

## Primitive reference

### 1. `filesystem.create`

- **Purpose:** Create one new directory directly on the Desktop.
- **Required parameters:** `name` (single directory name), `location` (must currently be `desktop`).
- **Outputs described by registry:** `path`, `created`.
- **Side effects:** Creates a real directory. Fails if it already exists.
- **Limitations:** Does not create nested parent directories; rejects names containing path separators, `.` or `..`.

### 2. `filesystem.create_file`

- **Purpose:** Create a new file in an existing supported directory.
- **Required:** `name`, `location` (`desktop` or a safe path such as `desktop/NovaWorkspace`).
- **Optional:** `content` (defaults to empty when omitted).
- **Outputs:** `path`, `created`.
- **Side effects:** Creates a real file and writes initial content. It will not overwrite an existing file.
- **Limitations:** Parent directory must already exist; file name must be a single path component.

### 3. `filesystem.delete`

- **Purpose:** Delete a file or directory.
- **Required:** `target` (safe Desktop-relative path such as `desktop/NovaWorkspace/file.txt`).
- **Optional:** `recursive` (defaults to `false`).
- **Outputs:** `deleted_path`, `deleted`.
- **Side effects:** Destructive and potentially irreversible. A non-empty directory is rejected unless recursive deletion is explicitly enabled.
- **Limitations:** Only supported Desktop-relative targets; traversal components are rejected.

### 4. `filesystem.move`

- **Purpose:** Move a file or directory.
- **Required:** `source`, `destination` (both safe Desktop-relative paths).
- **Outputs:** `path`, `moved`.
- **Side effects:** Changes the resource's location. Existing destinations are rejected.
- **Limitations:** Destination parent must exist; moving a directory into itself is rejected. The implementation uses filesystem rename semantics and may fail across volumes.

### 5. `filesystem.copy`

- **Purpose:** Copy a file or directory.
- **Required:** `source`, `destination` (safe Desktop-relative paths).
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

### 7. `application.launch`

- **Purpose:** Launch an allowlisted application, optionally opening an existing file or folder.
- **Required:** `application`.
- **Optional:** `target` (must be an existing supported Desktop-relative path).
- **Outputs:** `launched`, `application`.
- **Current allowlist:** Notepad, Calculator, Paint, Explorer, VS Code, Chrome, and Edge (including aliases implemented in `appExeFromName`).
- **Side effects:** Starts a real Windows process. Success means the launch request was accepted, not that the application finished loading or opened the target successfully.
- **Limitations:** Arbitrary executables are not allowed.

### 8. `application.close`

- **Purpose:** Request a window to close.
- **Required:** `application` (allowlisted application).
- **Optional:** `target` (window-title substring).
- **Outputs:** `closed`, `application`.
- **Side effects:** Sends `WM_CLOSE` to a matching visible window. It does not forcibly terminate the process.
- **Limitations:** Success means the close message was sent, not that the process has exited. Unsaved-work dialogs may remain open. The current implementation selects a matching visible window.

### 9. `application.focus`

- **Purpose:** Bring a matching visible window to the foreground.
- **Required:** `application` (allowlisted application).
- **Optional:** `target` (window-title substring).
- **Outputs:** `focused`, `application`.
- **Side effects:** Attempts to restore and foreground a real window.
- **Limitations:** Windows may deny foreground activation; in that case the primitive returns an execution error. The target must already have a visible window.

### 10. `text.write`

- **Purpose:** Replace the contents of an existing text file.
- **Required:** `target` (safe Desktop-relative file path), `content` (exact text).
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

- `browser.observe`: returns the controlled browser URL, title, page-kind hint, and up to 40 visible interactive elements with observed IDs, roles, and accessible text. IDs are transient and must be refreshed after navigation or UI changes.
- `browser.click`: clicks an element by an ID from the most recent observation or by an exact accessible role and name. Ambiguous role/name matches fail rather than guessing.
- `browser.type`: fills an identified input by the same ID or exact role/name matching. Text is bounded by the browser agent.

These primitives operate only in Nova's loopback-controlled Playwright browser profile. They do not inspect or control the user's unrelated everyday browser profile. Branch 3 observes before each planning decision and validates every proposed operation against the live primitive registry.
