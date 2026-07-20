# C11 Lead Processor

## Deterministic Lead Qualification with OpenAI-Compatible LLM Support

A portable C11 application that converts unstructured customer inquiries into validated, normalized, and routable JSON records.

The project combines an OpenAI-compatible language model with deterministic C code. The model handles natural-language interpretation, while the C engine remains responsible for schema validation, area calculations, pricing policy, service-area checks, retries, routing, and final output.

This is not a simple API wrapper. The model is treated as an untrusted interpreter whose output must pass deterministic checks before it can affect the final result.

---

## What the Project Does

The application accepts either:

- a CSV file containing multiple leads; or
- a single customer message from the command line.

It can then:

- parse CSV data safely;
- normalize lead fields;
- interpret English and French inquiries;
- identify service fit and space type;
- extract square footage;
- convert square meters to square feet;
- calculate dimensions such as `20 by 30 feet`;
- normalize urgency;
- enforce service-area policy;
- calculate or correct estimate ranges;
- generate a next action and draft reply;
- assign a stable route ID;
- retry malformed model responses;
- fall back to manual review;
- emit one JSON object per lead as JSONL.

---

## Architecture

```text
CSV file or single message
            |
            v
     Input normalization
            |
            v
 OpenAI-compatible model
   or offline mock fixture
            |
            v
    Strict JSON validation
            |
            v
 Deterministic business rules
            |
            v
 Area, pricing, and policy checks
            |
            v
       Route selection
            |
            v
       JSONL output
```

### Responsibility Split

The language model is used for:

- natural-language interpretation;
- identifying likely customer intent;
- recognizing the type of space;
- producing a summary;
- drafting a customer reply;
- suggesting a next step.

The deterministic C11 layer controls:

- accepted JSON structure;
- required fields and types;
- maximum string sizes;
- valid urgency values;
- area extraction and conversion;
- pricing rules;
- service-area policy;
- retry limits;
- route selection;
- manual-review fallback;
- final serialized output.

This separation reduces the risk of allowing an LLM to silently invent prices, bypass policy, or return malformed data.

---

## API Integration

The live client supports OpenAI-compatible `chat/completions` providers.

### Endpoint

```text
POST <LEAD_API_BASE_URL>/chat/completions
```

For OpenAI, the base URL can be:

```text
https://api.openai.com/v1
```

The request asks the provider to return a JSON object:

```json
{
  "response_format": {
    "type": "json_object"
  }
}
```

### Environment Variables

```text
LEAD_API_KEY
LEAD_API_BASE_URL
LEAD_API_MODEL
```

Example:

```bash
export LEAD_API_KEY="your-api-key"
export LEAD_API_BASE_URL="https://api.openai.com/v1"
export LEAD_API_MODEL="gpt-4.1-mini"
```

The API key is required only in live mode. It is not hardcoded in the source and is not intentionally printed or logged.

Provider compatibility depends on support for the OpenAI-style `chat/completions` request and JSON-object response format.

---

# How to Build and Use It

## 1. Install Dependencies

The project requires:

- a C11 compiler;
- Make;
- libcurl;
- cJSON;
- Python 3 for the bundled validation and local HTTP contract tests.

### Debian or Ubuntu

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

```bash
xcode-select --install
brew install curl cjson pkgconf
```

Homebrew may install curl as a keg-only package. This build command explicitly supplies its metadata:

```bash
export PKG_CONFIG_PATH="$(brew --prefix curl)/lib/pkgconfig:$(brew --prefix cjson)/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

make CC=clang \
  CPPFLAGS="-D_POSIX_C_SOURCE=200809L -Iinclude -Ithird_party $(pkg-config --cflags libcurl)" \
  CURL_LIBS="$(pkg-config --libs libcurl)" \
  CJSON_LIBS="$(pkg-config --libs libcjson)"
```

A plain `make CC=clang` may also work when the required headers and libraries are already discoverable.

---

## 2. Build the Application

With GCC:

```bash
make clean
make CC=gcc
```

With Clang:

```bash
make clean
make CC=clang
```

The executable is created at:

```text
build/lead_processor
```

Strict compilation flags are enabled by default:

