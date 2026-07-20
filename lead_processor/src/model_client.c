#include "model_client.h"
#include "cJSON.h"

#include <curl/curl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOCK_FILE_MAX_BYTES (8U * 1024U * 1024U)

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    size_t limit;
    bool overflow;
} HttpBuffer;

static void set_error(char *error, size_t error_size, const char *message) {
    if ((error != NULL) && (error_size > 0U)) {
        (void)lead_copy_string(error, error_size, message);
    }
}

static char *duplicate_string(const char *text) {
    size_t length = 0U;
    char *copy = NULL;
    if (text == NULL) {
        return NULL;
    }
    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy != NULL) {
        memcpy(copy, text, length + 1U);
    }
    return copy;
}

void model_client_init(ModelClient *client) {
    if (client != NULL) {
        memset(client, 0, sizeof(*client));
    }
}

void model_client_destroy(ModelClient *client) {
    size_t i = 0U;
    if (client == NULL) {
        return;
    }
    for (i = 0U; i < client->mock_count; ++i) {
        free(client->mock_entries[i].raw_response);
    }
    free(client->mock_entries);
    memset(client->api_key, 0, sizeof(client->api_key));
    if (client->curl_initialized) {
        curl_global_cleanup();
    }
    model_client_init(client);
}

static bool append_mock_entry(ModelClient *client, const char *lead_id, char *raw) {
    MockModelEntry *grown = NULL;
    size_t new_capacity = 0U;
    if ((client == NULL) || (lead_id == NULL) || (raw == NULL)) {
        return false;
    }
    if (client->mock_count == client->mock_capacity) {
        new_capacity = (client->mock_capacity == 0U) ? 32U : client->mock_capacity * 2U;
        grown = (MockModelEntry *)realloc(client->mock_entries, new_capacity * sizeof(*grown));
        if (grown == NULL) {
            return false;
        }
        client->mock_entries = grown;
        client->mock_capacity = new_capacity;
    }
    memset(&client->mock_entries[client->mock_count], 0, sizeof(MockModelEntry));
    if (!lead_copy_string(client->mock_entries[client->mock_count].lead_id,
                          sizeof(client->mock_entries[client->mock_count].lead_id), lead_id)) {
        return false;
    }
    client->mock_entries[client->mock_count].raw_response = raw;
    ++client->mock_count;
    return true;
}

bool model_client_open_mock(ModelClient *client, const char *path,
                            char *error, size_t error_size) {
    FILE *file = NULL;
    char *line = NULL;
    size_t capacity = 0U;
    ssize_t length = 0;
    size_t total = 0U;

    if ((client == NULL) || (path == NULL)) {
        set_error(error, error_size, "invalid mock client arguments");
        return false;
    }
    model_client_destroy(client);
    file = fopen(path, "rb");
    if (file == NULL) {
        char message[LEAD_ERROR_MAX];
        (void)snprintf(message, sizeof(message), "cannot open mock file: %s", strerror(errno));
        set_error(error, error_size, message);
        return false;
    }
    while ((length = getline(&line, &capacity, file)) >= 0) {
        cJSON *entry = NULL;
        cJSON *lead_id = NULL;
        cJSON *raw = NULL;
        cJSON *response = NULL;
        char *stored = NULL;
        const char *end = NULL;

        total += (size_t)length;
        if (total > MOCK_FILE_MAX_BYTES) {
            set_error(error, error_size, "mock file exceeds 8 MiB limit");
            goto fail;
        }
        while ((length > 0) && ((line[length - 1] == '\n') || (line[length - 1] == '\r'))) {
            line[--length] = '\0';
        }
        if (length == 0) {
            continue;
        }
        entry = cJSON_ParseWithOpts(line, &end, 1);
        if ((entry == NULL) || !cJSON_IsObject(entry)) {
            set_error(error, error_size, "each mock JSONL line must be one JSON object");
            cJSON_Delete(entry);
            goto fail;
        }
        lead_id = cJSON_GetObjectItemCaseSensitive(entry, "lead_id");
        raw = cJSON_GetObjectItemCaseSensitive(entry, "raw");
        response = cJSON_GetObjectItemCaseSensitive(entry, "response");
        if (!cJSON_IsString(lead_id) || (lead_id->valuestring == NULL)) {
            set_error(error, error_size, "mock entry requires string lead_id");
            cJSON_Delete(entry);
            goto fail;
        }
        if (cJSON_IsString(raw) && (raw->valuestring != NULL)) {
            stored = duplicate_string(raw->valuestring);
        } else if (cJSON_IsObject(response)) {
            stored = cJSON_PrintUnformatted(response);
        } else {
            set_error(error, error_size, "mock entry requires raw string or response object");
            cJSON_Delete(entry);
            goto fail;
        }
        if ((stored == NULL) || !append_mock_entry(client, lead_id->valuestring, stored)) {
            free(stored);
            set_error(error, error_size, "out of memory loading mock entries");
            cJSON_Delete(entry);
            goto fail;
        }
        cJSON_Delete(entry);
    }
    free(line);
    (void)fclose(file);
    if (client->mock_count == 0U) {
        set_error(error, error_size, "mock file contains no entries");
        model_client_destroy(client);
        return false;
    }
    client->mock_mode = true;
    return true;

fail:
    free(line);
    (void)fclose(file);
    model_client_destroy(client);
    return false;
}

