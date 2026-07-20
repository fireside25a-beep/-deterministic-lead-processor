#include "csv_reader.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **fields;
    size_t count;
    size_t capacity;
} CsvRow;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} TextBuffer;

static void set_error(char *error, size_t error_size, const char *message) {
    if ((error != NULL) && (error_size > 0U)) {
        (void)lead_copy_string(error, error_size, message);
    }
}

static void set_error_errno(char *error, size_t error_size, const char *prefix) {
    char buffer[LEAD_ERROR_MAX];
    (void)snprintf(buffer, sizeof(buffer), "%s: %s", prefix, strerror(errno));
    set_error(error, error_size, buffer);
}

static void csv_row_init(CsvRow *row) {
    row->fields = NULL;
    row->count = 0U;
    row->capacity = 0U;
}

static void csv_row_clear(CsvRow *row) {
    size_t i = 0U;
    if (row == NULL) {
        return;
    }
    for (i = 0U; i < row->count; ++i) {
        free(row->fields[i]);
    }
    free(row->fields);
    csv_row_init(row);
}

static bool csv_row_add(CsvRow *row, const char *data, size_t length) {
    char **grown = NULL;
    char *copy = NULL;
    size_t new_capacity = 0U;

    if ((row == NULL) || (data == NULL) || (length > CSV_MAX_FIELD_BYTES)) {
        return false;
    }
    if (row->count >= CSV_MAX_COLUMNS) {
        return false;
    }
    if (row->count == row->capacity) {
        new_capacity = (row->capacity == 0U) ? 16U : row->capacity * 2U;
        grown = (char **)realloc(row->fields, new_capacity * sizeof(*grown));
        if (grown == NULL) {
            return false;
        }
        row->fields = grown;
        row->capacity = new_capacity;
    }
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        return false;
    }
    memcpy(copy, data, length);
    copy[length] = '\0';
    row->fields[row->count++] = copy;
    return true;
}

static void text_buffer_init(TextBuffer *buffer) {
    buffer->data = NULL;
    buffer->length = 0U;
    buffer->capacity = 0U;
}

static void text_buffer_clear(TextBuffer *buffer) {
    free(buffer->data);
    text_buffer_init(buffer);
}

static bool text_buffer_append(TextBuffer *buffer, char ch) {
    char *grown = NULL;
    size_t new_capacity = 0U;

    if ((buffer == NULL) || (buffer->length >= CSV_MAX_FIELD_BYTES)) {
        return false;
    }
    if (buffer->length + 1U >= buffer->capacity) {
        new_capacity = (buffer->capacity == 0U) ? 128U : buffer->capacity * 2U;
        if (new_capacity > CSV_MAX_FIELD_BYTES + 1U) {
            new_capacity = CSV_MAX_FIELD_BYTES + 1U;
        }
        grown = (char *)realloc(buffer->data, new_capacity);
        if (grown == NULL) {
            return false;
        }
        buffer->data = grown;
        buffer->capacity = new_capacity;
    }
    buffer->data[buffer->length++] = ch;
    buffer->data[buffer->length] = '\0';
    return true;
}

static bool finish_field(CsvRow *row, TextBuffer *field) {
    const char *data = (field->data == NULL) ? "" : field->data;
    if (!csv_row_add(row, data, field->length)) {
        return false;
    }
    field->length = 0U;
    if (field->data != NULL) {
        field->data[0] = '\0';
    }
    return true;
}

static char *read_file(const char *path, size_t *length, char *error, size_t error_size) {
    FILE *file = NULL;
    char *buffer = NULL;
    long file_size = 0L;
    size_t read_count = 0U;

    if ((path == NULL) || (length == NULL)) {
        set_error(error, error_size, "invalid CSV path");
        return NULL;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        set_error_errno(error, error_size, "cannot open CSV");
        return NULL;
    }
    if ((fseek(file, 0L, SEEK_END) != 0) || ((file_size = ftell(file)) < 0L) ||
        (fseek(file, 0L, SEEK_SET) != 0)) {
        set_error_errno(error, error_size, "cannot inspect CSV");
        (void)fclose(file);
        return NULL;
    }
    if ((unsigned long)file_size > (unsigned long)CSV_MAX_FILE_BYTES) {
        set_error(error, error_size, "CSV exceeds the 10 MiB input limit");
        (void)fclose(file);
        return NULL;
    }
    buffer = (char *)malloc((size_t)file_size + 1U);
    if (buffer == NULL) {
        set_error(error, error_size, "out of memory reading CSV");
        (void)fclose(file);
        return NULL;
    }
    read_count = fread(buffer, 1U, (size_t)file_size, file);
    if ((read_count != (size_t)file_size) || ferror(file)) {
        set_error_errno(error, error_size, "cannot read CSV");
        free(buffer);
        (void)fclose(file);
        return NULL;
    }
    buffer[read_count] = '\0';
    *length = read_count;
    (void)fclose(file);
    return buffer;
}

