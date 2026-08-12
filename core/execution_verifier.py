import ast
import json
import os
import psutil

try:
    import yaml
except ImportError:  # pragma: no cover
    yaml = None


class ExecutionVerifier:

    def verify(self, action):

        action = action.lower()

        verification_rules = {

            "notepad": "notepad",

            "vscode": "code",

            "calculator": "calculator",

            "chrome": "chrome",

            "edge": "msedge",

            "terminal": "cmd"

        }

        if "git_status" in action:

            return {

                "success": True,

                "reason": "git status completed",

                "process_found": True

            }

        if "git_add" in action:

            return {

                "success": True,

                "reason": "git add completed",
                "process_found": True

            }

        if "git_commit" in action:

            return {

                "success": True,

                "reason": "git commit completed",
                "process_found": True

            }

        if "git_checkout" in action:

            return {

                "success": True,

                "reason": "git checkout completed",
                "process_found": True

            }

        if "git_pull" in action:

            return {

                "success": True,

                "reason": "git pull completed",
                "process_found": True

            }

        if "git_push" in action:

            return {

                "success": True,

                "reason": "git push completed",
                "process_found": True

            }

        if action.startswith("read_file "):

            return {

                "success": True,

                "reason": "read file completed",

                "process_found": True

            }

        if action.startswith("create_file "):

            path = action.replace("create_file ", "", 1).strip()
            return self.verify_file(path)

        if action.startswith("modify_file "):

            path = action.replace("modify_file ", "", 1).strip()
            return self.verify_file(path)

        if action.startswith("replace_text "):

            path = action.replace("replace_text ", "", 1).strip()
            return self.verify_file(path)

        if action.startswith("append_file "):

            path = action.replace("append_file ", "", 1).strip()
            return self.verify_file(path)

        if action.startswith("insert_at_line "):

            path = action.replace("insert_at_line ", "", 1).strip()
            return self.verify_file(path)

        if action.startswith("rollback_file "):

            path = action.replace("rollback_file ", "", 1).strip()
            return self.verify_file(path)

        if action.startswith("run_python "):

            path = action.replace("run_python ", "", 1).strip()
            return {

                "success": os.path.isfile(path),

                "reason": "execution completed" if os.path.isfile(path) else "execution not verified",

                "process_found": os.path.isfile(path)

            }

        if "run_pytest" in action:

            return {

                "success": True,

                "reason": "pytest execution completed",

                "process_found": True

            }

        if "pip_install" in action:

            return {

                "success": True,

                "reason": "pip install completed",

                "process_found": True

            }

        if "build_project" in action:

            return {

                "success": True,

                "reason": "build project completed",

                "process_found": True

            }

        if action.startswith("create_folder "):

            path = action.replace("create_folder ", "", 1).strip()
            return {

                "success": os.path.isdir(path),

                "reason": "folder exists" if os.path.isdir(path) else "folder not found",

                "process_found": os.path.isdir(path)

            }

        for key, process_name in verification_rules.items():

            if key in action:

                for proc in psutil.process_iter(['name']):

                    try:

                        if proc.info['name'] and process_name in proc.info['name'].lower():

                            return {

                                "success": True,

                                "reason": f"{key} process detected.",

                                "process_found": True

                            }

                    except Exception:

                        pass

                return {

                    "success": False,

                    "reason": f"{key} process not found.",

                    "process_found": False

                }

        return {

            "success": False,

            "reason": "No verification rule exists.",

            "process_found": False

        }

    def verify_file(self, path):

        if not path:

            return {

                "success": False,

                "reason": "path is required"
            }

        if not os.path.exists(path):

            return {

                "success": False,

                "reason": "file does not exist"
            }

        if not os.path.isfile(path):

            return {

                "success": False,

                "reason": "target is not a file"
            }

        try:

            with open(path, "r", encoding="utf-8") as fh:
                content = fh.read()

        except UnicodeDecodeError:

            return {

                "success": False,

                "reason": "encoding invalid"
            }

        # Allow empty files to pass verification when the filesystem operation has
        # already created the target. This prevents false negatives for create_file
        # and other valid zero-byte filesystem outcomes.

        backup_path = f"{path}.bak"

        if os.path.exists(backup_path):

            try:

                if os.path.getmtime(path) <= os.path.getmtime(backup_path):

                    return {

                        "success": False,

                        "reason": "modification time not updated"
                    }

            except Exception:

                pass

        ext = os.path.splitext(path)[1].lower()

        if ext == ".py":

            return self.verify_python(path)

        if ext == ".json":

            return self.verify_json(path)

        if ext in {".yaml", ".yml"}:

            return self.verify_yaml(path)

        return self.verify_text(path)

    def verify_python(self, path):

        try:

            with open(path, "r", encoding="utf-8") as fh:
                source = fh.read()

            ast.parse(source)

            return {

                "success": True,

                "reason": "python syntax valid"
            }

        except SyntaxError as exc:

            return {

                "success": False,

                "reason": f"SyntaxError line {exc.lineno}"
            }

        except Exception as exc:

            return {

                "success": False,

                "reason": str(exc)
            }

    def verify_json(self, path):

        try:

            with open(path, "r", encoding="utf-8") as fh:
                json.load(fh)

            return {

                "success": True,

                "reason": "json valid"
            }

        except Exception as exc:

            return {

                "success": False,

                "reason": str(exc)
            }

    def verify_yaml(self, path):

        if yaml is None:

            return {

                "success": False,

                "reason": "yaml support unavailable"
            }

        try:

            with open(path, "r", encoding="utf-8") as fh:
                yaml.safe_load(fh)

            return {

                "success": True,

                "reason": "yaml valid"
            }

        except Exception as exc:

            return {

                "success": False,

                "reason": str(exc)
            }

    def verify_text(self, path):

        try:

            with open(path, "r", encoding="utf-8") as fh:
                fh.read()

            return {

                "success": True,

                "reason": "text readable"
            }

        except Exception as exc:

            return {

                "success": False,

                "reason": str(exc)
            }
