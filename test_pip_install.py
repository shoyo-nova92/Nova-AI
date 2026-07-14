import tempfile
from pathlib import Path

from core.terminal_handler import TerminalHandler

handler = TerminalHandler()

with tempfile.TemporaryDirectory() as tmp_dir:
    package_dir = Path(tmp_dir) / "demo_pkg"
    package_dir.mkdir()
    (package_dir / "setup.py").write_text(
        "from setuptools import setup\nsetup(name='demo_pkg', version='0.1')\n",
        encoding="utf-8",
    )

    result = handler.pip_install(str(package_dir))

    assert result["success"] is True, result
    assert result["action"] == "pip_install", result
    assert result["package"] == str(package_dir), result

print("pip install ok")
