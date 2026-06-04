class IntentParser:

    def parse(self, command):
        text = command.lower().strip()

        #How App is Opening
        open_words = ["open", "launch", "start", "run"]

        for word in open_words:
            if word in text:

                if "chrome" in text or "browser" in text:
                    return {
                        "intent": "open_app",
                        "target": "chrome"
                    }
                elif "notepad" in text:
                    return {
                        "intent": "open_app",
                        "target": "notepad"
                    }
                elif "calculator" in text:
                    return {
                        "intent": "open_app",
                        "target": "calculator"
                    }
                elif "paint" in text:
                    return {
                        "intent": "open_app",
                        "target": "paint"
                    }

        #How to close everything

        if "close everything" in text:
            return {
                "intent": "close_app",
                "target": "all"
            }
        
        #How App is Closing
        if "close" in text:

                if "chrome" in text or "browser" in text:
                    return {
                        "intent": "close_app",
                        "target": "chrome"
                    }
                elif "notepad" in text:
                    return {
                        "intent": "close_app",
                        "target": "notepad"
                    }
                elif "calculator" in text:
                    return {
                        "intent": "close_app",
                        "target": "calculator"
                    }
                elif "paint" in text:
                    return {
                        "intent": "close_app",
                        "target": "paint"
                    }

        #How App is Searching

        if "search for" in text or "google" in text:

            query = text.replace("search", "").strip()

            return {
                "intent": "search_web", 
                "target": query
            }

        #How App is Catering Unknown Commands

        return {
            "intent": "unknown",
            "target": command   
        }