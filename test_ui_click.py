from core.ocr_reader import OCRReader
from core.ui_executor import UIExecutor

ocr = OCRReader()
ui = UIExecutor()

image_path = "screenshot_20260502_233336.png"

result = ocr.read_text_with_positions(image_path)

elements = result["elements"]

ui.click_element(
    elements,
    "new chat"
)