```text
-std=c11 -Wall -Wextra -Wpedantic -Werror
```

---

## 3. Run the Complete Test Suite

```bash
make test
```

Run the smoke and integration tests:

```bash
make smoke
```

Run both the build and validation flow:

```bash
make validate CC=gcc
```

Run AddressSanitizer and UndefinedBehaviorSanitizer:

```bash
make sanitize
```

---

## 4. Run Offline Mock Mode

Offline mode requires no API key and no internet connection.

Process the bundled 20-row CSV:

```bash
./build/lead_processor \
  --config config/default_config.json \
  --mock fixtures/mock_model_responses.jsonl \
  workbelt-leads-demo.csv
```

Save the JSONL output to a file:

```bash
./build/lead_processor \
  --config config/default_config.json \
  --mock fixtures/mock_model_responses.jsonl \
  workbelt-leads-demo.csv \
  > leads-output.jsonl
```

The mock fixture contains deterministic model responses, making it suitable for local development, demonstrations, CI, and regression testing.

---

## 5. Process a Single Message Offline

```bash
./build/lead_processor \
  --mock fixtures/mock_model_responses.jsonl \
  --message "Need epoxy coating for a 20 by 30 foot garage next week"
```

This mode is useful for quick experiments without creating a CSV file.

---

## 6. Process a Single Message with a Live API

Set the provider configuration:

```bash
export LEAD_API_KEY="your-api-key"
export LEAD_API_BASE_URL="https://api.openai.com/v1"
export LEAD_API_MODEL="gpt-4.1-mini"
```

Run the message:

```bash
./build/lead_processor \
  --message "Need epoxy coating for a 20 by 30 foot garage next week"
```

The model interprets the message, but the final area, policy, pricing, routing, and output remain controlled by the deterministic C layer.

---

## 7. Process a CSV with a Live API

```bash
export LEAD_API_KEY="your-api-key"
export LEAD_API_BASE_URL="https://api.openai.com/v1"
export LEAD_API_MODEL="gpt-4.1-mini"

./build/lead_processor \
  --config config/default_config.json \
  workbelt-leads-demo.csv
```

Save the result:

```bash
./build/lead_processor \
  --config config/default_config.json \
  workbelt-leads-demo.csv \
  > live-leads-output.jsonl
```

Each CSV row can produce a model request, so live CSV execution may consume API tokens and incur provider costs.

---

## 8. View Command-Line Help

```bash
./build/lead_processor --help
```

Main options:

```text
--config <file>    Load a JSON business-policy configuration
--mock <file>      Use offline JSONL model responses
--message <text>   Process one lead message instead of a CSV
--help             Display command usage
```

---

## Configuration

The default configuration is stored at:

```text
config/default_config.json
```

It controls:

- accepted service-area cities;
- behavior when no city is provided;
- minimum and maximum price per square foot;
- urgent-price adjustment;
- high-urgency keywords;
- maximum model retries;
- HTTP timeout;
- maximum response size.

Without `--config`, equivalent compiled demo defaults are used.

The bundled prices are sample values for the synthetic demonstration dataset. They are not real commercial quotes and should be replaced before practical business use.

---

## Example Input

```text
Need epoxy coating for a 20 by 30 foot garage next week.
```

The deterministic area parser recognizes:

```text
20 x 30 = 600 square feet
```

A French metric example is also supported:

```text
J'ai un sous-sol de 35 metres carres.
```

The deterministic conversion produces approximately:

```text
376.7 square feet
```

A message such as:

```text
How much?
```

does not contain enough context to infer a space or area reliably and can be routed as insufficient information instead of forcing a fabricated answer.

---

## Example Output

The application emits JSONL, with one envelope per lead:

```json
{
  "leadId": "1",
  "routeId": "ROUTE_STANDARD_RESIDENTIAL",
  "modelAttempts": 1,
  "result": {
    "fit": true,
    "fitReason": "The request matches the supported flooring service.",
    "space": "garage",
    "sqft": 600,
    "inArea": true,
    "urgency": "medium",
    "summary": "Customer requests an epoxy garage coating.",
    "estimate": "$4440-$5160",
    "nextStep": "Book an on-site assessment.",
    "draftReply": "Thank you for contacting us. We can help assess your garage project."
  }
}
```

