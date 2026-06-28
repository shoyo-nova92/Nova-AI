import os
import shutil
import subprocess


class FilesystemHandler:

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
