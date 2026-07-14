import tempfile
from pathlib import Path

from core.filesystem_handler import FilesystemHandler

handler = FilesystemHandler()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "parser.py"
    file_path.write_text("original\n", encoding="utf-8")
    handler.modify_file(str(file_path), "changed\n")

    result = handler.rollback_file(str(file_path))

    assert result["success"] is True, result
    assert file_path.read_text(encoding="utf-8") == "original\n", result
    assert Path(result["path"]).exists(), result

print("rollback file ok")
