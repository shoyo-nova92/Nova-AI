import re


class TaskTranslator:

    def translate(
        self,
        step
    ):

        normalized_step = (
            step
            or
            ""
        ).lower()
        
        
        file_target = (
            self._extract_create_file_target(
                step
            )
        )

        if file_target:

            return {

                "type":
                    "filesystem",

                "action":
                    "create_file",

                "action_type":
                    "create_file",

                "target":
                    file_target

            }

        folder_target = (
            self._extract_create_folder_target(
                step
            )
        )

        if folder_target:

            return {

                "type":
                    "filesystem",

                "action":
                    "create_folder",

                "action_type":
                    "create_folder",

                "target":
                    folder_target

            }

        modify_target = (
            self._extract_modify_file_target(
                step
            )
        )

        if modify_target:

            return {

                "type":
                    "filesystem",

                "action":
                    "modify_file",

                "action_type":
                    "modify_file",

                "target":
                    modify_target

            }

        replace_target = self._extract_replace_text_target(step)

        if replace_target:

            return {

                "type":
                    "filesystem",

                "action":
                    "replace_text",

                "action_type":
                    "replace_text",

                "target":
                    replace_target["target"],

                "parameters": {

                    "old":
                        replace_target["old"],

                    "new":
                        replace_target["new"]

                }

            }

        append_target = self._extract_append_file_target(step)

        if append_target:

            return {

                "type":
                    "filesystem",

                "action":
                    "append_file",

                "action_type":
                    "append_file",

                "target":
                    append_target["target"],

                "parameters": {

                    "content":
                        append_target["content"]

                }

            }

        insert_target = self._extract_insert_line_target(step)

        if insert_target:

            return {

                "type":
                    "filesystem",

                "action":
                    "insert_at_line",

                "action_type":
                    "insert_at_line",

                "target":
                    insert_target["target"],

                "parameters": {

                    "line":
                        insert_target["line"],

                    "content":
                        insert_target["content"]

                }

            }

        rollback_target = self._extract_rollback_file_target(step)

        if rollback_target:

            return {

                "type":
                    "filesystem",

                "action":
                    "rollback_file",

                "action_type":
                    "rollback_file",

                "target":
                    rollback_target

            }

        read_target = (
            self._extract_read_file_target(
                step
            )
        )

        if read_target:

            return {

                "type":
                    "filesystem",

                "action":
                    "read_file",

                "action_type":
                    "read_file",

                "target":
                    read_target

            }

        vscode_open_phrases = [

            "open vscode",
            "open vs code",
            "open visual studio code",
            "launch vscode",
            "launch vs code",
            "launch visual studio code",
            "start vscode",
            "start vs code",
            "start visual studio code",
            "focus vscode",
            "focus vs code",
            "focus visual studio code"

        ]

        if any(
            phrase in normalized_step
            for phrase in vscode_open_phrases
        ):

            return {

                "type":
                    "application",

                "action":
                    "open_app",

                "action_type":
                    "open_app",

                "target":
                    "vscode"

            }

        if "git status" in normalized_step:

            return {

                "type":
                    "terminal",

                "action":
                    "git_status",

                "action_type":
                    "git_status",

                "target":
                    None

            }

        if re.search(r"\b(?:install|pip install)\b", normalized_step):

            package_match = re.search(
                r"(?:install|pip install)\s+([\w\-\.]+)",
                step,
                re.IGNORECASE
            )

            target = None
            if package_match:
                target = self._clean_target(package_match.group(1))

            return {

                "type": "terminal",

                "action": "pip_install",

                "action_type": "pip_install",

                "target": target

            }

        if re.search(r"\b(?:build|compile|run build)\b", normalized_step):

            return {

                "type": "terminal",

                "action": "build_project",

                "action_type": "build_project",

                "target": None

            }

        if (
            "pytest" in normalized_step
            or "tests" in normalized_step
            or "test project" in normalized_step
            or re.search(r"\b(?:run|execute|test)\s+test[_\w./\\-]+\.py\b", step, re.IGNORECASE)
        ):

            target = None

            file_pattern = r"[\w./\\-]+\.py"
            match = re.search(
                rf"(?:run|execute|test)\s+([\"']?{file_pattern}[\"']?)",
                step,
                re.IGNORECASE
            )

            if match:

                target = self._clean_target(match.group(1))

            return {

                "type": "terminal",

                "action": "run_pytest",

                "action_type": "run_pytest",

                "target": target

            }

        if any(
            phrase in normalized_step
            for phrase in ["run ", "execute "]
        ) and ".py" in normalized_step:

            file_pattern = (
                r"[\w./\\-]+"
                r"\."
                r"(?:py)"
            )

            match = re.search(
                rf"(?:run|execute)\s+[\"']?({file_pattern})[\"']?",
                step,
                re.IGNORECASE
            )

            if match:

                return {

                    "type":
                        "terminal",

                    "action":
                        "run_python",

                    "action_type":
                        "run_python",

                    "target":
                        self._clean_target(match.group(1))

                }

        if "terminal" in normalized_step:

            return {

                "type":
                    "terminal",

                "action":
                    "open_terminal",

                "action_type":
                    "open_terminal",

                "target":
                    None

            }

        return {

            "type":
                None,

            "action":
                None,

            "action_type":
                None,

            "target":
                None

        }

    def _extract_create_file_target(
        self,
        step
    ):

        if not step:

            return None

        normalized_step = step.lower()

        if (
            "create" not in normalized_step
            and not re.search(
                r"\badd\s+file\b",
                normalized_step
            )
        ):

            return None

        file_pattern = (
            r"[\w./\\-]+"
            r"\."
            r"(?:json|yaml|yml|tsx|jsx|toml|html|"
            r"css|txt|ini|py|ts|js|md)"
        )

        backtick_match = re.search(
            rf"`({file_pattern})`",
            step,
            re.IGNORECASE
        )

        if backtick_match:

            return self._clean_target(
                backtick_match.group(1)
            )

        create_match = re.search(
            rf"\b(?:create\s+(?:file\s+)?|add\s+file\s+)"
            rf"[\"']?({file_pattern})[\"']?",
            step,
            re.IGNORECASE
        )

        if create_match:

            return self._clean_target(
                create_match.group(1)
            )

        return None

    def _extract_create_folder_target(
        self,
        step
    ):

        if not step:

            return None

        folder_match = re.search(
            r"\b(?:create|add)\b\s+"
            r"(?:folder|directory)\s+"
            r"[\"']?([\w./\\-]+)[\"']?",
            step,
            re.IGNORECASE
        )

        if folder_match:

            return self._clean_target(
                folder_match.group(1)
            )

        return None

    def _clean_target(
        self,
        target
    ):

        return (
            target
            .strip()
            .strip("`'\".,:;")
            .replace("\\", "/")
        )

    def _extract_modify_file_target(
        self,
        step
    ):

        if not step:

            return None

        file_pattern = (
            r"[\w./\\-]+"
            r"\."
            r"(?:json|yaml|yml|tsx|jsx|toml|html|"
            r"css|txt|ini|py|ts|js|md)"
        )

        direct_match = re.search(
            rf"\b(?:update|modify|edit)\b\s+"
            rf"[\"']?({file_pattern})[\"']?",
            step,
            re.IGNORECASE
        )

        if direct_match:

            return self._clean_target(
                direct_match.group(1)
            )

        add_to_match = re.search(
            rf"\badd\b\s+.+?\s+(?:to|in)\s+"
            rf"[\"']?({file_pattern})[\"']?",
            step,
            re.IGNORECASE
        )

        if add_to_match:

            return self._clean_target(
                add_to_match.group(1)
            )

        return None

    def _extract_replace_text_target(
        self,
        step
    ):

        if not step:

            return None

        normalized_step = step.lower()

        if "replace" not in normalized_step:

            return None

        match = re.search(
            r"\breplace\s+([^\s]+)\s+with\s+([^\s]+)",
            step,
            re.IGNORECASE
        )

        if not match:

            return None

        old_text = match.group(1)
        new_text = match.group(2)

        target = "parser.py"

        if " in " in step.lower():

            target_match = re.search(
                r"in\s+([\w./\\-]+\.[\w]+)",
                step,
                re.IGNORECASE
            )

            if target_match:

                target = self._clean_target(target_match.group(1))

        return {

            "target": target,

            "old": old_text,

            "new": new_text
        }

    def _extract_append_file_target(
        self,
        step
    ):

        if not step:

            return None

        normalized_step = step.lower()

        if "append" not in normalized_step:

            return None

        target = "notes.txt"

        if " to " in normalized_step:

            target_match = re.search(
                r"to\s+([\w./\\-]+\.[\w]+)",
                step,
                re.IGNORECASE
            )

            if target_match:

                target = self._clean_target(target_match.group(1))

        content_match = re.search(
            r"\bappend\b\s+(.+?)\s+to\s+",
            step,
            re.IGNORECASE
        )

        if not content_match:

            return None

        return {

            "target": target,

            "content": content_match.group(1).strip().strip("`'\".")
        }

    def _extract_insert_line_target(
        self,
        step
    ):

        if not step:

            return None

        normalized_step = step.lower()

        if "insert" not in normalized_step:

            return None

        target = "parser.py"
        line_number = None
        content = None

        line_match = re.search(
            r"line\s+(\d+)",
            step,
            re.IGNORECASE
        )

        if line_match:

            line_number = int(line_match.group(1))

        if " in " in normalized_step:

            target_match = re.search(
                r"in\s+([\w./\\-]+\.[\w]+)",
                step,
                re.IGNORECASE
            )

            if target_match:

                target = self._clean_target(target_match.group(1))

        content_match = re.search(
            r"insert\s+(.+?)\s+at\s+line",
            step,
            re.IGNORECASE
        )

        if content_match:

            content = content_match.group(1).strip().strip("`'\"")

        if line_number is None or content is None:

            return None

        return {

            "target": target,

            "line": line_number,

            "content": content
        }

    def _extract_rollback_file_target(
        self,
        step
    ):

        if not step:

            return None

        normalized_step = step.lower()

        if not any(
            phrase in normalized_step
            for phrase in [
                "rollback",
                "restore",
                "undo",
                "recover"
            ]
        ):

            return None

        file_pattern = (
            r"[\w./\\-]+"
            r"\."
            r"(?:json|yaml|yml|tsx|jsx|toml|html|"
            r"css|txt|ini|py|ts|js|md)"
        )

        match = re.search(
            rf"(?:rollback|restore|undo|recover)\s+(?:file\s+)?[\"']?({file_pattern})[\"']?",
            step,
            re.IGNORECASE
        )

        if match:

            return self._clean_target(match.group(1))

        return None

    def _extract_read_file_target(
        self,
        step
    ):

        if not step:

            return None

        normalized_step = step.lower()

        if "read" not in normalized_step:

            return None

        file_pattern = (
            r"[\w./\\-]+"
            r"\."
            r"(?:json|yaml|yml|tsx|jsx|toml|html|"
            r"css|txt|ini|py|ts|js|md)"
        )

        backtick_match = re.search(
            rf"`({file_pattern})`",
            step,
            re.IGNORECASE
        )

        if backtick_match:

            return self._clean_target(
                backtick_match.group(1)
            )

        read_match = re.search(
            rf"\bread\b\s+"
            rf"(?:file\s+)?"
            rf"[\"']?({file_pattern})[\"']?",
            step,
            re.IGNORECASE
        )

        if read_match:

            return self._clean_target(
                read_match.group(1)
            )

        return None
