import tempfile
from pathlib import Path

from core.filesystem_handler import FilesystemHandler

handler = FilesystemHandler()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "parser.py"
    file_path.write_text("original\n", encoding="utf-8")

    modify_result = handler.modify_file(str(file_path), "changed\n")
    assert modify_result["success"] is True, modify_result

    rollback_result = handler.rollback_file(str(file_path))
    assert rollback_result["success"] is True, rollback_result
    assert file_path.read_text(encoding="utf-8") == "original\n", rollback_result

print("failed modify ok")
