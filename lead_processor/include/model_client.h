#ifndef LEAD_PROCESSOR_MODEL_CLIENT_H
#define LEAD_PROCESSOR_MODEL_CLIENT_H

#include "config.h"
#include "lead.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char lead_id[LEAD_ID_MAX];
    char *raw_response;
    bool consumed;
} MockModelEntry;

typedef struct {
    bool mock_mode;
    bool curl_initialized;
    char api_key[2048];
    char base_url[1024];
    char model[256];
    long timeout_seconds;
    size_t max_response_bytes;
    MockModelEntry *mock_entries;
    size_t mock_count;
    size_t mock_capacity;
} ModelClient;

void model_client_init(ModelClient *client);
void model_client_destroy(ModelClient *client);
bool model_client_open_mock(ModelClient *client, const char *path,
                            char *error, size_t error_size);
bool model_client_open_live_from_env(ModelClient *client, const LeadConfig *config,
                                     char *error, size_t error_size);
bool model_client_request(ModelClient *client, const Lead *lead,
                          char **raw_response, char *error, size_t error_size);

#endif
