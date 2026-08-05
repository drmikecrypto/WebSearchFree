"""LangChain tool wrapper for WebSearchFree HTTP API.

Requires optional dependency: pip install langchain-core

Usage:
    from integrations.langchain.wsf_tool import WebSearchFreeTool
    tool = WebSearchFreeTool(base_url="http://127.0.0.1:8080")
    print(tool.invoke({"query": "open source metasearch"}))
"""

from __future__ import annotations

import json
import os
from typing import Any, Optional, Type

try:
    from langchain_core.tools import BaseTool
    from pydantic import BaseModel, Field
except ImportError as ex:  # pragma: no cover
    raise ImportError(
        "langchain-core is required for WebSearchFreeTool. "
        "Install with: pip install langchain-core"
    ) from ex

import urllib.error
import urllib.request


class WebSearchFreeInput(BaseModel):
    query: str = Field(..., description="Web search query")
    max_results: int = Field(5, description="Maximum number of results")
    include_raw_content: bool = Field(
        False, description="If true, fetch and attach page text for each result"
    )


class WebSearchFreeTool(BaseTool):
    name: str = "websearchfree"
    description: str = (
        "Free keyless web search via local WebSearchFree (Tavily-shaped). "
        "Use for live web results without API keys."
    )
    args_schema: Type[BaseModel] = WebSearchFreeInput
    base_url: str = Field(
        default_factory=lambda: os.environ.get("WSF_BASE_URL", "http://127.0.0.1:8080").rstrip("/")
    )

    def _run(
        self,
        query: str,
        max_results: int = 5,
        include_raw_content: bool = False,
        run_manager: Optional[Any] = None,
    ) -> str:
        del run_manager
        payload = {
            "query": query,
            "max_results": max_results,
            "include_raw_content": include_raw_content,
        }
        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(
            self.base_url + "/search",
            data=data,
            method="POST",
            headers={"Content-Type": "application/json"},
        )
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                body = json.loads(resp.read().decode("utf-8"))
        except urllib.error.URLError as ex:
            return json.dumps({"error": f"WebSearchFree unreachable at {self.base_url}: {ex}"})
        return json.dumps(body, indent=2)

    async def _arun(self, *args: Any, **kwargs: Any) -> str:
        return self._run(*args, **kwargs)


class WebSearchFreeExtractInput(BaseModel):
    urls: list[str] = Field(..., description="Page URLs to extract (max 10)")


class WebSearchFreeExtractTool(BaseTool):
    name: str = "websearchfree_extract"
    description: str = "Extract main text from web pages via local WebSearchFree."
    args_schema: Type[BaseModel] = WebSearchFreeExtractInput
    base_url: str = Field(
        default_factory=lambda: os.environ.get("WSF_BASE_URL", "http://127.0.0.1:8080").rstrip("/")
    )

    def _run(self, urls: list[str], run_manager: Optional[Any] = None) -> str:
        del run_manager
        payload = {"urls": urls}
        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(
            self.base_url + "/extract",
            data=data,
            method="POST",
            headers={"Content-Type": "application/json"},
        )
        try:
            with urllib.request.urlopen(req, timeout=120) as resp:
                body = json.loads(resp.read().decode("utf-8"))
        except urllib.error.URLError as ex:
            return json.dumps({"error": f"WebSearchFree unreachable at {self.base_url}: {ex}"})
        return json.dumps(body, indent=2)

    async def _arun(self, *args: Any, **kwargs: Any) -> str:
        return self._run(*args, **kwargs)
