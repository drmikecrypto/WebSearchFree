#!/usr/bin/env python3
"""Minimal example: metasearch without an API key."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys


def main() -> int:
    query = " ".join(sys.argv[1:]) or "open source metasearch"
    wsf = shutil.which("wsf")
    if not wsf:
        print("Build the project and add `wsf` to PATH first.", file=sys.stderr)
        return 1
    raw = subprocess.check_output(
        [wsf, "search", query, "--max", "5", "--json"], text=True
    )
    data = json.loads(raw)
    for w in data.get("warnings") or []:
        print(f"warning: {w}", file=sys.stderr)
    for i, r in enumerate(data.get("results", []), 1):
        print(f"{i}. {r['title']}\n   {r['url']}\n   {r.get('content', '')[:160]}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
