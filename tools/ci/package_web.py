#!/usr/bin/env python3
"""package_web.py -- the deployable Web directory (`HOUSE-02904`).

    tools/ci/package_web.py --build-dir build-consumer

Writes `<build-dir>/package/cna-house-<version>-web/`: the page (as `index.html` too), its script,
wasm and preload -- the preload is already only what ships (`stage_content.py`, `HOUSE-02850`) --
with LICENSE, NOTICE.md, licenses/ and a README on serving it. Large files are hard-linked where
the filesystem allows. The build must be the web preset's, and its download-budget check has run
as part of that build. Offline tooling: not runtime code, not subject to the XNA-only rule.
"""

from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
FILES = ("cna-house.html", "cna-house.js", "cna-house.wasm", "cna-house.data")

README = """CNA House {version} for the Web

Serve this directory from any static HTTP server and open index.html, for example:

    python3 -m http.server 8000     # then http://localhost:8000/

The server must send .wasm as application/wasm; gzip or brotli compression of .data and .wasm is
what keeps the download inside 180 MB. A WebGL 2 browser is required (tested: Chrome 152 and
Firefox 140 ESR). The first click or key press starts audio, as browsers require.

Limitations: settings apply for the session and are not kept across a reload; if the browser
loses the WebGL context (a driver reset, for example), reload the page.

The programme is Ms-PL (LICENSE); runtime notices are in NOTICE.md and every content asset's
author and licence is in licenses/THIRD-PARTY-ASSETS.md.
"""


def place(origin: Path, destination: Path) -> None:
    try:
        os.link(origin, destination)
    except OSError:
        shutil.copy2(origin, destination)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--build-dir", type=Path, required=True)
    arguments = parser.parse_args()
    build = arguments.build_dir.resolve()
    missing = [name for name in FILES if not (build / name).is_file()]
    if missing:
        print(f"package_web: not a built web preset tree ({', '.join(missing)} missing)", file=sys.stderr)
        return 1

    version = (REPO / "VERSION").read_text().strip()
    out = build / "package" / f"cna-house-{version}-web"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    for name in FILES:
        place(build / name, out / name)
    shutil.copy2(build / "cna-house.html", out / "index.html")
    for document in ("LICENSE", "NOTICE.md"):
        shutil.copy2(REPO / document, out / document)
    shutil.copytree(REPO / "licenses", out / "licenses")
    (out / "README.txt").write_text(README.format(version=version))
    size = sum(p.stat().st_size for p in out.rglob("*") if p.is_file())
    print(f"package_web: wrote {out} ({size / 1e6:.1f} MB before compression)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
