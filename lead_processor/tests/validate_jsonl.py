#!/usr/bin/env python3
"""Small dependency-free runtime check for lead_processor JSONL output."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def fail(message: str) -> None:
    print(f"validate_jsonl: FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> int:
    if len(sys.argv) != 3:
        fail("usage: validate_jsonl.py FILE EXPECTED_LINES")
    path = Path(sys.argv[1])
    expected = int(sys.argv[2])
    lines = path.read_text(encoding="utf-8").splitlines()
    if len(lines) != expected:
        fail(f"expected {expected} lines, got {len(lines)}")
    required_result = {
        "fit",
        "fitReason",
        "space",
        "sqft",
        "inArea",
        "urgency",
        "summary",
        "estimate",
        "nextStep",
        "draftReply",
    }
    for line_number, line in enumerate(lines, 1):
        try:
            value = json.loads(line)
        except json.JSONDecodeError as exc:
            fail(f"line {line_number}: {exc}")
        if not isinstance(value, dict):
            fail(f"line {line_number}: top level is not an object")
        if set(value) != {"leadId", "routeId", "modelAttempts", "result"}:
            fail(f"line {line_number}: envelope keys differ")
        if not isinstance(value["leadId"], str) or not value["leadId"]:
            fail(f"line {line_number}: invalid leadId")
        if not isinstance(value["routeId"], str) or not value["routeId"].startswith("ROUTE_"):
            fail(f"line {line_number}: invalid routeId")
        if not isinstance(value["modelAttempts"], int) or value["modelAttempts"] < 1:
            fail(f"line {line_number}: invalid modelAttempts")
        result = value["result"]
        if not isinstance(result, dict) or set(result) != required_result:
            fail(f"line {line_number}: result schema differs")
        if result["urgency"] not in {"low", "medium", "high"}:
            fail(f"line {line_number}: invalid urgency")
    print(f"validate_jsonl: PASS ({expected} object(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