static char detect_delimiter(const char *data, size_t length) {
    size_t i = 0U;
    size_t commas = 0U;
    size_t semicolons = 0U;
    bool quoted = false;

    for (i = 0U; i < length; ++i) {
        const char ch = data[i];
        if (ch == '"') {
            if (quoted && (i + 1U < length) && (data[i + 1U] == '"')) {
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (!quoted && ((ch == '\n') || (ch == '\r'))) {
            break;
        } else if (!quoted && (ch == ',')) {
            ++commas;
        } else if (!quoted && (ch == ';')) {
            ++semicolons;
        }
    }
    return (semicolons > commas) ? ';' : ',';
}

static int header_index(const CsvRow *header, const char *name) {
    size_t i = 0U;
    if ((header == NULL) || (name == NULL)) {
        return -1;
    }
    for (i = 0U; i < header->count; ++i) {
        if (strcmp(header->fields[i], name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static const char *field_at(const CsvRow *row, int index) {
    if ((row == NULL) || (index < 0) || ((size_t)index >= row->count)) {
        return "";
    }
    return row->fields[index];
}

static bool assign_field(char *dst, size_t dst_size, const CsvRow *row, int index) {
    return lead_copy_string(dst, dst_size, field_at(row, index));
}

typedef struct {
    int lead_id;
    int channel;
    int customer_name;
    int city;
    int phone;
    int email;
    int source;
    int message;
    int expected_fit;
    int expected_space;
    int expected_sqft;
    int expected_urgency;
    int expected_estimate;
    int expected_next_step;
    int expected_draft_reply;
} HeaderMap;

static bool build_header_map(const CsvRow *header, HeaderMap *map, char *error, size_t error_size) {
    memset(map, 0xFF, sizeof(*map));
    map->lead_id = header_index(header, "lead_id");
    map->channel = header_index(header, "channel");
    map->customer_name = header_index(header, "customer_name");
    map->city = header_index(header, "city");
    map->phone = header_index(header, "phone");
    map->email = header_index(header, "email");
    map->source = header_index(header, "source");
    map->message = header_index(header, "message");
    map->expected_fit = header_index(header, "expected_fit");
    map->expected_space = header_index(header, "expected_space");
    map->expected_sqft = header_index(header, "expected_sqft");
    map->expected_urgency = header_index(header, "expected_urgency");
    map->expected_estimate = header_index(header, "expected_estimate");
    map->expected_next_step = header_index(header, "expected_next_step");
    map->expected_draft_reply = header_index(header, "expected_draft_reply");
    if ((map->lead_id < 0) || (map->message < 0)) {
        set_error(error, error_size, "CSV requires lead_id and message headers");
        return false;
    }
    return true;
}

static bool row_is_empty(const CsvRow *row) {
    size_t i = 0U;
    for (i = 0U; i < row->count; ++i) {
        if (row->fields[i][0] != '\0') {
            return false;
        }
    }
    return true;
}

static bool append_lead(const CsvRow *row, const HeaderMap *map, LeadArray *out,
                        size_t row_number, char *error, size_t error_size) {
    Lead lead;
    char message[LEAD_ERROR_MAX];
    bool ok = true;

    if (row_is_empty(row)) {
        return true;
    }
    lead_init(&lead);
    ok = assign_field(lead.lead_id, sizeof(lead.lead_id), row, map->lead_id) && ok;
    ok = assign_field(lead.channel, sizeof(lead.channel), row, map->channel) && ok;
    ok = assign_field(lead.customer_name, sizeof(lead.customer_name), row, map->customer_name) && ok;
    ok = assign_field(lead.city, sizeof(lead.city), row, map->city) && ok;
    ok = assign_field(lead.phone, sizeof(lead.phone), row, map->phone) && ok;
    ok = assign_field(lead.email, sizeof(lead.email), row, map->email) && ok;
    ok = assign_field(lead.source, sizeof(lead.source), row, map->source) && ok;
    ok = assign_field(lead.message, sizeof(lead.message), row, map->message) && ok;
    ok = assign_field(lead.expected_fit, sizeof(lead.expected_fit), row, map->expected_fit) && ok;
    ok = assign_field(lead.expected_space, sizeof(lead.expected_space), row, map->expected_space) && ok;
    ok = assign_field(lead.expected_sqft, sizeof(lead.expected_sqft), row, map->expected_sqft) && ok;
    ok = assign_field(lead.expected_urgency, sizeof(lead.expected_urgency), row, map->expected_urgency) && ok;
    ok = assign_field(lead.expected_estimate, sizeof(lead.expected_estimate), row, map->expected_estimate) && ok;
    ok = assign_field(lead.expected_next_step, sizeof(lead.expected_next_step), row, map->expected_next_step) && ok;
    ok = assign_field(lead.expected_draft_reply, sizeof(lead.expected_draft_reply), row, map->expected_draft_reply) && ok;
    if (!ok) {
        (void)snprintf(message, sizeof(message), "CSV row %zu contains a field that exceeds its destination limit", row_number);
        set_error(error, error_size, message);
        return false;
    }
    lead_normalize(&lead);
    if ((lead.lead_id[0] == '\0') || (lead.message[0] == '\0')) {
        (void)snprintf(message, sizeof(message), "CSV row %zu has an empty lead_id or message", row_number);
        set_error(error, error_size, message);
        return false;
    }
    if (!lead_array_push(out, &lead)) {
        set_error(error, error_size, "out of memory storing CSV leads");
        return false;
    }
    return true;
}

bool csv_read_leads(const char *path, LeadArray *out, char *error, size_t error_size) {
    char *data = NULL;
    size_t length = 0U;
    size_t position = 0U;
    size_t row_number = 1U;
    char delimiter = ',';
    bool quoted = false;
    bool have_header = false;
    CsvRow row;
    TextBuffer field;
    HeaderMap map;

    if (out == NULL) {
        set_error(error, error_size, "output lead array is null");
        return false;
    }
    lead_array_init(out);
    csv_row_init(&row);
    text_buffer_init(&field);
    data = read_file(path, &length, error, error_size);
    if (data == NULL) {
        return false;
    }
    if ((length >= 3U) && ((unsigned char)data[0] == 0xEFU) &&
        ((unsigned char)data[1] == 0xBBU) && ((unsigned char)data[2] == 0xBFU)) {
        position = 3U;
    }
    delimiter = detect_delimiter(data + position, length - position);

    while (position <= length) {
        const bool at_end = (position == length);
        const char ch = at_end ? '\n' : data[position];

        if (quoted) {
            if (at_end) {
                set_error(error, error_size, "CSV ends inside a quoted field");
                goto fail;
            }
            if (ch == '"') {
                if ((position + 1U < length) && (data[position + 1U] == '"')) {
                    if (!text_buffer_append(&field, '"')) {
                        set_error(error, error_size, "CSV field exceeds limit or memory is exhausted");
                        goto fail;
                    }
                    position += 2U;
                    continue;
                }
                quoted = false;
                ++position;
                continue;
            }
            if (!text_buffer_append(&field, ch)) {
                set_error(error, error_size, "CSV field exceeds limit or memory is exhausted");
                goto fail;
            }
            ++position;
            continue;
        }

        if ((ch == '"') && (field.length == 0U)) {
            quoted = true;
            ++position;
            continue;
        }
        if (ch == delimiter) {
            if (!finish_field(&row, &field)) {
                set_error(error, error_size, "CSV has too many columns or memory is exhausted");
                goto fail;
            }
            ++position;
            continue;
        }
        if ((ch == '\r') || (ch == '\n') || at_end) {
            if (!finish_field(&row, &field)) {
                set_error(error, error_size, "CSV has too many columns or memory is exhausted");
                goto fail;
            }
            if (!have_header) {
                if (row_is_empty(&row)) {
                    set_error(error, error_size, "CSV header row is empty");
                    goto fail;
                }
                if (!build_header_map(&row, &map, error, error_size)) {
                    goto fail;
                }
                have_header = true;
            } else if (!append_lead(&row, &map, out, row_number, error, error_size)) {
                goto fail;
            }
            csv_row_clear(&row);
            csv_row_init(&row);
            ++row_number;
            if (!at_end && (ch == '\r') && (position + 1U < length) && (data[position + 1U] == '\n')) {
                position += 2U;
            } else {
                ++position;
            }
            continue;
        }
        if (!text_buffer_append(&field, ch)) {
            set_error(error, error_size, "CSV field exceeds limit or memory is exhausted");
            goto fail;
        }
        ++position;
    }

    free(data);
    csv_row_clear(&row);
    text_buffer_clear(&field);
    if (!have_header) {
        set_error(error, error_size, "CSV has no header");
        lead_array_free(out);
        return false;
    }
    if (out->count == 0U) {
        set_error(error, error_size, "CSV has no lead rows");
        lead_array_free(out);
        return false;
    }
    return true;

fail:
    free(data);
    csv_row_clear(&row);
    text_buffer_clear(&field);
    lead_array_free(out);
    return false;
}
