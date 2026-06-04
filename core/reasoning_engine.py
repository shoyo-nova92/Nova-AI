class ReasoningEngine:

    def analyze_context(self, vision_data):

        active_window = (
            vision_data["active_window"]["title"]
        ).lower()

        ocr_data = vision_data["visible_text"]

        visible_text = ""

        if (
            isinstance(ocr_data, dict)
            and "text" in ocr_data
        ):

            visible_text = " ".join(
                [str(x) for x in ocr_data["text"]]
            ).lower()

        combined_context = (
            active_window + " " + visible_text
        )

        # CODING CONTEXT
        coding_keywords = [

            "vscode",
            ".py",
            "python",
            "function",
            "class",
            "def",
            "import",
            "terminal",
            "debug",
            "code",
            "visionengine",
            "cognitiveloop",
            "reasoningengine"

        ]

        if any(
            keyword in combined_context
            for keyword in coding_keywords
        ):

            return {

                "current_activity": "coding",

                "suggested_actions": [

                    "continue coding",
                    "debug code",
                    "open terminal",
                    "search documentation",
                    "review architecture"

                ]

            }

        # PHOTOSHOP CONTEXT
        photoshop_keywords = [

            "photoshop",
            "layers",
            "brush",
            "export",
            "thumbnail",
            "canvas"

        ]

        if any(
            keyword in combined_context
            for keyword in photoshop_keywords
        ):

            return {

                "current_activity": "design_editing",

                "suggested_actions": [

                    "continue editing",
                    "export project",
                    "open references",
                    "organize assets"

                ]

            }

        # STUDY CONTEXT
        study_keywords = [

            "study",
            "course",
            "lecture",
            "exam",
            "notion",
            "university",
            "notes"

        ]

        if any(
            keyword in combined_context
            for keyword in study_keywords
        ):

            return {

                "current_activity": "studying",

                "suggested_actions": [

                    "start focus session",
                    "open notes",
                    "summarize material",
                    "create study plan"

                ]

            }

        # DEFAULT
        return {

            "current_activity": "unknown",

            "suggested_actions": [

                "observe further"

            ]

        }