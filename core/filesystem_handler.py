import os
import shutil
import subprocess


class FilesystemHandler:

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