bool model_client_open_live_from_env(ModelClient *client, const LeadConfig *config,
                                     char *error, size_t error_size) {
    const char *key = getenv("LEAD_API_KEY");
    const char *base = getenv("LEAD_API_BASE_URL");
    const char *model = getenv("LEAD_API_MODEL");

    if ((client == NULL) || (config == NULL)) {
        set_error(error, error_size, "invalid live model client arguments");
        return false;
    }
    if ((key == NULL) || (key[0] == '\0')) {
        set_error(error, error_size, "LEAD_API_KEY is not set; use --mock for offline mode");
        return false;
    }
    if (base == NULL || base[0] == '\0') {
        base = "https://api.openai.com/v1";
    }
    if (model == NULL || model[0] == '\0') {
        model = "gpt-4.1-mini";
    }
    model_client_destroy(client);
    if (!lead_copy_string(client->api_key, sizeof(client->api_key), key) ||
        !lead_copy_string(client->base_url, sizeof(client->base_url), base) ||
        !lead_copy_string(client->model, sizeof(client->model), model)) {
        set_error(error, error_size, "API environment value exceeds its safe limit");
        model_client_destroy(client);
        return false;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        set_error(error, error_size, "curl_global_init failed");
        model_client_destroy(client);
        return false;
    }
    client->curl_initialized = true;
    client->timeout_seconds = config->request_timeout_seconds;
    client->max_response_bytes = config->max_response_bytes;
    client->mock_mode = false;
    return true;
}

static size_t http_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    HttpBuffer *buffer = (HttpBuffer *)userdata;
    size_t bytes = size * nmemb;
    size_t needed = 0U;
    size_t new_capacity = 0U;
    char *grown = NULL;

    if ((buffer == NULL) || (size != 0U && bytes / size != nmemb)) {
        return 0U;
    }
    if (bytes > buffer->limit - buffer->length) {
        buffer->overflow = true;
        return 0U;
    }
    needed = buffer->length + bytes + 1U;
    if (needed > buffer->capacity) {
        new_capacity = (buffer->capacity == 0U) ? 4096U : buffer->capacity;
        while (new_capacity < needed) {
            if (new_capacity > buffer->limit / 2U) {
                new_capacity = buffer->limit + 1U;
                break;
            }
            new_capacity *= 2U;
        }
        grown = (char *)realloc(buffer->data, new_capacity);
        if (grown == NULL) {
            return 0U;
        }
        buffer->data = grown;
        buffer->capacity = new_capacity;
    }
    memcpy(buffer->data + buffer->length, ptr, bytes);
    buffer->length += bytes;
    buffer->data[buffer->length] = '\0';
    return bytes;
}

