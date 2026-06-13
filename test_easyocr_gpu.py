import easyocr

print("\n=== EASYOCR TEST ===\n")

reader = easyocr.Reader(
    ["en"],
    gpu=True
)

print("\nReader initialized successfully.")