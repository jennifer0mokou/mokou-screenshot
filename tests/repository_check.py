"""Portable repository and installer checks; all writes stay in temporary directories."""

import ast
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RepositoryCheck(unittest.TestCase):
    def test_document_links_and_python_sources(self):
        for path in [
            ROOT / "README.md",
            ROOT / "AGENTS.md",
            ROOT / "THIRD_PARTY_NOTICES.md",
            *ROOT.glob("docs/**/*.md"),
        ]:
            for target in re.findall(r"\]\(([^)]+)\)", path.read_text()):
                if "://" not in target and not target.startswith("#"):
                    self.assertTrue(
                        (path.parent / target.split("#")[0]).exists(), (path, target)
                    )
        for path in [*ROOT.glob("ocr/*.py"), Path(__file__)]:
            ast.parse(path.read_text(), filename=str(path))
        for path in ROOT.glob("scripts/*.sh"):
            subprocess.run(["bash", "-n", str(path)], check=True)

    def test_install_without_ocr_and_uninstall(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            commands = root / "commands"
            commands.mkdir()
            for name, text in {
                "pgrep": "exit 1",
                "cmake": "exit 0",
                "kbuildsycoca6": "exit 0",
            }.items():
                command = commands / name
                command.write_text("#!/bin/sh\n" + text + "\n")
                command.chmod(0o755)
            prefix = root / "prefix with spaces"
            environment = {
                **os.environ,
                "PATH": str(commands) + ":" + os.environ["PATH"],
                "MUSHROOM_INSTALL_PREFIX": str(prefix),
                "XDG_CONFIG_HOME": str(root / "config"),
            }
            subprocess.run(
                [str(ROOT / "scripts/install.sh"), "--without-ocr"],
                env=environment,
                check=True,
                capture_output=True,
            )
            autostart = root / "config/autostart/io.github.fedokou.MushroomShot.desktop"
            self.assertIn(
                f'Exec="{prefix}/bin/mushroom-shot" --background', autostart.read_text()
            )
            self.assertFalse((prefix / "share/mushroom-shot/ocr/venv").exists())
            # The uninstall list must preserve user content, settings and the OCR environment.
            retained = [
                prefix / "share/mushroom-shot/ocr/venv/keep",
                root / "config/fedokou/MushroomShot.conf",
            ]
            for path in retained:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("keep")
            notices = prefix / "share/licenses/mushroom-shot/LICENSE"
            notices.parent.mkdir(parents=True)
            notices.write_text("license")
            binary = prefix / "bin/mushroom-shot"
            binary.parent.mkdir(parents=True)
            binary.touch()
            subprocess.run(
                [str(ROOT / "scripts/uninstall.sh")],
                env=environment,
                check=True,
                capture_output=True,
            )
            self.assertFalse(binary.exists())
            self.assertFalse(notices.exists())
            self.assertFalse(autostart.exists())
            for path in retained:
                self.assertEqual(path.read_text(), "keep")

    def test_installer_arguments(self):
        environment = {**os.environ, "MUSHROOM_INSTALL_PREFIX": "relative-prefix"}
        result = subprocess.run(
            [str(ROOT / "scripts/install.sh"), "--without-ocr"],
            env=environment,
            capture_output=True,
        )
        self.assertEqual(result.returncode, 2)
        result = subprocess.run(
            [str(ROOT / "scripts/install.sh"), "--unknown"], capture_output=True
        )
        self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    unittest.main()
