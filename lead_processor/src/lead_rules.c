#include "lead_rules.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ascii_lower_copy(char *dst, size_t dst_size, const char *src) {
    size_t i = 0U;
    if ((dst == NULL) || (dst_size == 0U)) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    for (i = 0U; (src[i] != '\0') && (i + 1U < dst_size); ++i) {
        unsigned char ch = (unsigned char)src[i];
        if ((ch >= 'A') && (ch <= 'Z')) {
            ch = (unsigned char)(ch + ('a' - 'A'));
        }
        dst[i] = (char)ch;
    }
    dst[i] = '\0';
}

static bool contains_ci(const char *text, const char *needle) {
    char lower_text[LEAD_MESSAGE_MAX];
    char lower_needle[CONFIG_KEYWORD_MAX];
    if ((text == NULL) || (needle == NULL) || (needle[0] == '\0')) {
        return false;
    }
    ascii_lower_copy(lower_text, sizeof(lower_text), text);
    ascii_lower_copy(lower_needle, sizeof(lower_needle), needle);
    return strstr(lower_text, lower_needle) != NULL;
}

static const char *skip_spaces(const char *cursor) {
    while ((*cursor != '\0') && isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    return cursor;
}

static bool suffix_starts(const char *cursor, const char *suffix) {
    return strncmp(cursor, suffix, strlen(suffix)) == 0;
}

static bool is_sqft_suffix(const char *cursor) {
    cursor = skip_spaces(cursor);
    return suffix_starts(cursor, "sq ft") || suffix_starts(cursor, "sq. ft") ||
           suffix_starts(cursor, "sqft") || suffix_starts(cursor, "square feet") ||
           suffix_starts(cursor, "square foot") || suffix_starts(cursor, "pieds carr") ||
           suffix_starts(cursor, "pi²") || suffix_starts(cursor, "pi2");
}

static bool is_sqm_suffix(const char *cursor) {
    cursor = skip_spaces(cursor);
    return suffix_starts(cursor, "mètres carr") || suffix_starts(cursor, "metres carr") ||
           suffix_starts(cursor, "square metres") || suffix_starts(cursor, "square meters") ||
           suffix_starts(cursor, "m²") || suffix_starts(cursor, "m2");
}

bool lead_rules_extract_area(const char *message, double *sqft_out) {
    char text[LEAD_MESSAGE_MAX];
    const char *cursor = NULL;

    if ((message == NULL) || (sqft_out == NULL)) {
        return false;
    }
    ascii_lower_copy(text, sizeof(text), message);
    cursor = text;
    while (*cursor != '\0') {
        char *after_first = NULL;
        double first = 0.0;

        if (!isdigit((unsigned char)*cursor) && (*cursor != '.')) {
            ++cursor;
            continue;
        }
        first = strtod(cursor, &after_first);
        if ((after_first == cursor) || (first <= 0.0) || !isfinite(first)) {
            ++cursor;
            continue;
        }
        if (is_sqft_suffix(after_first)) {
            *sqft_out = first;
            return true;
        }
        if (is_sqm_suffix(after_first)) {
            *sqft_out = first * 10.76391041671;
            return true;
        }

        {
            const char *between = skip_spaces(after_first);
            bool dimension_marker = false;
            if (suffix_starts(between, "by")) {
                between = skip_spaces(between + 2);
                dimension_marker = true;
            } else if ((*between == 'x') || ((unsigned char)*between == 0xD7U)) {
                between = skip_spaces(between + 1);
                dimension_marker = true;
            }
            if (dimension_marker) {
                char *after_second = NULL;
                double second = strtod(between, &after_second);
                const char *unit = skip_spaces(after_second);
                if ((after_second != between) && (second > 0.0) && isfinite(second) &&
                    (suffix_starts(unit, "feet") || suffix_starts(unit, "foot") ||
                     suffix_starts(unit, "ft"))) {
                    *sqft_out = first * second;
                    return true;
                }
            }
        }
        cursor = after_first;
    }
    return false;
}

bool lead_rules_is_spam(const char *message) {
    return contains_ci(message, "ranking on page 1") || contains_ci(message, "top 3 positions") ||
           contains_ci(message, "seo agency") || contains_ci(message, "strategy call") ||
           contains_ci(message, "guest post") || contains_ci(message, "backlinks");
}

bool lead_rules_is_wrong_specialty(const char *message) {
    return contains_ci(message, "repair roofs") || contains_ci(message, "roof leak") ||
           contains_ci(message, "need a plumber") || contains_ci(message, "kitchen sink") ||
           contains_ci(message, "stain fences") || contains_ci(message, "fence staining");
}

static bool contains_floor_context(const char *message) {
    return contains_ci(message, "garage") || contains_ci(message, "basement") ||
           contains_ci(message, "patio") || contains_ci(message, "driveway") ||
           contains_ci(message, "warehouse") || contains_ci(message, "floor") ||
           contains_ci(message, "plancher") || contains_ci(message, "epoxy") ||
           contains_ci(message, "polyaspartic") || contains_ci(message, "coating") ||
           contains_ci(message, "concrete");
}

bool lead_rules_is_insufficient(const char *message) {
    char lower[LEAD_MESSAGE_MAX];
    size_t letters_or_digits = 0U;
    size_t words = 0U;
    bool in_word = false;
    size_t i = 0U;

    if (message == NULL) {
        return true;
    }
    ascii_lower_copy(lower, sizeof(lower), message);
    for (i = 0U; lower[i] != '\0'; ++i) {
        const bool alnum = isalnum((unsigned char)lower[i]) != 0;
        if (alnum) {
            ++letters_or_digits;
            if (!in_word) {
                ++words;
                in_word = true;
            }
        } else {
            in_word = false;
        }
    }
    if (letters_or_digits == 0U) {
        return true;
    }
    if ((strstr(lower, "how much") != NULL) && !contains_floor_context(lower)) {
        return true;
    }
    return (words <= 2U) && !contains_floor_context(lower);
}

static bool city_matches(const char *city, const char *configured) {
    char a[LEAD_CITY_MAX];
    char b[LEAD_CITY_MAX];
    ascii_lower_copy(a, sizeof(a), city);
    ascii_lower_copy(b, sizeof(b), configured);
    return strcmp(a, b) == 0;
}

bool lead_rules_is_in_service_area(const Lead *lead, const LeadConfig *config) {
    size_t i = 0U;
    if ((lead == NULL) || (config == NULL)) {
        return false;
    }
    if (lead->city[0] != '\0') {
        for (i = 0U; i < config->service_area_city_count; ++i) {
            if (city_matches(lead->city, config->service_area_cities[i])) {
                return true;
            }
        }
        return false;
    }
    for (i = 0U; i < config->service_area_city_count; ++i) {
        if (contains_ci(lead->message, config->service_area_cities[i])) {
            return true;
        }
    }
    return config->unknown_city_in_area_default;
}

static double round_to_nearest_50(double value) {
    return floor((value + 25.0) / 50.0) * 50.0;
}

bool lead_rules_calculate_estimate(const LeadResult *result, const LeadConfig *config,
                                   char *estimate, size_t estimate_size) {
    const PriceRule *rule = NULL;
    double low = 0.0;
    double high = 0.0;
    int written = 0;

    if ((result == NULL) || (config == NULL) || (estimate == NULL) || (estimate_size == 0U) ||
        !result->fit || !result->in_area || !result->sqft_present || (result->sqft <= 0.0)) {
        return false;
    }
    rule = lead_config_find_price(config, result->space);
    if (rule == NULL) {
        return false;
    }
    low = round_to_nearest_50(result->sqft * rule->min_per_sqft);
    high = round_to_nearest_50(result->sqft * rule->max_per_sqft);
    if (strcmp(result->urgency, "high") == 0) {
        high += rule->urgent_max_adjustment;
    }
    written = snprintf(estimate, estimate_size, "$%.0f–$%.0f", low, high);
    return (written > 0) && ((size_t)written < estimate_size);
}

static bool parse_estimate_range(const char *text, double *low, double *high) {
    const char *cursor = text;
    char *end = NULL;
    double values[2] = {0.0, 0.0};
    size_t count = 0U;
    if ((text == NULL) || (low == NULL) || (high == NULL)) {
        return false;
    }
    while ((*cursor != '\0') && (count < 2U)) {
        while ((*cursor != '\0') && !isdigit((unsigned char)*cursor)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        values[count] = strtod(cursor, &end);
        if ((end == cursor) || !isfinite(values[count])) {
            return false;
        }
        ++count;
        cursor = end;
    }
    if ((count != 2U) || (values[0] < 0.0) || (values[1] < values[0])) {
        return false;
    }
    *low = values[0];
    *high = values[1];
    return true;
}

static bool estimate_is_close_to_policy(const char *model_estimate, const char *policy_estimate) {
    double model_low = 0.0;
    double model_high = 0.0;
    double policy_low = 0.0;
    double policy_high = 0.0;
    return parse_estimate_range(model_estimate, &model_low, &model_high) &&
           parse_estimate_range(policy_estimate, &policy_low, &policy_high) &&
           fabs(model_low - policy_low) <= 100.0 && fabs(model_high - policy_high) <= 100.0;
}

static bool urgency_is_valid(const char *urgency) {
    return (strcmp(urgency, "low") == 0) || (strcmp(urgency, "medium") == 0) ||
           (strcmp(urgency, "high") == 0);
}

static void infer_space(const char *message, char *space, size_t size) {
    if (contains_ci(message, "garage")) {
        (void)lead_copy_string(space, size, "garage");
    } else if (contains_ci(message, "basement") || contains_ci(message, "sous-sol")) {
        (void)lead_copy_string(space, size, "basement");
    } else if (contains_ci(message, "driveway")) {
        (void)lead_copy_string(space, size, "driveway");
    } else if (contains_ci(message, "patio")) {
        (void)lead_copy_string(space, size, "patio");
    } else if (contains_ci(message, "warehouse")) {
        (void)lead_copy_string(space, size, "commercial");
    } else if (contains_ci(message, "condo") || contains_ci(message, "living area")) {
        (void)lead_copy_string(space, size, "interior");
    }
}

void lead_rules_apply(const Lead *lead, const LeadConfig *config, LeadResult *result) {
    size_t i = 0U;
    double deterministic_sqft = 0.0;
    char estimate[LEAD_ESTIMATE_MAX];

    if ((lead == NULL) || (config == NULL) || (result == NULL)) {
        return;
    }

    if (!urgency_is_valid(result->urgency)) {
        (void)lead_copy_string(result->urgency, sizeof(result->urgency), "low");
    }
    for (i = 0U; i < config->high_urgency_keyword_count; ++i) {
        if (contains_ci(lead->message, config->high_urgency_keywords[i])) {
            (void)lead_copy_string(result->urgency, sizeof(result->urgency), "high");
            break;
        }
    }

    if (lead_rules_is_spam(lead->message)) {
        result->fit = false;
        result->space[0] = '\0';
        result->sqft_present = false;
        result->estimate_present = false;
        result->estimate[0] = '\0';
        (void)lead_copy_string(result->fit_reason, sizeof(result->fit_reason), "Unsolicited marketing or spam, not a customer flooring lead.");
        (void)lead_copy_string(result->next_step, sizeof(result->next_step), "Ignore / mark as spam");
        result->draft_reply[0] = '\0';
        return;
    }
    if (lead_rules_is_insufficient(lead->message)) {
        result->fit = false;
        result->space[0] = '\0';
        result->sqft_present = false;
        result->estimate_present = false;
        result->estimate[0] = '\0';
        (void)lead_copy_string(result->fit_reason, sizeof(result->fit_reason), "Insufficient message context to identify a flooring request or space type.");
        if (contains_ci(lead->message, "how much")) {
            (void)lead_copy_string(result->next_step, sizeof(result->next_step), "Ask what space and rough size");
        } else {
            (void)lead_copy_string(result->next_step, sizeof(result->next_step), "Wait or send a friendly prompt");
        }
        return;
    }
    if (lead_rules_is_wrong_specialty(lead->message)) {
        result->fit = false;
        result->space[0] = '\0';
        result->sqft_present = false;
        result->estimate_present = false;
        result->estimate[0] = '\0';
        (void)lead_copy_string(result->fit_reason, sizeof(result->fit_reason), "The request is outside the configured floor-coating specialty.");
        return;
    }

    if (result->space[0] == '\0') {
        infer_space(lead->message, result->space, sizeof(result->space));
    }
    if (lead_rules_extract_area(lead->message, &deterministic_sqft)) {
        result->sqft = deterministic_sqft;
        result->sqft_present = true;
    }
    result->in_area = lead_rules_is_in_service_area(lead, config);
    if (!result->in_area) {
        result->estimate_present = false;
        result->estimate[0] = '\0';
    } else if (lead_rules_calculate_estimate(result, config, estimate, sizeof(estimate))) {
        if (!result->estimate_present || !estimate_is_close_to_policy(result->estimate, estimate)) {
            (void)lead_copy_string(result->estimate, sizeof(result->estimate), estimate);
        }
        result->estimate_present = true;
    } else {
        result->estimate_present = false;
        result->estimate[0] = '\0';
    }
}
