import tempfile
from pathlib import Path

from core.filesystem_handler import FilesystemHandler

handler = FilesystemHandler()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "parser.py"
    file_path.write_text("print('Hello')\nprint('World')\n", encoding="utf-8")

    result = handler.replace_text(str(file_path), "print(", "logger.info(")

    assert result["success"] is True, result
    assert result["occurrences"] == 2, result
    assert result["replaced"] == 2, result
    assert Path(result["backup_path"]).exists(), result
    assert "logger.info('Hello')" in file_path.read_text(encoding="utf-8"), result

print("replace text ok")
