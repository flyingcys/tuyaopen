from __future__ import annotations

import os
from pathlib import Path


def iter_candidate_binaries(project_dir: Path):
    for base_dir in (project_dir / "dist", project_dir / ".build/bin"):
        if not base_dir.exists():
            continue

        for candidate in sorted(base_dir.rglob("*")):
            if candidate.is_file() and os.access(candidate, os.X_OK):
                yield candidate


def discover_default_binary(project_dir: Path) -> Path | None:
    for candidate in iter_candidate_binaries(project_dir):
        return candidate

    return None


class FlashRunner:
    def __init__(self, project_dir: Path, image_path: Path | None = None):
        self.project_dir = Path(project_dir)
        self.image_path = Path(image_path) if image_path is not None else discover_default_binary(self.project_dir)

    def flash(self, port: str | None = None) -> Path:
        if self.image_path is None:
            raise RuntimeError("No built target image found under dist/ or .build/bin/")

        if port is None:
            return self.image_path

        raise RuntimeError("FlashRunner skeleton does not implement board flashing yet")
