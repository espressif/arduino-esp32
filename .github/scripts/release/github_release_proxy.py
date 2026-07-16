#!/usr/bin/env python3
"""Serve package JSON locally and stream draft release assets via the GitHub API."""

from __future__ import annotations

import argparse
import http.server
import json
import mimetypes
import os
import socketserver
import sys
import urllib.error
import urllib.request


class ReleaseProxyHandler(http.server.BaseHTTPRequestHandler):
    static_dir: str
    asset_ids: dict[str, int]
    repo: str
    token: str

    def log_message(self, fmt: str, *args) -> None:
        # Log errors to stderr for debuggability
        sys.stderr.write(f"[release-proxy] {fmt % args}\n")

    def do_HEAD(self) -> None:
        self._dispatch(send_body=False)

    def do_GET(self) -> None:
        self._dispatch(send_body=True)

    def _dispatch(self, send_body: bool) -> None:
        name = self.path.split("?", 1)[0].lstrip("/")
        if name == "health":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            if send_body:
                self.end_headers()
                self.wfile.write(b"ok")
            else:
                self.send_header("Content-Length", "2")
                self.end_headers()
            return
        if not name or ".." in name or name.startswith("/"):
            self.send_error(400, "bad path")
            return

        # Package indexes are local; draft release archives always stream via GitHub API.
        if name.endswith(".json"):
            local_path = os.path.join(self.static_dir, name)
            if os.path.isfile(local_path):
                self._serve_file(local_path, send_body=send_body)
                return
            self.send_error(404, f"not found: {name}")
            return

        asset_id = self.asset_ids.get(name)
        if asset_id is not None:
            self._proxy_github_asset(asset_id, name, send_body=send_body)
            return

        self.send_error(404, f"not found: {name}")

    def _serve_file(self, path: str, send_body: bool = True) -> None:
        content_type = mimetypes.guess_type(path)[0] or "application/octet-stream"
        if send_body:
            with open(path, "rb") as fh:
                data = fh.read()
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        size = os.path.getsize(path)
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(size))
        self.end_headers()

    def _proxy_github_asset(self, asset_id: int, name: str, send_body: bool = True) -> None:
        url = f"https://api.github.com/repos/{self.repo}/releases/assets/{asset_id}"
        method = "GET" if send_body else "HEAD"
        sys.stderr.write(
            f"[release-proxy] draft asset {name} (id={asset_id}) via GitHub API ({method})\n"
        )
        req = urllib.request.Request(
            url,
            method=method,
            headers={
                "Authorization": f"Bearer {self.token}",
                "Accept": "application/octet-stream",
                "User-Agent": "arduino-esp32-release-test-proxy",
            },
        )
        try:
            with urllib.request.urlopen(req, timeout=600) as resp:
                self.send_response(resp.status)
                self.send_header("X-Draft-Release-Source", "github-api")
                content_type = resp.headers.get("Content-Type")
                if content_type:
                    self.send_header("Content-Type", content_type)
                content_length = resp.headers.get("Content-Length")
                if content_length:
                    self.send_header("Content-Length", content_length)
                self.end_headers()
                if not send_body:
                    return
                while True:
                    chunk = resp.read(1024 * 64)
                    if not chunk:
                        break
                    self.wfile.write(chunk)
        except urllib.error.HTTPError as exc:
            body = exc.read(256).decode("utf-8", errors="replace")
            sys.stderr.write(f"[release-proxy] GitHub API {exc.code} for {name}: {body}\n")
            self.send_error(exc.code, f"github asset proxy failed for {name}")
        except urllib.error.URLError as exc:
            sys.stderr.write(f"[release-proxy] Network error for {name}: {exc.reason}\n")
            self.send_error(502, f"github asset proxy network error for {name}")


def load_asset_ids(path: str) -> dict[str, int]:
    with open(path, encoding="utf-8") as fh:
        data = json.load(fh)
    mapping: dict[str, int] = {}
    for name, entry in (data.get("assets") or {}).items():
        if isinstance(entry, dict) and entry.get("id") is not None:
            mapping[name] = int(entry["id"])
    if not mapping:
        raise SystemExit(f"ERROR: no asset ids in {path}")
    return mapping


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dir", required=True, help="Directory with package JSON files")
    parser.add_argument("--assets", required=True, help="draft-assets.json path")
    parser.add_argument("--repo", required=True, help="owner/repo")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--pid-file", required=True, help="Write bound port number here")
    args = parser.parse_args()

    token = os.environ.get("GITHUB_TOKEN")
    if not token:
        raise SystemExit("ERROR: GITHUB_TOKEN required")

    static_dir = os.path.abspath(args.dir)
    asset_ids = load_asset_ids(args.assets)

    handler = ReleaseProxyHandler
    handler.static_dir = static_dir
    handler.asset_ids = asset_ids
    handler.repo = args.repo
    handler.token = token

    with socketserver.TCPServer(("127.0.0.1", args.port), handler) as httpd:
        port = httpd.server_address[1]
        # Write port file atomically to avoid consumers reading partial content
        tmp_pid = args.pid_file + ".tmp"
        with open(tmp_pid, "w", encoding="utf-8") as fh:
            fh.write(str(port))
        os.replace(tmp_pid, args.pid_file)
        sys.stderr.write(f"[release-proxy] listening on http://127.0.0.1:{port}\n")
        httpd.serve_forever()


if __name__ == "__main__":
    main()
