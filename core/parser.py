class IntentParser:

    def parse(self, command):
        text = command.lower().strip()

        actions = []

        # ---------------- OPEN APP ----------------

        open_words = ["open", "launch", "start", "run"]

        if any(word in text for word in open_words):

            if "chrome" in text or "browser" in text:
                actions.append({
                    "intent": "open_app",
                    "target": "chrome"
                })

            elif "notepad" in text:
                actions.append({
                    "intent": "open_app",
                    "target": "notepad"
                })

            elif "calculator" in text:
                actions.append({
                    "intent": "open_app",
                    "target": "calculator"
                })

            elif "paint" in text:
                actions.append({
                    "intent": "open_app",
                    "target": "paint"
                })

        # ---------------- SEARCH ----------------

        if "search for" in text:

            query = text.split("search for", 1)[1].strip()

            actions.append({
                "intent": "search_web",
                "target": query
            })

        elif text.startswith("google "):

            query = text.replace("google", "", 1).strip()

            actions.append({
                "intent": "search_web",
                "target": query
            })

        # ---------------- CLOSE EVERYTHING ----------------

        if "close everything" in text:

            actions.append({
                "intent": "close_all",
                "target": ""
            })

        # ---------------- CLOSE APP ----------------

        elif "close" in text:

            if "chrome" in text:
                actions.append({
                    "intent": "close_app",
                    "target": "chrome"
                })

            elif "notepad" in text:
                actions.append({
                    "intent": "close_app",
                    "target": "notepad"
                })

            elif "calculator" in text:
                actions.append({
                    "intent": "close_app",
                    "target": "calculator"
                })

            elif "paint" in text:
                actions.append({
                    "intent": "close_app",
                    "target": "paint"
                })

        if not actions:
            actions.append({
                "intent": "unknown",
                "target": command
            })

        return actions