from core.ocr_reader import OCRReader

ocr = OCRReader()

result = ocr.read_text_with_positions(
    "screenshot_20260502_233336.png"
)

for element in result["elements"][:20]:
    print(element)