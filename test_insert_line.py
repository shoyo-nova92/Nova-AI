import tempfile
from pathlib import Path

from core.filesystem_handler import FilesystemHandler

handler = FilesystemHandler()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "parser.py"
    file_path.write_text("print('hello')\n", encoding="utf-8")

    result = handler.insert_at_line(str(file_path), 1, "import logging\n")

    assert result["success"] is True, result
    assert result["line"] == 1, result
    assert result["inserted_lines"] == 1, result
    assert Path(result["backup"]).exists(), result
    assert "import logging" in file_path.read_text(encoding="utf-8"), result

print("insert line ok")
