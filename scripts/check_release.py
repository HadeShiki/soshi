"""Release gate: the native libraries must exist before the build, and must travel
inside every artifact afterwards.

PyPI refuses to replace a file once it is uploaded, so a wheel without the DLLs would
burn that version number.
"""

import sys
import tarfile
import zipfile
from pathlib import Path

NATIVE = ("soshi.dll", "SDL3.dll")
SOURCE_DIR = Path(__file__).resolve().parent.parent / "python" / "soshi"
FIX = "Run scripts\\build.cmd first."


def fail(message):
    print(f"  {message}")
    print(f"  {FIX}")
    return 1


def members(path):
    """Archive member paths, without the sdist's top-level folder."""
    if path.suffix == ".whl":
        with zipfile.ZipFile(path) as archive:
            return set(archive.namelist())
    with tarfile.open(path) as archive:
        return {name.split("/", 1)[-1] for name in archive.getnames()}


def check_sources():
    missing = [name for name in NATIVE if not (SOURCE_DIR / name).is_file()]
    if missing:
        return fail(f"missing in python/soshi: {', '.join(missing)}")
    print(f"  python/soshi: {', '.join(NATIVE)} present")
    return 0


def check_artifact(path):
    if not path.is_file():
        return fail(f"{path} was not built")

    # A wheel installs the package at its root, an sdist keeps the source layout.
    prefix = "soshi" if path.suffix == ".whl" else "python/soshi"
    present = members(path)
    missing = [f"{prefix}/{name}" for name in NATIVE if f"{prefix}/{name}" not in present]
    if missing:
        return fail(f"{path.name} is missing: {', '.join(missing)}")

    print(f"  {path.name}: native libraries present")
    return 0


def main(argv):
    if argv == ["--sources"]:
        return check_sources()
    if len(argv) != 1:
        print("usage: check_release.py --sources | <artifact>")
        return 2
    return check_artifact(Path(argv[0]))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
