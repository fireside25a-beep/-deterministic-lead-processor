# Lead Processor — Portable C11 Application

A standalone lead-processing pipeline written in C11. It reads leads from CSV or a single message, obtains language interpretation from an OpenAI-compatible `chat/completions` endpoint (or an offline mock file), validates the model JSON, applies deterministic business policy, selects a stable route ID, and emits normalized JSONL.

The project is intentionally standalone. It does **not** depend on NeuroRoute. The `LeadRoute` enum and router API are isolated so a future NeuroRoute adapter can replace route selection without rewriting CSV, model, validation, or business-rule code.

## What is deterministic

C code, not the model, controls:

- CSV parsing, field limits, BOM handling, quoting, and UTF-8 pass-through;
- phone/email normalization where practical;
- area extraction, including `20 by 30 feet` and metric-square conversion;
- service-area decisions;
- urgency keyword overrides;
- sample pricing calculations and estimate correction;
- JSON schema/type/length validation;
- malformed-response retries and manual-review fallback;
- final route selection and normalized output.

The model is used for language-heavy interpretation, summarization, space classification, urgency hints, and customer-reply drafting. Phone and email are not sent in the model prompt.

## Dependencies

- a C11 compiler (`gcc` or `clang`);
- GNU/POSIX `make`;
- libcurl development files;
- cJSON development/runtime library;
- Python 3 for the bundled JSONL and local-HTTP contract tests.

A small cJSON 1.7-compatible declaration header is included under `third_party/`; the cJSON implementation is still linked as an external library.

### Debian / Ubuntu

```bash
sudo apt update
sudo apt install build-essential clang make pkg-config python3 \
  libcurl4-openssl-dev libcjson-dev
```

### Fedora

```bash
sudo dnf install gcc clang make pkgconf-pkg-config python3 \
  libcurl-devel cjson-devel
```

### macOS with Homebrew

Install Apple command-line tools and dependencies:

```bash
xcode-select --install
brew install curl cjson pkgconf
```

Homebrew's curl can be keg-only. This explicit build command uses Homebrew metadata:

```bash
export PKG_CONFIG_PATH="$(brew --prefix curl)/lib/pkgconfig:$(brew --prefix cjson)/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
make CC=clang \
  CPPFLAGS="-D_POSIX_C_SOURCE=200809L -Iinclude -Ithird_party $(pkg-config --cflags libcurl)" \
  CURL_LIBS="$(pkg-config --libs libcurl)" \
  CJSON_LIBS="$(pkg-config --libs libcjson)"
```

A plain `make CC=clang` may also work when the system libcurl headers and Homebrew cJSON are already discoverable.

## Build

```bash
make clean
make CC=gcc
```

The executable is created at:

```text
build/lead_processor
```

Strict compilation flags are enabled by default:

```text
-std=c11 -Wall -Wextra -Wpedantic -Werror
```

## Run offline mock mode

No API key and no internet are needed:

```bash
./build/lead_processor \
  --config config/default_config.json \
  --mock fixtures/mock_model_responses.jsonl \
  workbelt-leads-demo.csv
```

Output is JSONL: one envelope per input lead.

```json
{"leadId":"1","routeId":"ROUTE_STANDARD_RESIDENTIAL","modelAttempts":1,"result":{"fit":true,"fitReason":"...","space":"garage","sqft":500,"inArea":true,"urgency":"medium","summary":"...","estimate":"$3700–$4300","nextStep":"Book an on-site assessment","draftReply":"..."}}
```

## Run single-message mode

Mock smoke test:

```bash
./build/lead_processor \
  --mock fixtures/mock_model_responses.jsonl \
  --message "Need epoxy coating for a 20 by 30 foot garage next week"
```

Live single-message mode uses the same deterministic rules after the model response:

```bash
export LEAD_API_KEY="user-provided-key"
export LEAD_API_BASE_URL="https://api.openai.com/v1"
export LEAD_API_MODEL="gpt-4.1-mini"

./build/lead_processor \
  --message "Need epoxy coating for a 20 by 30 foot garage next week"
```

The API key is required only in live mode. It is never hardcoded and is not intentionally printed or logged.

## Run live CSV mode

```bash
export LEAD_API_KEY="user-provided-key"
export LEAD_API_BASE_URL="https://api.openai.com/v1"
export LEAD_API_MODEL="gpt-4.1-mini"

./build/lead_processor \
  --config config/default_config.json \
  workbelt-leads-demo.csv
```

The client sends:

```text
POST <LEAD_API_BASE_URL>/chat/completions
```

