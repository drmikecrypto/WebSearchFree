#!/usr/bin/env bash
# Install the latest WebSearchFree (wsf) release binary for Linux/macOS-like amd64.
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/drmikecrypto/WebSearchFree/main/scripts/install.sh | bash
#   WSF_INSTALL_DIR=~/bin curl -fsSL ... | bash
set -euo pipefail

REPO="${WSF_REPO:-drmikecrypto/WebSearchFree}"
INSTALL_DIR="${WSF_INSTALL_DIR:-$HOME/.local/bin}"
ASSET="wsf-linux-amd64.tar.gz"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "This installer currently ships Linux amd64 binaries." >&2
  echo "On macOS/Windows use Docker: docker run --rm -p 8080:8080 ghcr.io/drmikecrypto/websearchfree:latest" >&2
  exit 1
fi

arch="$(uname -m)"
if [[ "$arch" != "x86_64" && "$arch" != "amd64" ]]; then
  echo "Unsupported arch: $arch (need amd64). Use Docker instead." >&2
  exit 1
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

api="https://api.github.com/repos/${REPO}/releases/latest"
echo "Fetching latest release from ${REPO}…"
url="$(curl -fsSL "$api" | sed -n "s/.*\"browser_download_url\": \"\\([^\"]*${ASSET}\\)\".*/\\1/p" | head -n1)"
if [[ -z "$url" ]]; then
  echo "Could not find asset ${ASSET} in the latest GitHub release." >&2
  echo "Create a release (tag v*) or use: docker compose up --build" >&2
  exit 1
fi

echo "Downloading ${url}"
curl -fsSL "$url" -o "${tmpdir}/${ASSET}"
tar -xzf "${tmpdir}/${ASSET}" -C "$tmpdir"

mkdir -p "$INSTALL_DIR"
install -m 755 "${tmpdir}/wsf" "${INSTALL_DIR}/wsf"

echo
echo "Installed: ${INSTALL_DIR}/wsf"
if ! command -v wsf >/dev/null 2>&1; then
  echo "Add to PATH, e.g.:"
  echo "  export PATH=\"${INSTALL_DIR}:\$PATH\""
fi
echo
echo "Try:"
echo "  wsf search \"open source metasearch\" --max 3"
echo "  wsf serve --port 8080"
echo "  # open http://127.0.0.1:8080"
