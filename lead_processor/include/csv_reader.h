#ifndef LEAD_PROCESSOR_CSV_READER_H
#define LEAD_PROCESSOR_CSV_READER_H

#include "lead.h"

#include <stdbool.h>
#include <stddef.h>

#define CSV_MAX_FILE_BYTES (10U * 1024U * 1024U)
#define CSV_MAX_FIELD_BYTES 16384U
#define CSV_MAX_COLUMNS 64U
#define CSV_MAX_ROWS 100000U

bool csv_read_leads(const char *path, LeadArray *out, char *error, size_t error_size);

#endif
