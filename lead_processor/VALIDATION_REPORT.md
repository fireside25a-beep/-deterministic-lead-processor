# Lead Processor Validation Report

Validation date: 2026-07-20

## Scope

This report covers the standalone C11 implementation only. NeuroRoute was not integrated, and no existing GitHub repository was modified.

## Actual test host

```text
Operating system: Debian GNU/Linux 13 (trixie)
Architecture:     x86_64
Kernel report:    Linux 4.4.0
GCC:              gcc (Debian 14.2.0-19) 14.2.0
Clang:            clang 17.0.0
GNU Make:         4.4.1
libcurl:          8.10.1
cJSON runtime:    1.7.18-3.1+deb13u1
Python:           3.13.5
```

The produced test binary was a dynamically linked Linux x86-64 ELF PIE using libcurl and libcjson.

## Input inspection

`workbelt-leads-demo.csv` was inspected before implementation:

```text
size:          9,174 bytes
encoding:      UTF-8 with BOM
line endings:  CRLF
separator:     semicolon
headers:       15
lead rows:     20
quoted fields: present
blank fields:  present
Unicode:       present, including French text and emoji
```

Headers:

```text
lead_id
channel
customer_name
city
phone
email
source
message
expected_fit
expected_space
expected_sqft
expected_urgency
expected_estimate
expected_next_step
expected_draft_reply
```

## Strict GCC build and tests

Commands:

```bash
make clean
make CC=gcc
make test CC=gcc
make smoke CC=gcc
```

Result: **PASS**

Passed compiled tests:

```text
test_lead
test_config
test_csv
test_area
test_pricing
test_schema
test_router
test_model_client
test_pipeline
test_demo_csv
```

Passed runtime smoke checks:

```text
20-row mock CSV pipeline
20 normalized JSONL schema checks
single-message mock pipeline
single normalized JSONL schema check
help output
invalid configuration rejection
missing LEAD_API_KEY rejection
local OpenAI-compatible HTTP contract test
```

The local HTTP contract test performed a real libcurl POST to a loopback server and verified:

```text
POST /v1/chat/completions
Authorization: Bearer <test-only local value>
Content-Type: application/json
model override
response_format = {"type":"json_object"}
two-message system/user request
choices[0].message.content parsing
deterministic post-model area/service/pricing enforcement
no test API-key value in stdout or stderr
```

The bad-endpoint unit test used `127.0.0.1:1`, returned a real libcurl connection failure, and was correctly reported as an HTTP request error.

## Strict Clang build and tests

Commands:

```bash
make clean
make CC=clang
make test CC=clang
make smoke CC=clang
```

Result: **PASS**

The same ten compiled tests and all smoke checks passed with Clang 17.0.0 under strict C11 warnings-as-errors.

## Sanitizers

Command:

```bash
make sanitize CC=gcc
```

Flags:

```text
-fsanitize=address,undefined
-fno-omit-frame-pointer
-O1 -g
-Wall -Wextra -Wpedantic -Werror
```

Result: **PASS**

All ten compiled tests, the 20-row mock pipeline, single-message pipeline, missing-key test, and local HTTP contract test completed without an AddressSanitizer or UndefinedBehaviorSanitizer report.

## Demo expected-data comparison

Observed result:

```text
exact=18
tolerance=1
mismatch=0
ambiguous=1
impossible=0
unscored_fields=60
retried=1
manual_review=0
```

Interpretation:

- 18 rows matched all scoreable expected columns exactly.
- Lead 13 used numeric tolerance because 35 square metres converts to approximately 376.7 square feet while the dataset says 375.
- Lead 12 was marked ambiguous because `how much?` does not contain enough context to infer a garage, despite the dataset's expected garage label.
- No scoreable row mismatched.
- No row was wholly impossible to score.
- `fitReason`, `inArea`, and `summary` lack expected columns, so 3 fields × 20 rows = 60 unscored fields.
- Lead 19 consumed one malformed mock response, retried, then accepted the next valid response.
- No demo row exhausted retries into manual review.

A separate pipeline test supplied three malformed responses and verified fallback to `ROUTE_MANUAL_REVIEW` after three total attempts with the default retry setting.

## Security and robustness checks

Result: **PASS** for the implemented checks.

```text
No hardcoded production API key
Empty LEAD_API_KEY in .env.example
No API-key printing in application error paths tested
Invalid fractional retry and negative urgent-price settings rejected
No system(), popen(), sh -c, or bash -c in C application code
Bounded CSV/config/mock/HTTP inputs
Allocation failures checked in owned dynamic buffers
Model prompt values JSON-escaped through cJSON
Strict top-level model JSON parsing with trailing-text rejection
Required model keys and types validated
Urgency enum enforced
Nullable sqft and estimate validated
HTTP timeout and response-size limits applied
HTTP status and malformed response handling tested
Malformed model response retry tested
Retry-exhaustion manual-review fallback tested
Generated binaries excluded by .gitignore and release archives
```

## Static release audit

Result: **PASS**

```text
No TODO/FIXME/placeholder markers in implementation/test/config/fixture files
No system(), popen(), sh -c, or bash -c in C application or C tests
No likely embedded provider key pattern
.env.example contains an empty LEAD_API_KEY
Every C/header/Python source file is at most 600 lines
Python test scripts compile successfully
All required deliverable paths are present
```

## Failed tests

None in the final GCC, Clang, or GCC sanitizer runs.

## Skipped / not claimed

- No real external model-provider call was made because no user API key was supplied. Request construction and response parsing were validated against a local OpenAI-compatible HTTP server.
- macOS compilation/runtime was not performed on this Linux host.
- Windows compilation/runtime was not performed and is not supported by the included Makefile.
- NeuroRoute integration was intentionally not attempted because the standalone application was the required first deliverable.

## Known limitations

- City matching is configured string matching, not geocoding.
- Production results still depend on the selected model's language performance and schema compliance.
- Some OpenAI-compatible providers may not implement `response_format` identically.
- Deterministic French extraction is deliberately narrow; broader French interpretation is delegated to the model.
- Context-free leads cannot use prior conversation history because no conversation store is included.
- Pricing values are synthetic sample defaults and must be replaced by the business owner.
