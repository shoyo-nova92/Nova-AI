import os
import tempfile
from pathlib import Path

from core.filesystem_handler import FilesystemHandler

handler = FilesystemHandler()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "sample.txt"
    file_path.write_text("old content\n", encoding="utf-8")

    result = handler.modify_file(str(file_path), "new content\n")

    assert result["success"] is True, result
    assert result["backup_path"].endswith(".bak"), result
    assert Path(result["backup_path"]).exists(), result
    assert file_path.read_text(encoding="utf-8") == "new content\n", result
    assert Path(result["backup_path"]).read_text(encoding="utf-8") == "old content\n", result

    missing_result = handler.modify_file(str(Path(tmp_dir) / "missing.txt"), "noop")
    assert missing_result["success"] is False, missing_result

print("modify file ok")
