from core.ocr_reader import OCRReader

ocr = OCRReader()

result = ocr.read_text("screenshot_20260502_231001.png")

print(result)