#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complex.h>
#include <math.h>

#include "aids.h"
#include "fft.h"
#include "steg.h"

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
        steg__util_show_lsbn(bytes + i * byte_stride, ((unsigned char *)message_length) + i, compression);
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

#define ALPHA 0.1f // Adjust alpha as needed

STEGDEF Steg_Result steg_hide_fft(uint8_t *bytes, size_t width, size_t height,
                                  const uint8_t *payload,
                                  size_t payload_length) {
    complex double *x = NULL;
    size_t bytes_length = width * height * 3;

    Steg_Result result = STEG_OK;

    if ((width & (width - 1)) != 0 || (height & (height - 1)) != 0) {
        steg__g_failure_reason = "The input data is not a power of 2 shape.";
        return_defer(STEG_ERR);
    }

    size_t margin_y = 1, margin_x = 1;
    float alpha = ALPHA;

    size_t payload_width = (width - 2 * margin_x) / 2;
    size_t payload_height = height - 2 * margin_y;

    if (payload_length > payload_width * payload_height) {
        steg__g_failure_reason = "Payload is too large for the cover image";
        return_defer(STEG_ERR);
    }

    x = AIDS_REALLOC(NULL, bytes_length * sizeof(complex double));
    if (x == NULL) {
        steg__g_failure_reason = aids_failure_reason();
        return_defer(STEG_ERR);
    }

    for (size_t i = 0; i < bytes_length; i++) {
        x[i] = (double)bytes[i] / 255.0 + 0.0*I;
    }

    complex double *fft_r = AIDS_REALLOC(NULL, sizeof(complex double) * width * height);
    if (fft_r == NULL) {
        steg__g_failure_reason = aids_failure_reason();
        return_defer(STEG_ERR);
    }

    for (size_t i = 0; i < width * height; i++) {
        fft_r[i] = x[i * 3 + 0];
    }

    fft2d(fft_r, width, height, fft_r);

    for (size_t y = 0; y < payload_height; y++) {
        bool yes = true;

        for (size_t x = 0; x < payload_width; x++) {
            size_t index = y * payload_width + x;
            if (index >= payload_length) {
                yes = false;
                break;
            }

            double payload_value = (double)payload[index] / 255.0;

            size_t ty = margin_y + y;
            size_t tx = margin_x + x;
            size_t tx_mirror = width - 1 - tx;

            printf("Payload value at (%zu, %zu): %f\n", ty, tx, payload_value);

            fft_r[ty * width + tx] += payload_value * alpha;
            fft_r[ty * width + tx_mirror] += payload_value * alpha;
        }

        if (!yes) {
            break;
        }
    }

    ifft2d(fft_r, width, height, fft_r);

    for (size_t i = 0; i < width * height; i++) {
        bytes[i * 3 + 0] = fmin(fmax(creal(fft_r[i]) * 255.0, 0), 255);
    }

defer:
    if (x != NULL) AIDS_FREE(x);

    return result;
}

STEGDEF Steg_Result steg_show_fft(uint8_t *bytes, size_t width, size_t height,
                                  uint8_t **message, size_t *message_length) {
    complex double *x = NULL;
    size_t bytes_length = width * height * 3;

    Steg_Result result = STEG_OK;

    if ((width & (width - 1)) != 0 || (height & (height - 1)) != 0) {
        steg__g_failure_reason = "The input data is not a power of 2 shape.";
        return_defer(STEG_ERR);
    }

    size_t margin_y = 1, margin_x = 1;
    float alpha = ALPHA;

    size_t payload_width = (width - 2 * margin_x) / 2;
    size_t payload_height = height - 2 * margin_y;
    size_t max_payload_length = payload_width * payload_height;

    x = AIDS_REALLOC(NULL, bytes_length * sizeof(complex double));
    if (x == NULL) {
        steg__g_failure_reason = aids_failure_reason();
        return_defer(STEG_ERR);
    }

    for (size_t i = 0; i < bytes_length; i++) {
        x[i] = (double)bytes[i] / 255.0 + 0.0*I;
    }

    complex double *fft_r = AIDS_REALLOC(NULL, sizeof(complex double) * width * height);
    if (fft_r == NULL) {
        steg__g_failure_reason = aids_failure_reason();
        return_defer(STEG_ERR);
    }

    for (size_t i = 0; i < width * height; i++) {
        fft_r[i] = x[i * 3 + 0];
    }

    fft2d(fft_r, width, height, fft_r);

    *message_length = 0;
    *message = AIDS_REALLOC(NULL, (max_payload_length + 1) * sizeof(unsigned char));
    if (*message == NULL) {
        steg__g_failure_reason = aids_failure_reason();
        return_defer(STEG_ERR);
    }

    memset(*message, 0, (max_payload_length + 1) * sizeof(unsigned char));

    for (size_t y = 0; y < payload_height; y++) {
        bool yes = true;
        for (size_t x = 0; x < payload_width; x++) {
            size_t index = y * payload_width + x;

            printf("index: %zu, x: %zu, y: %zu\n", index, x, y);

            if (index >= 10) { yes = false; break; } // Limit to first 10 payload bytes for debugging

            size_t ty = margin_y + y;
            size_t tx = margin_x + x;
            size_t tx_mirror = width - 1 - tx;

            double payload_value = (creal(fft_r[ty * width + tx]) +
                                    creal(fft_r[ty * width + tx_mirror])) / (2.0 * alpha);

            printf("Payload value at (%zu, %zu): %f\n", ty, tx, payload_value);

            if (*message_length < max_payload_length) {
                (*message)[*message_length] = (unsigned char)(fmin(fmax(payload_value * 255.0, 0), 255));
                (*message_length)++;
            }
        }

        if (!yes) {
            break;
        }
    }

defer:
    if (x == NULL) {
        AIDS_FREE(x);
    }

    return result;
}

STEGDEF const char *steg_failure_reason(void) { return steg__g_failure_reason; }
