import os


class PDFRenamerPlugin:

    name = "pdf_renamer"

    def run(self, folder_path):
        files = os.listdir(folder_path)

        renamed = []

        for file in files:
            if file.endswith(".pdf"):
                old_path = os.path.join(folder_path, file)

                new_name = f"document_{len(renamed)+1}.pdf"
                new_path = os.path.join(folder_path, new_name)

                os.rename(old_path, new_path)

                renamed.append(new_name)

        return renamed