with:

```json
"response_format": {"type":"json_object"}
```

`LEAD_API_BASE_URL` should normally end at the provider's API-version root, such as `/v1`, not at `/chat/completions` itself. Provider compatibility depends on support for the OpenAI-style request and JSON-object response format.

## Configuration

`config/default_config.json` contains:

- service-area city names;
- behavior for leads with no city;
- per-space minimum/maximum sample prices per square foot;
- optional urgent upper-range adjustment;
- high-urgency keyword rules;
- maximum model retries;
- HTTP timeout;
- response-size limit.

**The bundled prices are sample/demo values inferred only to reproduce the synthetic dataset. They are not production quotes. The business owner must replace them before real use.**

Without `--config`, equivalent compiled sample defaults are used.

## Route IDs

```text
ROUTE_SPAM
ROUTE_WRONG_SPECIALTY
ROUTE_INSUFFICIENT_INFORMATION
ROUTE_STANDARD_RESIDENTIAL
ROUTE_COMMERCIAL
ROUTE_URGENT
ROUTE_OUT_OF_AREA
ROUTE_MODEL_RETRY
ROUTE_MANUAL_REVIEW
```

`ROUTE_MODEL_RETRY` is exposed for future routing integration. Current final JSON reports the final route and records retry count in `modelAttempts`.

## Model JSON contract

The model content must be exactly one top-level JSON object containing:

```json
{
  "fit": true,
  "fitReason": "string",
  "space": "garage",
  "sqft": 600,
  "inArea": true,
  "urgency": "medium",
  "summary": "string",
  "estimate": "string or null",
  "nextStep": "string",
  "draftReply": "string"
}
```

`sqft` and `estimate` may be `null`. Urgency must be `low`, `medium`, or `high`. Missing keys, wrong types, trailing text, excessive strings, invalid area values, and malformed JSON are rejected and retried.

## Tests

Run the full ordinary suite:

```bash
make clean
make CC=gcc
make test CC=gcc
make smoke CC=gcc
```

Repeat with Clang:

```bash
make clean
make CC=clang
make test CC=clang
make smoke CC=clang
```

Run GCC AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
make sanitize CC=gcc
```

`make smoke` verifies:

- 20-row offline CSV processing;
- every output line parses as the documented normalized schema;
- single-message mode;
- help output and invalid-config rejection;
- missing-key rejection before networking;
- a real libcurl POST to a local test server;
- `/v1/chat/completions`, bearer authorization, model override, and `response_format` request shape;
- deterministic area, service-area, and pricing overrides after the local model response;
- no API-key leak to process stdout/stderr in that contract test.

## Demo comparison result

The bundled 20-row mock pipeline records:

```text
exact rows:             18
numeric-tolerance rows:  1
mismatches:              0
ambiguous rows:          1
impossible rows:         0
unscored expected fields: 60
malformed-response retries: 1
manual-review fallbacks:    0
```

The exceptions are deliberate:

- lead 12 contains only `how much?`; the CSV labels it as a garage, but the message has no such context. It is scored as ambiguous and deterministically routed to insufficient information;
- lead 13 says `35 mètres carrés`; accurate conversion is about 376.7 square feet while the CSV expectation is rounded to 375, so it is scored using numeric tolerance;
- `fitReason`, `inArea`, and `summary` have no `expected_*` columns, producing 60 unscored fields across 20 rows. No expected labels were invented.

## Safety limits

- CSV file: 10 MiB maximum;
- CSV field: 16 KiB maximum;
- CSV columns: 64 maximum;
- CSV rows: 100,000 maximum;
- config file: 1 MiB maximum;
- mock file: 8 MiB maximum;
- model response: configurable, 64 KiB by default;
- model retries: configurable, bounded to 0–10;
- request timeout: configurable, bounded to 1–300 seconds.

The C application does not call `system()`, `popen()`, `sh -c`, or `bash -c`. It does not execute strings from leads, configuration, or model output.

## Known limitations

- Production model quality and provider compatibility were not validated with a real external API key.
- Service-area matching is city-name policy, not geocoding, postal-code validation, or radius calculation.
- Deterministic French support covers the tested area phrases; broader language understanding remains the model's job.
- The app has no conversation-history store, so context-free messages remain insufficient.
- Windows is not supported by this Makefile. macOS source portability is documented but was not runtime-tested in the validation container.
- The included pricing is demonstration policy and must be replaced for production.

See `VALIDATION_REPORT.md` for the exact environment, commands, passes, skips, and limitations.