static char *build_user_content(const Lead *lead) {
    cJSON *object = cJSON_CreateObject();
    char *json = NULL;
    if ((object == NULL) ||
        (cJSON_AddStringToObject(object, "lead_id", lead->lead_id) == NULL) ||
        (cJSON_AddStringToObject(object, "channel", lead->channel) == NULL) ||
        (cJSON_AddStringToObject(object, "customer_name", lead->customer_name) == NULL) ||
        (cJSON_AddStringToObject(object, "city", lead->city) == NULL) ||
        (cJSON_AddStringToObject(object, "source", lead->source) == NULL) ||
        (cJSON_AddStringToObject(object, "message", lead->message) == NULL)) {
        cJSON_Delete(object);
        return NULL;
    }
    json = cJSON_PrintUnformatted(object);
    cJSON_Delete(object);
    return json;
}

static char *build_request_body(const ModelClient *client, const Lead *lead) {
    static const char *system_prompt =
        "You classify floor-coating leads. Return exactly one JSON object with keys: "
        "fit(boolean), fitReason(string), space(string), sqft(number or null), inArea(boolean), "
        "urgency(low|medium|high), summary(string), estimate(string or null), nextStep(string), "
        "draftReply(string). Interpret English and French. Do not infer a garage from 'how much?' "
        "or an emoji without conversation history. C code will verify area, service region and pricing.";
    cJSON *root = NULL;
    cJSON *messages = NULL;
    cJSON *system = NULL;
    cJSON *user = NULL;
    cJSON *format = NULL;
    char *user_content = NULL;
    char *body = NULL;

    root = cJSON_CreateObject();
    messages = cJSON_CreateArray();
    system = cJSON_CreateObject();
    user = cJSON_CreateObject();
    format = cJSON_CreateObject();
    user_content = build_user_content(lead);
    if ((root == NULL) || (messages == NULL) || (system == NULL) || (user == NULL) ||
        (format == NULL) || (user_content == NULL)) {
        goto done;
    }
    if ((cJSON_AddStringToObject(root, "model", client->model) == NULL) ||
        (cJSON_AddStringToObject(system, "role", "system") == NULL) ||
        (cJSON_AddStringToObject(system, "content", system_prompt) == NULL) ||
        (cJSON_AddStringToObject(user, "role", "user") == NULL) ||
        (cJSON_AddStringToObject(user, "content", user_content) == NULL)) {
        goto done;
    }
    if (!cJSON_AddItemToArray(messages, system)) {
        goto done;
    }
    system = NULL;
    if (!cJSON_AddItemToArray(messages, user)) {
        goto done;
    }
    user = NULL;
    if (!cJSON_AddItemToObject(root, "messages", messages)) {
        goto done;
    }
    messages = NULL;
    if (cJSON_AddStringToObject(format, "type", "json_object") == NULL) {
        goto done;
    }
    if (!cJSON_AddItemToObject(root, "response_format", format)) {
        goto done;
    }
    format = NULL;
    body = cJSON_PrintUnformatted(root);

done:
    free(user_content);
    cJSON_Delete(system);
    cJSON_Delete(user);
    cJSON_Delete(messages);
    cJSON_Delete(format);
    cJSON_Delete(root);
    return body;
}

static bool build_endpoint(const char *base, char *endpoint, size_t endpoint_size) {
    size_t length = strlen(base);
    int written = 0;
    if (length >= strlen("/chat/completions") &&
        strcmp(base + length - strlen("/chat/completions"), "/chat/completions") == 0) {
        return lead_copy_string(endpoint, endpoint_size, base);
    }
    while ((length > 0U) && (base[length - 1U] == '/')) {
        --length;
    }
    written = snprintf(endpoint, endpoint_size, "%.*s/chat/completions", (int)length, base);
    return (written > 0) && ((size_t)written < endpoint_size);
}

