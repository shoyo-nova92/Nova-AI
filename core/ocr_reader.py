import easyocr
from core.error_handler import NovaErrorHandler

class OCRReader:

    def __init__(self):
        self.reader = easyocr.Reader(
            ['en'],
            gpu=False
        )
    
    def read_text(self, image_path):
        try:
            results = self.reader.readtext(image_path)

            extracted_text = []
            seen = set()

            junk_words = {
                "",
                "|",
                "-",
                "_",
                ".",
                ",",
                ":",
                ";",
                "0",
                "1",
                "2",
                "3"
            }

            for item in results:
                text = item[1].strip()

                # remove very short junk
                if len(text) < 3:
                    continue

                # remove duplicate text
                if text.lower() in seen:
                    continue

                # remove junk tokens
                if text in junk_words:
                    continue

                # remove weird OCR garbage
                if text.count("|") > 2:
                    continue

                if text.count("/") > 3:
                    continue

                seen.add(text.lower())
                extracted_text.append(text)

            return {
                "status": "success",
                "text": extracted_text[:30]
            }

        except Exception as e:
            return NovaErrorHandler.handle(
                e,
                "OCRReader"
            )

    def read_text_with_positions(self, image_path):
        try:
            results = self.reader.readtext(image_path)

            ui_elements = []

            for item in results:
                bbox = item[0]
                text = item[1]
                confidence = item[2]

                ui_elements.append({
                    "text": text,
                    "position": bbox,
                    "confidence": confidence
                })

            return {
                "status": "success",
                "elements": ui_elements
            }

        except Exception as e:
            return {
                "status": "error",
                "message": str(e)
            }