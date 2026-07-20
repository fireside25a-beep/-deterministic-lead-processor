CC ?= cc
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L -Iinclude -Ithird_party
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
LDFLAGS ?=
CURL_LIBS ?= -lcurl
CJSON_LIBS ?= $(shell sh -c 'if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libcjson 2>/dev/null; then pkg-config --libs libcjson; elif command -v ldconfig >/dev/null 2>&1; then p=$$(ldconfig -p 2>/dev/null | awk '\''/libcjson\.so\.1/{print $$NF; exit}'\''); if [ -n "$$p" ]; then printf "%s" "$$p"; else printf "%s" "-lcjson"; fi; else printf "%s" "-lcjson"; fi')
LDLIBS ?= $(CURL_LIBS) $(CJSON_LIBS) -lm

BUILD_DIR := build
APP := $(BUILD_DIR)/lead_processor
CORE_SRCS := src/lead.c src/csv_reader.c src/config.c src/lead_rules.c src/json_validator.c src/lead_router.c src/model_client.c src/lead_processor.c
CORE_OBJS := $(CORE_SRCS:src/%.c=$(BUILD_DIR)/%.o)
APP_OBJ := $(BUILD_DIR)/main.o
TEST_NAMES := test_lead test_config test_csv test_area test_pricing test_schema test_router test_model_client test_pipeline test_demo_csv
TEST_BINS := $(TEST_NAMES:%=$(BUILD_DIR)/%)

.PHONY: all clean test smoke validate sanitize print-config

all: $(APP) $(TEST_BINS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(APP): $(APP_OBJ) $(CORE_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@ $(LDLIBS)

$(BUILD_DIR)/test_%: tests/test_%.c $(CORE_OBJS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@ $(LDLIBS)

test: $(TEST_BINS)
	@set -eu; for test_binary in $(TEST_BINS); do ./$$test_binary; done

smoke: $(APP)
	@set -eu; \
	  ./$(APP) --config config/default_config.json --mock fixtures/mock_model_responses.jsonl workbelt-leads-demo.csv > $(BUILD_DIR)/mock-output.jsonl; \
	  test "$$(wc -l < $(BUILD_DIR)/mock-output.jsonl | tr -d ' ')" = "20"; \
	  python3 tests/validate_jsonl.py $(BUILD_DIR)/mock-output.jsonl 20; \
	  ./$(APP) --mock fixtures/mock_model_responses.jsonl --message "Need epoxy coating for a 20 by 30 foot garage next week" > $(BUILD_DIR)/single-output.jsonl; \
	  python3 tests/validate_jsonl.py $(BUILD_DIR)/single-output.jsonl 1; \
	  ./$(APP) --help | grep -Fq "Usage:"; \
	  if ./$(APP) --config fixtures/invalid_config_fractional.json --mock fixtures/mock_model_responses.jsonl --message "test" >$(BUILD_DIR)/bad-config.out 2>$(BUILD_DIR)/bad-config.err; then \
	    echo "invalid config test unexpectedly succeeded" >&2; exit 1; \
	  fi; \
	  grep -Fq "max_model_retries" $(BUILD_DIR)/bad-config.err; \
	  if env -u LEAD_API_KEY ./$(APP) --message "test" >$(BUILD_DIR)/missing-key.out 2>$(BUILD_DIR)/missing-key.err; then \
	    echo "missing API key test unexpectedly succeeded" >&2; exit 1; \
	  fi; \
	  grep -Fq "LEAD_API_KEY is not set" $(BUILD_DIR)/missing-key.err; \
	  python3 tests/test_live_http.py; \
	  echo "smoke: PASS"

validate:
	$(MAKE) clean
	$(MAKE) all test smoke CC="$(CC)"

sanitize:
	$(MAKE) clean
	$(MAKE) all test smoke \
	  CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
	  LDFLAGS='-fsanitize=address,undefined'

print-config:
	@printf 'CC=%s\nCFLAGS=%s\nCJSON_LIBS=%s\nCURL_LIBS=%s\n' "$(CC)" "$(CFLAGS)" "$(CJSON_LIBS)" "$(CURL_LIBS)"

clean:
	rm -rf $(BUILD_DIR)