static bool extract_chat_content(const char *http_json, char **content, char *error, size_t error_size) {
    cJSON *root = NULL;
    cJSON *choices = NULL;
    cJSON *first = NULL;
    cJSON *message = NULL;
    cJSON *value = NULL;
    const char *end = NULL;

    root = cJSON_ParseWithOpts(http_json, &end, 1);
    if ((root == NULL) || !cJSON_IsObject(root)) {
        set_error(error, error_size, "HTTP response is not valid JSON");
        cJSON_Delete(root);
        return false;
    }
    choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
    first = cJSON_GetArrayItem(choices, 0);
    message = cJSON_GetObjectItemCaseSensitive(first, "message");
    value = cJSON_GetObjectItemCaseSensitive(message, "content");
    if (!cJSON_IsArray(choices) || !cJSON_IsObject(first) || !cJSON_IsObject(message) ||
        !cJSON_IsString(value) || (value->valuestring == NULL)) {
        set_error(error, error_size, "HTTP response lacks choices[0].message.content");
        cJSON_Delete(root);
        return false;
    }
    *content = duplicate_string(value->valuestring);
    cJSON_Delete(root);
    if (*content == NULL) {
        set_error(error, error_size, "out of memory copying model content");
        return false;
    }
    return true;
}

static bool request_live(ModelClient *client, const Lead *lead, char **raw_response,
                         char *error, size_t error_size) {
    CURL *curl = NULL;
    CURLcode code;
    struct curl_slist *headers = NULL;
    char endpoint[1200];
    char authorization[2200];
    char *body = NULL;
    HttpBuffer response = {0};
    long status = 0L;
    bool ok = false;

    if (!build_endpoint(client->base_url, endpoint, sizeof(endpoint))) {
        set_error(error, error_size, "API endpoint is too long");
        return false;
    }
    body = build_request_body(client, lead);
    if (body == NULL) {
        set_error(error, error_size, "cannot build API request JSON");
        return false;
    }
    curl = curl_easy_init();
    if (curl == NULL) {
        set_error(error, error_size, "curl_easy_init failed");
        cJSON_free(body);
        return false;
    }
    (void)snprintf(authorization, sizeof(authorization), "Authorization: Bearer %s", client->api_key);
    headers = curl_slist_append(NULL, "Content-Type: application/json");
    if (headers == NULL) {
        set_error(error, error_size, "cannot allocate HTTP headers");
        goto done;
    }
    {
        struct curl_slist *grown_headers = curl_slist_append(headers, authorization);
        if (grown_headers == NULL) {
            set_error(error, error_size, "cannot allocate authorization header");
            goto done;
        }
        headers = grown_headers;
    }
    response.limit = client->max_response_bytes;
    curl_easy_setopt(curl, CURLOPT_URL, endpoint);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, client->timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, client->timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "lead-processor-c11/1.0");
    code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        if (response.overflow) {
            set_error(error, error_size, "HTTP response exceeded configured size limit");
        } else {
            char message_text[LEAD_ERROR_MAX];
            (void)snprintf(message_text, sizeof(message_text), "HTTP request failed: %s", curl_easy_strerror(code));
            set_error(error, error_size, message_text);
        }
        goto done;
    }
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if ((status < 200L) || (status >= 300L)) {
        char message_text[LEAD_ERROR_MAX];
        (void)snprintf(message_text, sizeof(message_text), "HTTP endpoint returned status %ld", status);
        set_error(error, error_size, message_text);
        goto done;
    }
    if ((response.data == NULL) || !extract_chat_content(response.data, raw_response, error, error_size)) {
        goto done;
    }
    ok = true;

done:
    memset(authorization, 0, sizeof(authorization));
    free(response.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    cJSON_free(body);
    return ok;
}

bool model_client_request(ModelClient *client, const Lead *lead,
                          char **raw_response, char *error, size_t error_size) {
    size_t i = 0U;
    if ((client == NULL) || (lead == NULL) || (raw_response == NULL)) {
        set_error(error, error_size, "invalid model request arguments");
        return false;
    }
    *raw_response = NULL;
    if (client->mock_mode) {
        for (i = 0U; i < client->mock_count; ++i) {
            MockModelEntry *entry = &client->mock_entries[i];
            if (!entry->consumed && strcmp(entry->lead_id, lead->lead_id) == 0) {
                entry->consumed = true;
                *raw_response = duplicate_string(entry->raw_response);
                if (*raw_response == NULL) {
                    set_error(error, error_size, "out of memory copying mock response");
                    return false;
                }
                return true;
            }
        }
        set_error(error, error_size, "no unused mock response matches lead_id");
        return false;
    }
    return request_live(client, lead, raw_response, error, error_size);
}
