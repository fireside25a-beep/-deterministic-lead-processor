# Lead Processor Portfolio Case Study

## Deterministic C11 + LLM Lead Qualification Pipeline

A production-style **C11 lead processing application** that combines deterministic business logic with an OpenAI-compatible `chat/completions` API.

## Overview

This project transforms unstructured customer inquiries into validated, normalized JSON suitable for CRMs and automation systems.

Instead of trusting an LLM to make business decisions, the architecture separates responsibilities.

### Language Model
- Understands English and French inquiries
- Determines service intent
- Identifies space type
- Infers urgency
- Produces summaries
- Drafts customer replies

### Deterministic C11 Engine
- Safe CSV parsing
- JSON schema validation
- Area extraction
- Metric and imperial conversions
- Pricing rules
- Service-area validation
- Routing
- Retry handling
- Manual-review fallback

## API

Compatible with OpenAI-compatible providers.

Endpoint:

`POST /v1/chat/completions`

Environment variables:

- `LEAD_API_KEY`
- `LEAD_API_BASE_URL`
- `LEAD_API_MODEL`

No API keys are stored in source code.

## Processing Pipeline

```text
CSV Input
    |
Normalization
    |
LLM
    |
JSON Validation
    |
Business Rules
    |
Pricing
    |
Routing
    |
JSONL Output
```

## Validation

Validated using:

- GCC
- Clang
- AddressSanitizer
- UndefinedBehaviorSanitizer
- Runtime smoke tests
- Offline mock execution
- HTTP contract tests
- JSON validation
- CSV edge-case testing

## Portfolio Value

This project demonstrates:

- Modern C systems programming
- AI API integration
- Deterministic software architecture
- Defensive programming
- Hybrid AI pipelines
- Automated validation
- Testing and verification
- Production-oriented engineering

The language model is responsible for natural-language understanding, while deterministic C code remains responsible for enforcing business policy and producing predictable outputs.