Exact wording can vary by model or fixture. The normalized structure and deterministic policy enforcement remain stable.

---

## Route IDs

The router can produce:

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

The route interface is intentionally isolated so a future NeuroRoute adapter can replace or extend route selection without rewriting the CSV parser, model client, validator, or pricing engine.

---

## Error Handling and Safety

The application includes bounded limits for:

- CSV field size;
- CSV column count;
- CSV row count;
- configuration file size;
- mock fixture size;
- model response size;
- retry count;
- HTTP timeout.

The C program does not call:

```text
system()
popen()
sh -c
bash -c
```

Lead text, configuration values, and model output are treated as data and are not executed as shell commands.

Malformed or incomplete model output is rejected. The application can retry the model and eventually route the lead to manual review instead of silently accepting invalid data.

---

## Testing Strategy

The repository includes tests for:

- lead allocation and normalization;
- configuration parsing and rejection;
- CSV quoting and edge cases;
- area extraction;
- metric conversion;
- pricing calculations;
- JSON schema validation;
- route selection;
- mock model behavior;
- retry fallback;
- complete pipeline execution;
- the bundled 20-row demonstration CSV.

A local Python HTTP server is used to verify the real libcurl request contract without requiring an external API key.

The contract test checks:

- `POST /v1/chat/completions`;
- bearer authorization;
- model selection;
- message-array structure;
- `response_format`;
- response parsing;
- deterministic post-processing;
- absence of API-key leakage in application output.

---

## Validation Results

The completed project was validated with:

- GCC builds;
- Clang builds;
- strict C11 warnings treated as errors;
- unit tests;
- runtime smoke tests;
- offline CSV processing;
- single-message processing;
- local live-HTTP contract testing;
- AddressSanitizer;
- UndefinedBehaviorSanitizer;
- ZIP extraction and rebuild;
- TAR.GZ extraction and rebuild;
- archive integrity checks;
- path-traversal checks;
- static source audit.

Bundled demonstration comparison:

```text
Exact rows:              18
Numeric-tolerance rows:   1
Mismatches:               0
Ambiguous rows:           1
Impossible rows:          0
Retries:                  1
Manual-review results:    0
```

The tolerance row comes from accurate metric conversion. The ambiguous row is intentionally not forced into an unsupported conclusion.

---

## Repository Structure

```text
lead_processor/
|-- include/
|   |-- config.h
|   |-- lead.h
|   |-- lead_processor.h
|   |-- lead_router.h
|   |-- lead_rules.h
|   `-- model_client.h
|-- src/
|   |-- config.c
|   |-- csv_reader.c
|   |-- json_validator.c
|   |-- lead.c
|   |-- lead_processor.c
|   |-- lead_router.c
|   |-- lead_rules.c
|   |-- main.c
|   `-- model_client.c
|-- tests/
|-- fixtures/
|-- config/
|-- third_party/
|-- validation/
|-- Makefile
|-- README.md
|-- VALIDATION_REPORT.md
|-- THIRD_PARTY_NOTICES.md
|-- LICENSE
`-- workbelt-leads-demo.csv
```

---

## Engineering Value

This project demonstrates more than calling an AI API.

It includes:

- systems programming in portable C11;
- HTTP and JSON API integration;
- hybrid deterministic and probabilistic architecture;
- defensive parsing;
- bounded resource handling;
- schema enforcement;
- configurable business rules;
- retry and fallback design;
- multilingual input handling;
- unit, integration, sanitizer, and contract testing;
- reproducible offline execution;
- source-only distribution.

The central design principle is that a language model may interpret language, but deterministic code must remain responsible for policy, calculations, validation, and final execution decisions.

---

## Current Limitations

- External provider quality was not validated with a real production API key.
- Provider compatibility depends on OpenAI-style request support.
- Service-area checks use configured city names rather than geocoding.
- French deterministic parsing covers the tested area expressions, not every possible phrase.
- The application does not maintain conversation history.
- The bundled pricing values are demonstration data.
- Windows is not currently supported by the Makefile.
- NeuroRoute integration is planned but is not part of this standalone release.

---

## License

MIT License.
