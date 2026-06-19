class UISemanticsEngine:

    def analyze(
        self,
        ocr_elements
    ):

        semantic_elements = []

        for element in ocr_elements:

            text = element["text"].lower()

            if "save" in text:

                semantic_elements.append({

                    "type": "button",

                    "label": element["text"],

                    "intent": "save_document"

                })

            elif "export" in text:

                semantic_elements.append({

                    "type": "button",

                    "label": element["text"],

                    "intent": "export_project"

                })

            elif "submit" in text:

                semantic_elements.append({

                    "type": "button",

                    "label": element["text"],

                    "intent": "submit_form"

                })

        return semantic_elements