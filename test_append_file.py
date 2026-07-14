import tempfile
from pathlib import Path

from core.filesystem_handler import FilesystemHandler

handler = FilesystemHandler()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "notes.txt"
    file_path.write_text("first\n", encoding="utf-8")

    result = handler.append_file(str(file_path), "second\n")

    assert result["success"] is True, result
    assert file_path.read_text(encoding="utf-8") == "first\nsecond\n", result
    assert Path(result["backup_path"]).exists(), result

print("append file ok")
