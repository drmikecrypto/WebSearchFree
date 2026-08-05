#!/usr/bin/env python3
"""Minimal stdio MCP server for WebSearchFree (stdlib only).

Talks to a running `wsf serve` HTTP API by default (WSF_BASE_URL),
or falls back to the `wsf` CLI on PATH.

Cursor / Claude Desktop example:

{
  "mcpServers": {
    "websearchfree": {
      "command": "python",
      "args": ["integrations/mcp/server.py"],
      "env": { "WSF_BASE_URL": "http://127.0.0.1:8080" }
    }
  }
}
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import urllib.error
import urllib.request
from typing import Any

BASE_URL = os.environ.get("WSF_BASE_URL", "http://127.0.0.1:8080").rstrip("/")
PROTOCOL_VERSION = "2024-11-05"
SERVER_NAME = "websearchfree"
SERVER_VERSION = "0.2.1"


def _http_json(method: str, path: str, body: dict[str, Any] | None = None) -> dict[str, Any]:
    data = None if body is None else json.dumps(body).encode("utf-8")
    req = urllib.request.Request(
        BASE_URL + path,
        data=data,
        method=method,
        headers={"Content-Type": "application/json", "Accept": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=60) as resp:
        return json.loads(resp.read().decode("utf-8"))


def _cli_json(args: list[str]) -> dict[str, Any]:
    wsf = shutil.which("wsf")
    if not wsf:
        raise RuntimeError("wsf CLI not found and HTTP API unreachable")
    out = subprocess.check_output([wsf, *args], text=True, timeout=120)
    return json.loads(out)


def web_search(query: str, max_results: int = 5, include_raw_content: bool = False) -> dict[str, Any]:
    payload = {
        "query": query,
        "max_results": max_results,
        "include_raw_content": include_raw_content,
    }
    try:
        return _http_json("POST", "/search", payload)
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError):
        cmd = ["search", query, "--max", str(max_results), "--json"]
        if include_raw_content:
            cmd.append("--raw")
        return _cli_json(cmd)


def web_extract(urls: list[str]) -> dict[str, Any]:
    if isinstance(urls, str):
        urls = [urls]
    payload = {"urls": urls}
    try:
        return _http_json("POST", "/extract", payload)
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError):
        if not urls:
            return {"results": []}
        one = _cli_json(["extract", urls[0], "--json"])
        return {"results": [one]}


TOOLS = [
    {
        "name": "web_search",
        "description": "Keyless metasearch via WebSearchFree (DuckDuckGo/Brave/Wikipedia).",
        "inputSchema": {
            "type": "object",
            "required": ["query"],
            "properties": {
                "query": {"type": "string", "description": "Search query"},
                "max_results": {"type": "integer", "default": 5},
                "include_raw_content": {"type": "boolean", "default": False},
            },
        },
    },
    {
        "name": "web_extract",
        "description": "Fetch URL(s) and extract main text for RAG/LLM context.",
        "inputSchema": {
            "type": "object",
            "required": ["urls"],
            "properties": {
                "urls": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "One or more page URLs (max 10)",
                }
            },
        },
    },
]


def _result_text(obj: Any) -> dict[str, Any]:
    return {
        "content": [{"type": "text", "text": json.dumps(obj, indent=2)}],
        "isError": False,
    }


def _error_text(message: str) -> dict[str, Any]:
    return {
        "content": [{"type": "text", "text": message}],
        "isError": True,
    }


def handle_request(msg: dict[str, Any]) -> dict[str, Any] | None:
    method = msg.get("method")
    req_id = msg.get("id")
    params = msg.get("params") or {}

    # Notifications have no id and get no response.
    if req_id is None and method and method.startswith("notifications/"):
        return None

    if method == "initialize":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {"tools": {}},
                "serverInfo": {"name": SERVER_NAME, "version": SERVER_VERSION},
            },
        }

    if method == "ping":
        return {"jsonrpc": "2.0", "id": req_id, "result": {}}

    if method == "tools/list":
        return {"jsonrpc": "2.0", "id": req_id, "result": {"tools": TOOLS}}

    if method == "tools/call":
        name = params.get("name")
        args = params.get("arguments") or {}
        try:
            if name == "web_search":
                data = web_search(
                    str(args.get("query", "")),
                    int(args.get("max_results", 5)),
                    bool(args.get("include_raw_content", False)),
                )
                return {"jsonrpc": "2.0", "id": req_id, "result": _result_text(data)}
            if name == "web_extract":
                urls = args.get("urls") or args.get("url") or []
                if isinstance(urls, str):
                    urls = [urls]
                data = web_extract(list(urls))
                return {"jsonrpc": "2.0", "id": req_id, "result": _result_text(data)}
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": _error_text(f"Unknown tool: {name}"),
            }
        except Exception as ex:  # noqa: BLE001 — surface to MCP client
            return {"jsonrpc": "2.0", "id": req_id, "result": _error_text(str(ex))}

    if method == "resources/list":
        return {"jsonrpc": "2.0", "id": req_id, "result": {"resources": []}}

    if method == "prompts/list":
        return {"jsonrpc": "2.0", "id": req_id, "result": {"prompts": []}}

    if req_id is None:
        return None

    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "error": {"code": -32601, "message": f"Method not found: {method}"},
    }


def main() -> None:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            continue
        resp = handle_request(msg)
        if resp is not None:
            sys.stdout.write(json.dumps(resp, separators=(",", ":")) + "\n")
            sys.stdout.flush()


if __name__ == "__main__":
    main()
