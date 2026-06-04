import subprocess
import webbrowser
import os


class Executor:

    def execute(self, result):
        intent = result["intent"]
        target = result["target"]

        # OPEN APPLICATIONS
        if intent == "open_app":
            return self.open_app(target)

        # SEARCH WEB
        elif intent == "search_web":
            webbrowser.open(f"https://www.google.com/search?q={target}")
            return f"Searching {target}"

        # CLOSE APPLICATIONS
        elif intent == "close_app":
            return self.close_app(target)

        # CLOSE EVERYTHING
        elif intent == "close_all":
            os.system("taskkill /f /im chrome.exe")
            os.system("taskkill /f /im notepad.exe")
            os.system("taskkill /f /im msedge.exe")
            return "Closing everything"

        return "Command not understood"

    # ---------------- OPEN APP ----------------
    def open_app(self, target):

        apps = {
            "notepad": "notepad.exe",
            "calculator": "calc.exe",
            "paint": "mspaint.exe",
            "cmd": "cmd.exe",
            "chrome": r"C:\Program Files\Google\Chrome\Application\chrome.exe",
            "edge": r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
            "photoshop": r"C:\Program Files\Adobe\Adobe Photoshop 2025\Photoshop.exe",
            "spotify": r"C:\Users\shour\AppData\Roaming\Spotify\Spotify.exe",
            "vlc": r"C:\Program Files\VideoLAN\VLC\vlc.exe",
            "word": r"C:\Program Files\Microsoft Office\root\Office16\WINWORD.exe",
            "excel": r"C:\Program Files\Microsoft Office\root\Office16\EXCEL.exe",
            "powerpoint": r"C:\Program Files\Microsoft Office\root\Office16\POWERPNT.EXE",
            "discord": r"C:\Users\shour\AppData\Local\Discord\Application\Discord.exe",
            "visual studio": r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe",
            "adobe reader": r"C:\Program Files\Adobe\Acrobat Reader DC\Reader\AcroRd32.exe",
            "illustrator": r"C:\Program Files\Adobe\Adobe Illustrator 2023\Support Files\Contents\Windows\Illustrator.exe",
            "premiere": r"C:\Program Files\Adobe\Adobe Premiere Pro 2023\Adobe Premiere Pro.exe",
            "after effects": r"C:\Program Files\Adobe\Adobe After Effects 2023\Support Files\AfterFX.exe",
            "valorant": r"C:\Riot Games\VALORANT\live\VALORANT.exe"
        }

        if target in apps:
            try:
                subprocess.Popen(apps[target])
                return f"Opening {target}"
            except:
                return f"Failed to open {target}"

        return f"App '{target}' not found"

    # ---------------- CLOSE APP ----------------
    def close_app(self, target):

        process_map = {
            "notepad": "notepad.exe",
            "calculator": "CalculatorApp.exe",
            "paint": "mspaint.exe",
            "cmd": "cmd.exe",
            "chrome": "chrome.exe",
            "edge": "msedge.exe",
            "photoshop": "Photoshop.exe",
            "spotify": "Spotify.exe",
            "vlc": "vlc.exe",
            "word": "WINWORD.exe",
            "excel": "EXCEL.exe",
            "powerpoint": "POWERPNT.exe",
            "discord": "Discord.exe",
            "visual studio": "devenv.exe",
            "adobe reader": "AcroRd32.exe",
            "illustrator": "Illustrator.exe",
            "premiere": "Adobe Premiere Pro.exe",
            "after effects": "AfterFX.exe",
            "valorant": "VALORANT.exe"
        }

        if target in process_map:
            os.system(f'taskkill /f /im "{process_map[target]}"')
            return f"Closing {target}"

        return f"Cannot close {target}"