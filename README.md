<div align="center">

# NOVA

**A Windows desktop assistant for natural-language tasks.**

C++20 runtime · Python browser and voice helpers · OpenRouter

[Setup](#setup) · [Requirements](#requirements) · [Voice input](#optional-voice-input) · [Configuration](#configuration) · [Troubleshooting](#troubleshooting)

</div>

---

NOVA turns typed or spoken requests into desktop actions. It can work with local files, launch and control applications, and interact with websites through a dedicated browser session. An OpenRouter language model interprets requests and plans actions; the Windows runtime executes them and reports the result.

## What you can do

- Create, find, move, and edit local files and folders.
- Launch applications and perform keyboard and mouse actions.
- Navigate and interact with websites in Microsoft Edge or Google Chrome.
- Enter requests in the console, or use the included **“Hey Nova”** wakeword model with optional GPU speech recognition.
- Enable spoken replies, read shared Notion pages, or configure Twilio voice calls.

NOVA is a source-built Windows application. Model availability, website behavior, and local permissions affect which requests succeed. Review actions before using it with important files or signed-in services.

## Requirements

For the initial **text-input setup**:

- **64-bit Windows 10 or Windows 11.** Linux and macOS are not supported by this runtime.
- **Git for Windows** and an internet connection.
- **MSYS2 UCRT64**, with GCC supporting C++20, CMake 3.20+, Ninja, libcurl, and nlohmann/json. Installation commands are below.
- **64-bit CPython 3.13**, the Python version used by the current environment. Use the Windows Python installer, rather than MSYS2 Python, for `.venv-win`.
- **An OpenRouter API key**, with access to the model you choose. Provider limits and charges depend on your account and model.
- **Microsoft Edge or Google Chrome**, installed normally, for browser tasks.

Voice input additionally requires a microphone, an NVIDIA GPU with enough available VRAM for Whisper **large-v3**, a compatible NVIDIA driver, and the CUDA 12/cuDNN 9 runtime libraries listed in the voice requirements. The current voice helper uses CUDA and float16; it has no CPU fallback. Text input does not require a GPU or the voice dependencies.

## Setup

### 1. Install the build tools

Install [Git for Windows](https://git-scm.com/downloads/win), [Python for Windows](https://www.python.org/downloads/windows/), and [MSYS2](https://www.msys2.org/).

Open **MSYS2 UCRT64** from the Start menu. Update its packages:

```bash
pacman -Syu
```

If the update closes the terminal, reopen **MSYS2 UCRT64** and run the update again. Then install the native Windows build dependencies:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-curl mingw-w64-ucrt-x86_64-nlohmann-json
```

Use packages from the same UCRT64 environment. See the [MSYS2 CMake guide](https://www.msys2.org/docs/cmake/) for details.

### 2. Clone NOVA and install Python requirements

Open **PowerShell**. All remaining commands run there, from the repository root:

```powershell
git clone https://github.com/shoyo-nova92/Nova-AI.git
Set-Location Nova-AI
py -3.13 -m venv .venv-win
.\.venv-win\Scripts\python.exe -m pip install --upgrade pip
.\.venv-win\Scripts\python.exe -m pip install -r python\requirements.txt
```

The `.venv-win` name is required by the runtime. Environment activation is unnecessary because the commands use its Python executable directly. The core requirements install Playwright and timezone data. Browser automation uses your installed Edge or Chrome; a separate Playwright Chromium download is unnecessary.

### 3. Configure your credentials

For a fresh clone, copy the examples:

```powershell
Copy-Item .env.example .env
Copy-Item config\personal_sources.example.json config\personal_sources.json
notepad .env
```

Set your own key in `.env`:

```dotenv
OPENROUTER_API_KEY=your_openrouter_api_key
NOVA_TTS_ENABLED=0
NOVA_CALLS_ENABLED=0
```

The key shown above is a placeholder. Get your actual key from [OpenRouter](https://openrouter.ai/settings/keys). If needed, set `NOVA_MODEL` to an available model ID from the [OpenRouter model catalog](https://openrouter.ai/models). The built-in default is defined in `src/llm_client.cpp`; access to free models is subject to provider availability and rate limits.

Keep existing configuration when updating an installation. Both `.env` and `config/personal_sources.json` are ignored by Git. Environment variables already set in your shell take precedence over `.env` values.

### 4. Build

Add the UCRT64 tools to the current PowerShell session. Adjust the path if you installed MSYS2 elsewhere:

```powershell
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Nova --parallel 4
```

Keep this tool directory on `PATH` when running NOVA: the executable uses DLLs supplied by MSYS2. Building produces `build\Nova.exe`.

### 5. Run with text input

Launch from the **repository root**, so Python helpers and configuration paths resolve correctly:

```powershell
.\build\Nova.exe --text
```

Start with a small request such as `Create a folder named NovaDemo on my desktop`. Read the console output and any clarification prompts. Type `q` or `quit` at the request prompt to exit.

## Optional voice input

Complete the text setup first, then install the voice dependencies in the same environment:

```powershell
.\.venv-win\Scripts\python.exe -m pip install -r python\requirements-voice.txt
.\.venv-win\Scripts\python.exe -c "from openwakeword.utils import download_models; download_models()"
```

The second command downloads openWakeWord's supporting models. NOVA's custom wakeword classifier is already included at `models/wakeword/hey_nova.onnx`. Whisper downloads the large-v3 speech-recognition weights on first use; allow extra disk space and time for that download. A compatible NVIDIA driver must be installed separately. The voice requirements supply CUDA runtime DLLs in the virtual environment; they do not install a graphics driver.

Enable microphone access in Windows settings, select the intended default input device, and run:

```powershell
.\build\Nova.exe
```

Say **“Hey Nova”**, wait for the listening prompt, then speak your request. Press **V** while waiting for voice input to switch to typing, or **Q** to quit. Wakeword detection and transcription run locally; the resulting request is sent to the configured language-model provider for planning.

For speech-model compatibility details, see [faster-whisper](https://github.com/SYSTRAN/faster-whisper#gpu) and [openWakeWord](https://github.com/dscripka/openWakeWord). Wakeword limitations and model attribution are documented in [models/wakeword/README.md](models/wakeword/README.md).

## Configuration

Use `.env.example` as the starting point. Restart NOVA after changing configuration.

- `OPENROUTER_API_KEY`: required for language-model requests and cloud speech output.
- `NOVA_MODEL`: optional OpenRouter model ID override. `NOVA_FALLBACK_MODEL` selects an optional fallback.
- `NOVA_TTS_ENABLED`: set to `1` for spoken replies or `0` for silence. Spoken output is separate from voice input.
- `NOVA_TTS_MODEL` and `NOVA_TTS_VOICE`: optional cloud speech overrides. The current defaults are in `src/speech_output.hpp`; Windows speech is the default fallback when cloud synthesis fails.
- `NOVA_TIMEZONE`: optional IANA timezone, such as `Europe/London`; overrides the timezone in your local sources file.
- `NOTION_API_KEY`: optional Notion integration token. Share the desired pages with that integration, then add their links to your local sources file. Notion access is read-only.
- `NOVA_CALLS_ENABLED`: defaults to disabled. Calling also requires `TWILIO_ACCOUNT_SID`, `TWILIO_AUTH_TOKEN`, `TWILIO_FROM_NUMBER`, and your own verified `NOVA_PHONE_TO` destination in international E.164 format.

The sources example starts with UTC and an empty list. Edit your local `config/personal_sources.json` to choose your timezone and optional sources:

```json
{
  "timezone": "Europe/London",
  "sources": [
    {
      "url": "YOUR_SHARED_NOTION_PAGE_URL",
      "user_description": "Tasks and deadlines"
    }
  ]
}
```

Leave `sources` empty if you do not use Notion. Optional integrations are unnecessary for ordinary desktop and browser tasks.

## Privacy and local data

Your requests and task context needed for planning are sent to OpenRouter and the selected model provider. Browser observations and file contents used by a task can become part of that context. Enabling cloud speech output also sends reply text to the speech provider. Optional integrations contact their respective services.

NOVA writes runtime logs under `logs/` and recordings or generated audio under `audio_output/`. These can contain personal content and are ignored by Git. Browser automation normally uses a separate persistent profile under `%LOCALAPPDATA%\Nova\BrowserProfile`; sign in there only when a task needs it. Keep credentials, logs, recordings, and personal source links out of commits and issue attachments.

## Troubleshooting

- **CMake, GCC, or a dependency is missing:** install the UCRT64 packages above and add `C:\msys64\ucrt64\bin` to the current PowerShell `PATH`. Avoid mixing MSYS, MINGW64, and UCRT64 libraries.
- **CMake reports a different generator or old paths:** use a new build directory, for example `cmake -S . -B build-fresh -G Ninja -DCMAKE_BUILD_TYPE=Release`, then build and run from that directory.
- **A DLL is missing when NOVA starts:** run from a PowerShell session with the UCRT64 `bin` directory on `PATH`.
- **The API key is missing or requests fail:** check `.env` in the project root, model access, account quota, and network connectivity. Check whether a shell environment variable overrides your file.
- **The Python helper cannot start:** keep the virtual environment named `.venv-win` and launch NOVA from the repository root.
- **Browser launch fails:** install Edge or Chrome and ensure Playwright is installed using `.venv-win\Scripts\python.exe`.
- **Voice model files are missing:** run the openWakeWord download command above. Keep the bundled wakeword model and JSON settings together.
- **CUDA, cuDNN, or GPU memory errors:** check the NVIDIA driver and voice dependency installation, close other GPU-heavy programs, or run `Nova.exe --text`.
- **Date/time lookup fails:** install the core requirements and set a valid IANA timezone in the local configuration.

## Project layout

```text
src/                 Windows C++ runtime and action execution
python/              Browser, clock, and voice helpers; dependency lists
models/wakeword/     Hey Nova classifier, settings, and attribution
config/              Safe configuration example
.env.example         Credential and integration template
CMakeLists.txt       Application build definition
```

## License and third-party assets

This repository currently has no project-wide license file. Public source availability alone does not grant a general redistribution license. Dependencies and bundled model assets have their own terms; see the [wakeword attribution](models/wakeword/README.md#sources-and-attribution) before redistributing or using those assets commercially.
