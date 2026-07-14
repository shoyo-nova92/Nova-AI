import os
import shutil
import subprocess
from pathlib import Path


class FilesystemHandler:

    def read_file(
        self,
        target
    ):

        try:

            path = Path(target)

            if not path.exists():

                return {

                    "success": False,

                    "reason":
                        "file does not exist"

                }

            if not path.is_file():

                return {

                    "success": False,

                    "reason":
                        "target is not a file"

                }

            content = path.read_text(
                encoding="utf-8"
            )

            return {

                "success": True,

                "action":
                    f"read file {target}",

                "path":
                    str(path),

                "content":
                    content,

                "lines":
                    len(
                        content.splitlines()
                    )

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def create_file(
        self,
        path
    ):

        try:

            if not path:

                return {

                    "success": False,

                    "reason":
                        "path is required"

                }

            folder = os.path.dirname(
                path
            )

            if folder:

                os.makedirs(
                    folder,
                    exist_ok=True
                )

            if os.path.exists(path):

                return {

                    "success": True,

                    "action":
                        f"file already exists {path}"

                }

            with open(
                path,
                "x",
                encoding="utf-8"
            ):

                pass

            return {

                "success": True,

                "action":
                    f"create file {path}"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def modify_file(
        self,
        path,
        new_content=None
    ):

        try:

            if not path:

                return {

                    "success": False,

                    "reason":
                        "path is required"

                }

            file_path = Path(path)

            if not file_path.exists():

                return {

                    "success": False,

                    "reason":
                        "file does not exist"

                }

            if not file_path.is_file():

                return {

                    "success": False,

                    "reason":
                        "target is not a file"

                }

            original_content = file_path.read_text(
                encoding="utf-8"
            )

            if new_content is None:

                new_content = ""

            backup_path = Path(f"{path}.bak")
            backup_path.write_text(
                original_content,
                encoding="utf-8"
            )

            file_path.write_text(
                new_content,
                encoding="utf-8"
            )

            verification_content = file_path.read_text(
                encoding="utf-8"
            )

            if verification_content != new_content:

                return {

                    "success": False,

                    "reason":
                        "write verification failed"

                }

            return {

                "success": True,

                "action":
                    f"modify file {path}",

                "path":
                    str(file_path),

                "backup_path":
                    str(backup_path),

                "original_content":
                    original_content,

                "new_content":
                    new_content

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def replace_text(
        self,
        path,
        old_text,
        new_text
    ):

        try:

            if not path:

                return {

                    "success": False,

                    "reason": "path is required"
                }

            file_path = Path(path)

            if not file_path.exists():

                return {

                    "success": False,

                    "reason": "file does not exist"
                }

            if not file_path.is_file():

                return {

                    "success": False,

                    "reason": "target is not a file"
                }

            if old_text is None:

                return {

                    "success": False,

                    "reason": "old_text is required"
                }

            original_content = file_path.read_text(
                encoding="utf-8"
            )

            if old_text not in original_content:

                return {

                    "success": False,

                    "reason": "old_text not found"
                }

            updated_content = original_content.replace(
                old_text,
                new_text
            )

            modify_result = self.modify_file(
                str(file_path),
                updated_content
            )

            if not modify_result.get("success"):

                return modify_result

            occurrences = original_content.count(old_text)
            replaced = occurrences

            return {

                "success": True,

                "action": "replace_text",

                "path": str(file_path),
                "backup_path": modify_result.get("backup_path"),
                "occurrences": occurrences,
                "replaced": replaced,
                "new_content": updated_content
            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)
            }

    def append_file(
        self,
        path,
        content
    ):

        try:

            if not path:

                return {

                    "success": False,

                    "reason": "path is required"
                }

            file_path = Path(path)

            if not file_path.exists():

                return {

                    "success": False,

                    "reason": "file does not exist"
                }

            if not file_path.is_file():

                return {

                    "success": False,

                    "reason": "target is not a file"
                }

            current_content = file_path.read_text(
                encoding="utf-8"
            )

            updated_content = current_content + content

            modify_result = self.modify_file(
                str(file_path),
                updated_content
            )

            if not modify_result.get("success"):

                return modify_result

            return {

                "success": True,

                "action": "append_file",

                "path": str(file_path),
                "backup_path": modify_result.get("backup_path"),
                "content": content
            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)
            }

    def insert_at_line(
        self,
        path,
        line_number,
        content
    ):

        try:

            if not path:

                return {

                    "success": False,

                    "reason": "path is required"
                }

            file_path = Path(path)

            if not file_path.exists():

                return {

                    "success": False,

                    "reason": "file does not exist"
                }

            if not file_path.is_file():

                return {

                    "success": False,

                    "reason": "target is not a file"
                }

            if line_number is None:

                return {

                    "success": False,

                    "reason": "line_number is required"
                }

            current_content = file_path.read_text(
                encoding="utf-8"
            )
            lines = current_content.splitlines()

            try:

                line_index = int(line_number) - 1

            except (ValueError, TypeError):

                return {

                    "success": False,

                    "reason": "line_number must be an integer"
                }

            if line_index < 0:

                return {

                    "success": False,

                    "reason": "line_number must be positive"
                }

            if line_index > len(lines):

                line_index = len(lines)

            new_lines = lines[:line_index] + [content] + lines[line_index:]
            updated_content = "\n".join(new_lines)

            if updated_content and not updated_content.endswith("\n"):

                updated_content += "\n"

            modify_result = self.modify_file(
                str(file_path),
                updated_content
            )

            if not modify_result.get("success"):

                return modify_result

            return {

                "success": True,

                "action": "insert_at_line",

                "path": str(file_path),
                "line": int(line_number),
                "inserted_lines": 1,
                "backup": modify_result.get("backup_path")
            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)
            }

    def rollback_file(
        self,
        path
    ):

        try:

            if not path:

                return {

                    "success": False,

                    "reason": "path is required"
                }

            file_path = Path(path)

            if not file_path.exists():

                return {

                    "success": False,

                    "reason": "file does not exist"
                }

            backup_path = Path(f"{path}.bak")

            if not backup_path.exists():

                return {

                    "success": False,

                    "reason": "backup does not exist"
                }

            restored_content = backup_path.read_text(
                encoding="utf-8"
            )
            file_path.write_text(
                restored_content,
                encoding="utf-8"
            )

            verify_content = file_path.read_text(
                encoding="utf-8"
            )

            return {

                "success": True,

                "action": "rollback_file",

                "path": str(file_path),
                "restored": verify_content == restored_content,
                "backup": str(backup_path)
            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)
            }

    def create_folder(self, path):

        try:

            os.makedirs(
                path,
                exist_ok=True
            )

            return {

                "success": True,

                "action":
                    f"create folder {path}"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def open_folder(self, path):

        try:

            subprocess.Popen(
                f'explorer "{path}"'
            )

            return {

                "success": True,

                "action":
                    f"open folder {path}"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def open_file(self, path):

        try:

            if not os.path.exists(path):

                return {

                    "success": False,

                    "reason":
                        "file not found"

                }

            os.startfile(path)

            return {

                "success": True,

                "action":
                    f"open file {path}"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def rename_folder(

        self,

        old_path,

        new_path

    ):

        try:

            os.rename(
                old_path,
                new_path
            )

            return {

                "success": True,

                "action":
                    "rename folder"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def delete_folder(

        self,

        path

    ):

        return {

            "success": False,

            "reason":
                "confirmation required"

        }

    def copy_file(

        self,

        source,

        destination

    ):

        try:

            shutil.copy2(
                source,
                destination
            )

            return {

                "success": True,

                "action":
                    "copy file"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def move_file(

        self,

        source,

        destination

    ):

        return { 

            "success": False,

            "reason":
                "confirmation required"

        }
