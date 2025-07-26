#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "steg.h"
#include "aids.h"

static const char *steg__g_failure_reason;

#define BYTE_SIZE 8

static bool steg__validate_compression(int compression) {
    bool is_in_range = (compression > 0 && compression <= BYTE_SIZE);
    bool is_power_of_two = (compression & (compression - 1)) == 0;

    return is_in_range && is_power_of_two;
}

static void steg__util_hide_lsbn(unsigned char *ptr, unsigned char byte, size_t count) {
    for (size_t i = 0; i < BYTE_SIZE / count; i++) {
        unsigned char ptr_byte = ptr[i];
        for (size_t j = 0; j < count; j++) {
            unsigned char mask = 0b00000001 << (count - j - 1);
            ptr_byte = ptr_byte & ~mask;
        }
        for (size_t j = 0; j < count; j++) {
            unsigned char bit = ((byte >> (BYTE_SIZE - i * count - j - 1)) & 0b00000001);
            ptr_byte = ptr_byte | (bit << (count - j - 1));
        }
        ptr[i] = ptr_byte;
    }
}

static void steg__util_show_lsbn(const unsigned char *ptr, unsigned char *byte, size_t count) {
    for (size_t i = 0; i < BYTE_SIZE / count; i++) {
        unsigned char ptr_byte = ptr[i];
        for (size_t j = 0; j < count; j++) {
            unsigned char mask = 0b00000001 << (count - j - 1);
            unsigned char bit = ((ptr_byte & mask) >> (count - j - 1)) & 0b00000001;
            *byte |= (bit << (BYTE_SIZE - i * count - j - 1));
        }
    }
}

STEGDEF Steg_Result steg_hide_lsb(uint8_t *bytes, size_t bytes_length,
                                  const uint8_t *payload,
                                  size_t payload_length, int compression) {
    Steg_Result result = STEG_OK;

    if (!steg__validate_compression(compression)) {
        steg__g_failure_reason = "Invalid compression value";
        return_defer(STEG_ERR);
    }

    size_t byte_stride = BYTE_SIZE / compression;
    if (payload_length * byte_stride + sizeof(size_t) * byte_stride > bytes_length) {
        steg__g_failure_reason = "Data is too big for the cover image";
        return_defer(STEG_ERR);
    }

    for (size_t i = 0; i < sizeof(size_t); i++) {
        unsigned char length_byte = ((unsigned char*)&payload_length)[i];
        steg__util_hide_lsbn(bytes + i * byte_stride, length_byte, compression);
    }

    for (size_t i = 0; i < payload_length; i++) {
        steg__util_hide_lsbn(bytes + i * byte_stride + sizeof(size_t) * byte_stride, payload[i], compression);
    }

defer:
    return result;
}

STEGDEF Steg_Result steg_show_lsb(uint8_t *bytes, size_t bytes_length,
                                  uint8_t **message,
                                  size_t *message_length, int compression) {
    Steg_Result result = STEG_OK;

    if (!steg__validate_compression(compression)) {
        steg__g_failure_reason = "Invalid compression value";
        return_defer(STEG_ERR);
    }

    size_t byte_stride = BYTE_SIZE / compression;

    for (size_t i = 0; i < sizeof(size_t); i++) {
        steg__util_show_lsbn(bytes + i * byte_stride, ((unsigned char*)message_length) + i, compression);
    }

    if (*message_length * byte_stride + sizeof(size_t) * byte_stride > bytes_length) {
        steg__g_failure_reason = "Data is too big for the cover image";
        return_defer(STEG_ERR);
    }

    *message = AIDS_REALLOC(NULL, (*message_length + 1) * sizeof(unsigned char));
    if (message == NULL) {
        steg__g_failure_reason = "Memory allocation failed";
        return_defer(STEG_ERR);
    }
    memset(*message, 0, (*message_length + 1) * sizeof(unsigned char));

    for (size_t i = 0; i < *message_length; i++) {
        steg__util_show_lsbn(bytes + i * byte_stride + sizeof(size_t) * byte_stride, (*message) + i, compression);
    }

defer:
    return result;
}

STEGDEF const char *steg_failure_reason(void) { return steg__g_failure_reason; }
