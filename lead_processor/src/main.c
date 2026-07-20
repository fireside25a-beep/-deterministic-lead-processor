#include "config.h"
#include "csv_reader.h"
#include "json_validator.h"
#include "lead.h"
#include "lead_processor.h"
#include "lead_router.h"
#include "model_client.h"
#include "cJSON.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *mock_path;
    const char *config_path;
    const char *message;
    const char *csv_path;
    bool show_help;
} CliOptions;

static void print_usage(FILE *stream, const char *program) {
    (void)fprintf(stream,
                  "Usage:\n"
                  "  %s [--config FILE] [--mock JSONL] CSV_FILE\n"
                  "  %s [--config FILE] [--mock JSONL] --message TEXT\n"
                  "\n"
                  "Options:\n"
                  "  --mock FILE     Use offline JSONL model responses.\n"
                  "  --config FILE   Load business policy from a JSON file.\n"
                  "  --message TEXT  Process one message using lead_id 'single'.\n"
                  "  -h, --help      Show this help.\n"
                  "\n"
                  "Live mode reads LEAD_API_KEY, LEAD_API_BASE_URL, and LEAD_API_MODEL.\n"
                  "Output is one normalized JSON object per line (JSONL).\n",
                  program, program);
}

static bool take_value(int argc, char **argv, int *index, const char **value,
                       const char *option) {
    if ((*index + 1) >= argc) {
        (void)fprintf(stderr, "error: %s requires a value\n", option);
        return false;
    }
    ++(*index);
    *value = argv[*index];
    return true;
}

static bool parse_options(int argc, char **argv, CliOptions *options) {
    int i = 0;

    if ((argv == NULL) || (options == NULL)) {
        return false;
    }
    memset(options, 0, sizeof(*options));
    for (i = 1; i < argc; ++i) {
        const char *argument = argv[i];
        if ((strcmp(argument, "-h") == 0) || (strcmp(argument, "--help") == 0)) {
            options->show_help = true;
        } else if (strcmp(argument, "--mock") == 0) {
            if ((options->mock_path != NULL) ||
                !take_value(argc, argv, &i, &options->mock_path, "--mock")) {
                if (options->mock_path != NULL) {
                    (void)fprintf(stderr, "error: --mock may be supplied only once\n");
                }
                return false;
            }
        } else if (strcmp(argument, "--config") == 0) {
            if ((options->config_path != NULL) ||
                !take_value(argc, argv, &i, &options->config_path, "--config")) {
                if (options->config_path != NULL) {
                    (void)fprintf(stderr, "error: --config may be supplied only once\n");
                }
                return false;
            }
        } else if (strcmp(argument, "--message") == 0) {
            if ((options->message != NULL) ||
                !take_value(argc, argv, &i, &options->message, "--message")) {
                if (options->message != NULL) {
                    (void)fprintf(stderr, "error: --message may be supplied only once\n");
                }
                return false;
            }
        } else if (argument[0] == '-') {
            (void)fprintf(stderr, "error: unknown option: %s\n", argument);
            return false;
        } else if (options->csv_path == NULL) {
            options->csv_path = argument;
        } else {
            (void)fprintf(stderr, "error: only one CSV input may be supplied\n");
            return false;
        }
    }

    if (options->show_help) {
        return true;
    }
    if ((options->message == NULL) == (options->csv_path == NULL)) {
        (void)fprintf(stderr, "error: supply exactly one CSV file or --message TEXT\n");
        return false;
    }
    if ((options->message != NULL) && (options->message[0] == '\0')) {
        (void)fprintf(stderr, "error: --message cannot be empty\n");
        return false;
    }
    return true;
}

static bool emit_one(const Lead *lead, const LeadConfig *config, ModelClient *client) {
    LeadProcessOutcome outcome;
    char *json = NULL;

    if (!lead_process_one(lead, config, client, &outcome)) {
        (void)fprintf(stderr, "error: lead %s could not be processed\n", lead->lead_id);
        return false;
    }
    json = json_serialize_envelope(lead->lead_id, lead_route_name(outcome.route),
                                   outcome.model_attempts, &outcome.result);
    if (json == NULL) {
        (void)fprintf(stderr, "error: lead %s output could not be serialized\n", lead->lead_id);
        return false;
    }
    if (puts(json) == EOF) {
        (void)fprintf(stderr, "error: failed writing JSON output\n");
        cJSON_free(json);
        return false;
    }
    cJSON_free(json);
    if (outcome.manual_review) {
        (void)fprintf(stderr, "warning: lead %s routed to manual review after %u model attempt(s): %s\n",
                      lead->lead_id, outcome.model_attempts,
                      outcome.last_error[0] == '\0' ? "model retry exhaustion" : outcome.last_error);
    }
    return true;
}

static bool process_message(const char *message, const LeadConfig *config,
                            ModelClient *client) {
    Lead lead;

    lead_init(&lead);
    if (!lead_copy_string(lead.lead_id, sizeof(lead.lead_id), "single") ||
        !lead_copy_string(lead.channel, sizeof(lead.channel), "direct") ||
        !lead_copy_string(lead.source, sizeof(lead.source), "single-message") ||
        !lead_copy_string(lead.message, sizeof(lead.message), message)) {
        (void)fprintf(stderr, "error: single message exceeds the %u-byte safe limit\n",
                      (unsigned int)(LEAD_MESSAGE_MAX - 1U));
        return false;
    }
    lead_normalize(&lead);
    return emit_one(&lead, config, client);
}

static bool process_csv(const char *path, const LeadConfig *config,
                        ModelClient *client) {
    LeadArray leads;
    char error[LEAD_ERROR_MAX] = {0};
    size_t i = 0U;
    bool ok = true;

    lead_array_init(&leads);
    if (!csv_read_leads(path, &leads, error, sizeof(error))) {
        (void)fprintf(stderr, "error: %s\n", error);
        return false;
    }
    for (i = 0U; i < leads.count; ++i) {
        if (!emit_one(&leads.items[i], config, client)) {
            ok = false;
            break;
        }
    }
    lead_array_free(&leads);
    return ok;
}

int main(int argc, char **argv) {
    CliOptions options;
    LeadConfig config;
    ModelClient client;
    char error[LEAD_ERROR_MAX] = {0};
    bool ok = false;

    if (!parse_options(argc, argv, &options)) {
        print_usage(stderr, argv[0]);
        return 2;
    }
    if (options.show_help) {
        print_usage(stdout, argv[0]);
        return 0;
    }

    lead_config_defaults(&config);
    if ((options.config_path != NULL) &&
        !lead_config_load_json(options.config_path, &config, error, sizeof(error))) {
        (void)fprintf(stderr, "error: cannot load config: %s\n", error);
        return 2;
    }

    model_client_init(&client);
    if (options.mock_path != NULL) {
        if (!model_client_open_mock(&client, options.mock_path, error, sizeof(error))) {
            (void)fprintf(stderr, "error: cannot open mock model: %s\n", error);
            model_client_destroy(&client);
            return 2;
        }
    } else if (!model_client_open_live_from_env(&client, &config, error, sizeof(error))) {
        (void)fprintf(stderr, "error: cannot initialize live model client: %s\n", error);
        model_client_destroy(&client);
        return 2;
    }

    if (options.message != NULL) {
        ok = process_message(options.message, &config, &client);
    } else {
        ok = process_csv(options.csv_path, &config, &client);
    }
    model_client_destroy(&client);
    return ok ? 0 : 1;
}
