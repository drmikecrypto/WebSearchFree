"""Stdlib HTTP client for WebSearchFree — drop into any Python project.

No native build required. Point at a running server:

    docker run --rm -p 8080:8080 ghcr.io/drmikecrypto/websearchfree:latest
    # or: wsf serve --port 8080

Usage:

    from client import WebSearchFree  # if copied as client.py
    wsf = WebSearchFree()
    print(wsf.search("open source metasearch", max_results=5))
"""

from __future__ import annotations

import json
import os
import urllib.error
import urllib.request
from typing import Any, Optional


class WebSearchFreeError(RuntimeError):
    pass


class WebSearchFree:
    """Thin Tavily-shaped client for a local WebSearchFree HTTP API."""

    def __init__(self, base_url: Optional[str] = None, timeout: float = 60.0) -> None:
        self.base_url = (base_url or os.environ.get("WSF_BASE_URL") or "http://127.0.0.1:8080").rstrip(
            "/"
        )
        self.timeout = timeout

    def _request(self, method: str, path: str, body: Optional[dict[str, Any]] = None) -> Any:
        data = None if body is None else json.dumps(body).encode("utf-8")
        req = urllib.request.Request(
            self.base_url + path,
            data=data,
            method=method,
            headers={"Content-Type": "application/json", "Accept": "application/json"},
        )
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except urllib.error.HTTPError as ex:
            detail = ex.read().decode("utf-8", errors="replace")
            raise WebSearchFreeError(f"HTTP {ex.code}: {detail}") from ex
        except urllib.error.URLError as ex:
            raise WebSearchFreeError(
                f"Cannot reach WebSearchFree at {self.base_url}. "
                "Start it with: wsf serve --port 8080   or   "
                "docker run --rm -p 8080:8080 ghcr.io/drmikecrypto/websearchfree:latest"
            ) from ex

    def health(self) -> dict[str, Any]:
        return self._request("GET", "/health")

    def search(
        self,
        query: str,
        max_results: int = 5,
        include_raw_content: bool = False,
        engines: Optional[list[str]] = None,
        searx_url: Optional[str] = None,
        timeout_ms: Optional[int] = None,
    ) -> dict[str, Any]:
        payload: dict[str, Any] = {
            "query": query,
            "max_results": max_results,
            "include_raw_content": include_raw_content,
        }
        if engines is not None:
            payload["engines"] = engines
        if searx_url:
            payload["searx_url"] = searx_url
        if timeout_ms is not None:
            payload["timeout_ms"] = timeout_ms
        return self._request("POST", "/search", payload)

    def extract(
        self,
        urls: str | list[str],
        timeout_ms: Optional[int] = None,
        concurrency: int = 4,
    ) -> dict[str, Any]:
        if isinstance(urls, str):
            urls = [urls]
        payload: dict[str, Any] = {"urls": list(urls), "concurrency": concurrency}
        if timeout_ms is not None:
            payload["timeout_ms"] = timeout_ms
        return self._request("POST", "/extract", payload)


if __name__ == "__main__":
    import sys

    q = " ".join(sys.argv[1:]) or "open source metasearch"
    client = WebSearchFree()
    print(json.dumps(client.search(q, max_results=3), indent=2))
