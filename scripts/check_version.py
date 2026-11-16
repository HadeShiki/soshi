"""Release gate: a pushed tag must name the version in python/soshi/version.py.

PyPI refuses to replace an uploaded file, so a tag that disagrees with the version
would either publish the wrong number or fail after part of the upload is already
there.
"""

import re
import sys
from pathlib import Path

VERSION_FILE = Path(__file__).resolve().parent.parent / "python" / "soshi" / "version.py"


def main(argv):
    if len(argv) != 1:
        print("usage: check_version.py <tag>")
        return 2

    tag = argv[0].strip()
    match = re.search(r'__version__\s*=\s*"([^"]+)"', VERSION_FILE.read_text(encoding="utf-8"))
    if not match:
        print(f"  no __version__ in {VERSION_FILE}")
        return 1

    version = match.group(1)
    if tag.lstrip("v") != version:
        print(f"  tag {tag} does not match {VERSION_FILE.name} ({version})")
        print(f"  bump the version first, or push the tag v{version}")
        return 1

    print(f"  tag {tag} matches version {version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
