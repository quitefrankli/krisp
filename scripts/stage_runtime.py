"""Stage the engine's runtime defaults without including sample asset collections."""

import shutil
import sys
from pathlib import Path


def stage(source: Path, build: Path) -> None:
    runtime = build / "runtime"
    shutil.copytree(source / "resources/default/textures/skybox",
                    runtime / "resources/textures/skybox", dirs_exist_ok=True)
    (runtime / "configs").mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / "configs/default.yaml", runtime / "configs/default.yaml")
    for shader in (build / "shaders").rglob("*.spv"):
        destination = runtime / "shaders" / shader.relative_to(build / "shaders")
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(shader, destination)
    shutil.copy2(build / "tools/default_environment.krisp-ibl",
                 runtime / "default_environment.krisp-ibl")


if __name__ == "__main__":
    stage(Path(sys.argv[1]), Path(sys.argv[2]))
    Path(sys.argv[3]).touch()
