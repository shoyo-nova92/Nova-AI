from core.error_handler import NovaErrorHandler

try:
    import easyocr
except ImportError:
    easyocr = None


class OCRReader:

    def __init__(self):

        self.reader = None
        self.init_error = None

        if easyocr is None:

            self.init_error = (
                "easyocr is not installed"
            )

            return

        try:

            self.reader = easyocr.Reader(
                ['en'],
                gpu=True
            )

        except Exception as e:

            try:

                self.reader = easyocr.Reader(
                    ['en'],
                    gpu=False
                )

            except Exception:

                self.init_error = str(e)
    
    def read_text(self, image_path):

        if self.reader is None:

            return {

                "status":
                    "unavailable",

                "text":
                    [],

                "reason":
                    self.init_error

            }

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

        if self.reader is None:

            return {

                "status":
                    "unavailable",

                "elements":
                    [],

                "reason":
                    self.init_error

            }

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
