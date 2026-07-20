#!/usr/bin/env python3
"""Exercise the real libcurl chat/completions path against a local HTTP server."""

from __future__ import annotations

import http.server
import json
import os
import subprocess
import sys
import threading
from pathlib import Path
from typing import Any


class CaptureServer(http.server.ThreadingHTTPServer):
    captured: dict[str, Any]


class Handler(http.server.BaseHTTPRequestHandler):
    server: CaptureServer

    def do_POST(self) -> None:  # noqa: N802 - required by BaseHTTPRequestHandler
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        self.server.captured = {
            "path": self.path,
            "authorization": self.headers.get("Authorization"),
            "content_type": self.headers.get("Content-Type"),
            "body": body,
        }
        model_result = {
            "fit": True,
            "fitReason": "Local contract-test flooring lead",
            "space": "garage",
            "sqft": 600,
            "inArea": False,
            "urgency": "medium",
            "summary": "A garage coating request.",
            "estimate": None,
            "nextStep": "Book an on-site assessment",
            "draftReply": "Thanks for reaching out.",
        }
        response = {
            "id": "local-test",
            "choices": [{"message": {"role": "assistant", "content": json.dumps(model_result)}}],
        }
        encoded = json.dumps(response).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def log_message(self, format: str, *args: object) -> None:
        del format, args


def check(condition: bool, message: str) -> None:
    if not condition:
        print(f"test_live_http: FAIL: {message}", file=sys.stderr)
        raise SystemExit(1)


def main() -> int:
    binary = Path("build/lead_processor")
    check(binary.is_file(), "build/lead_processor is missing")

    server = CaptureServer(("127.0.0.1", 0), Handler)
    server.captured = {}
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()

    secret = "local-contract-key-not-for-production"
    env = os.environ.copy()
    env.update(
        {
            "LEAD_API_KEY": secret,
            "LEAD_API_BASE_URL": f"http://127.0.0.1:{server.server_port}/v1",
            "LEAD_API_MODEL": "local-contract-model",
        }
    )
    try:
        completed = subprocess.run(
            [str(binary), "--message", "Need epoxy coating for a 20 by 30 foot garage next week"],
            env=env,
            text=True,
            capture_output=True,
            timeout=10,
            check=False,
        )
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)

    check(completed.returncode == 0, f"CLI returned {completed.returncode}: {completed.stderr}")
    check(secret not in completed.stdout, "API key leaked to stdout")
    check(secret not in completed.stderr, "API key leaked to stderr")
    captured = server.captured
    check(captured.get("path") == "/v1/chat/completions", "wrong endpoint path")
    check(captured.get("authorization") == f"Bearer {secret}", "missing/wrong bearer header")
    check(str(captured.get("content_type", "")).startswith("application/json"), "wrong content type")

    request = json.loads(captured["body"].decode("utf-8"))
    check(request.get("model") == "local-contract-model", "model override was not used")
    check(request.get("response_format") == {"type": "json_object"}, "response_format contract differs")
    messages = request.get("messages")
    check(isinstance(messages, list) and len(messages) == 2, "messages array differs")
    check(messages[0].get("role") == "system", "system message missing")
    check(messages[1].get("role") == "user", "user message missing")
    user_content = json.loads(messages[1]["content"])
    check("20 by 30 foot garage" in user_content.get("message", ""), "lead message missing from prompt")

    output = json.loads(completed.stdout)
    check(output["leadId"] == "single", "wrong single-message lead ID")
    check(output["result"]["sqft"] == 600, "deterministic area rule did not survive live path")
    check(output["result"]["inArea"] is True, "deterministic service-area rule did not override model")
    check(output["result"]["estimate"] == "$4450–$5150", "deterministic pricing did not run")
    print("test_live_http: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
