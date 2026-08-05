#!/usr/bin/env python3
"""Minimal agent-tool bridge: map OpenAI-style tool calls to WebSearchFree HTTP.

Requires a running server (no C++ build in *your* project):

    docker run --rm -p 8080:8080 ghcr.io/drmikecrypto/websearchfree:latest

Then from this repo (or copy integrations/http/client.py next to this file):

    python examples/drop_in_agent.py "what is metasearch"
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "integrations" / "http"))

from client import WebSearchFree  # noqa: E402


def dispatch(name: str, arguments: dict) -> dict:
    wsf = WebSearchFree()
    if name == "web_search":
        return wsf.search(
            query=arguments["query"],
            max_results=int(arguments.get("max_results", 5)),
            include_raw_content=bool(arguments.get("include_raw_content", False)),
        )
    if name == "web_extract":
        return wsf.extract(arguments["urls"])
    raise ValueError(f"unknown tool: {name}")


def main() -> int:
    query = " ".join(sys.argv[1:]) or "open source metasearch"
    # Simulate a model choosing web_search
    result = dispatch("web_search", {"query": query, "max_results": 5})
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
