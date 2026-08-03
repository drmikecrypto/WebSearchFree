"""WebSearchFree Python package.

Prefer the compiled extension `wsf_native` when available. Falls back to
calling the `wsf` CLI if the extension is not built.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


def _prepare_native_dlls() -> None:
    """Help Windows find MinGW runtime DLLs next to the extension or on PATH."""
    if sys.platform != "win32":
        return
    candidates = [
        Path(__file__).resolve().parent.parent.parent / "build" / "bindings" / "python",
        Path(sys.prefix) / "Library" / "bin",
        Path(r"C:\mingw64\bin"),
        Path(r"C:\msys64\ucrt64\bin"),
    ]
    for p in candidates:
        if p.is_dir():
            try:
                os.add_dll_directory(str(p))
            except (OSError, AttributeError):
                pass


_prepare_native_dlls()

try:
    from wsf_native import search as _native_search
    from wsf_native import extract as _native_extract

    def search(
        query: str,
        max_results: int = 5,
        include_raw_content: bool = False,
        engines: list[str] | None = None,
        timeout_ms: int = 8000,
    ) -> dict[str, Any]:
        resp = _native_search(
            query,
            max_results=max_results,
            include_raw_content=include_raw_content,
            engines=engines or ["ddg", "brave", "wikipedia"],
            timeout_ms=timeout_ms,
        )
        return resp.to_dict()

    def extract(url: str, timeout_ms: int = 8000) -> str:
        return _native_extract(url, timeout_ms)

except ImportError:

    def search(
        query: str,
        max_results: int = 5,
        include_raw_content: bool = False,
        engines: list[str] | None = None,
        timeout_ms: int = 8000,
    ) -> dict[str, Any]:
        wsf = shutil.which("wsf")
        if not wsf:
            raise RuntimeError(
                "wsf_native not built and `wsf` CLI not on PATH. "
                "Build with -DWSF_BUILD_PYTHON=ON or install the CLI."
            )
        cmd = [
            wsf,
            "search",
            query,
            "--max",
            str(max_results),
            "--json",
            "--timeout",
            str(timeout_ms),
        ]
        if include_raw_content:
            cmd.append("--raw")
        if engines:
            cmd.extend(["--engines", ",".join(engines)])
        out = subprocess.check_output(cmd, text=True)
        return json.loads(out)

    def extract(url: str, timeout_ms: int = 8000) -> str:
        del timeout_ms  # CLI uses default timeout
        wsf = shutil.which("wsf")
        if not wsf:
            raise RuntimeError("wsf CLI not found on PATH")
        out = subprocess.check_output([wsf, "extract", url, "--json"], text=True)
        return json.loads(out).get("raw_content", "")
