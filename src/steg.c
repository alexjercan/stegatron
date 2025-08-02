#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complex.h>
#include <math.h>

#include "aids.h"
#include "signal.h"
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
                                  const uint8_t *payload, size_t payload_length,
                                  int compression) {
    Steg_Result result = STEG_OK;

    if (!steg__validate_compression(compression)) {
        steg__g_failure_reason = "Invalid compression value";
        return_defer(STEG_ERR);
    }

    size_t byte_stride = BYTE_SIZE / compression;
    if (payload_length * byte_stride > bytes_length) {
        steg__g_failure_reason = "Data is too big for the cover image";
        return_defer(STEG_ERR);
    }

    for (size_t i = 0; i < payload_length; i++) {
        steg__util_hide_lsbn(bytes + i * byte_stride, payload[i], compression);
    }

defer:
    return result;
}

STEGDEF Steg_Result steg_show_lsb(const uint8_t *bytes, size_t bytes_length,
                                  uint8_t *message, size_t message_length,
                                  int compression) {
    Steg_Result result = STEG_OK;

    if (!steg__validate_compression(compression)) {
        steg__g_failure_reason = "Invalid compression value";
        return_defer(STEG_ERR);
    }

    if (message_length * (BYTE_SIZE / compression) > bytes_length) {
        steg__g_failure_reason = "Data is too big for the cover image";
        return_defer(STEG_ERR);
    }

    size_t byte_stride = BYTE_SIZE / compression;

    memset(message, 0, message_length * sizeof(unsigned char));
    for (size_t i = 0; i < message_length; i++) {
        steg__util_show_lsbn(bytes + i * byte_stride, message + i, compression);
    }

defer:
    return result;
}

/* Copied from https://github.com/MidoriYakumo/FdSig/tree/master */
static void steg__centralize(complex double *x, size_t width, size_t height,
                             double *y, double *low, double *high) {
    size_t n = width * height;
    for (size_t i = 0; i < n; i++) {
        y[i] = creal(x[i]);
    }

    double min_val = y[0];
    double max_val = y[0];
    for (size_t i = 1; i < n; i++) {
        if (y[i] < min_val) {
            min_val = y[i];
        }
        if (y[i] > max_val) {
            max_val = y[i];
        }
    }

    size_t threshold = (size_t)((double)n * 0.06);
    double l = min_val;
    double r = max_val;
    while (l + 1 <= r) {
        double m = (l + r) / 2.0;
        size_t count = 0;
        for (size_t i = 0; i < n; i++) {
            if (y[i] < m) {
                count++;
            }
        }

        if (count < threshold) {
            l = m;
        } else {
            r = m;
        }
    }
    *low = l;

    l = min_val;
    r = max_val;
    while (l + 1 <= r) {
        double m = (l + r) / 2.0;
        size_t count = 0;
        for (size_t i = 0; i < n; i++) {
            if (y[i] > m) {
                count++;
            }
        }

        if (count < threshold) {
            r = m;
        } else {
            l = m;
        }
    }
    *high = r;
    if (*low + 1 >= *high) {
        *high = *low + 1;
    }

    for (size_t i = 0; i < n; i++) {
        y[i] = (y[i] - *low) / (*high - *low);
        if (y[i] < 0.0) {
            y[i] = 0.0;
        } else if (y[i] > 1.0) {
            y[i] = 1.0;
        }
    }
}

STEGDEF Steg_Result steg_hide_fft(uint8_t *bytes, size_t width, size_t height, size_t num_chan,
                                  const uint8_t *payload, size_t payload_width, size_t payload_height, size_t payload_chan) {
    // Params
    size_t margin_y = 1, margin_x = 1;

    complex double *fft_c = NULL;
    complex double *tmp = NULL;
    double *y = NULL;

    Steg_Result result = STEG_OK;

    // Validate input dimensions
    if ((width & (width - 1)) != 0 || (height & (height - 1)) != 0) {
        steg__g_failure_reason = "The input data is not a power of 2 shape.";
        return_defer(STEG_ERR);
    }
    if (payload_width > width / 2 - 2 * margin_x || payload_height > height / 2 - 2 * margin_y) {
        steg__g_failure_reason = "Payload dimensions are too large for the cover image";
        return_defer(STEG_ERR);
    }
    if (payload_chan > num_chan) {
        steg__g_failure_reason = "Payload channel count exceeds cover image channel count";
        return_defer(STEG_ERR);
    }

    for (size_t c = 0; c < num_chan; c++) {
        // Normalize the image to [0, 1] range
        fft_c = AIDS_REALLOC(NULL, sizeof(complex double) * width * height);
        if (fft_c == NULL) {
            steg__g_failure_reason = aids_failure_reason();
            return_defer(STEG_ERR);
        }
        for (size_t i = 0; i < width * height; i++) {
            fft_c[i] = (double)bytes[i * num_chan + c] / 255.0 + 0.0*I;
        }

        // Perform FFT on the image data
        tmp = AIDS_REALLOC(NULL, sizeof(complex double) * width * height);
        if (tmp == NULL) {
            steg__g_failure_reason = aids_failure_reason();
            return_defer(STEG_ERR);
        }
        fft2d(fft_c, width, height, tmp);
        memcpy(fft_c, tmp, sizeof(complex double) * width * height);
        AIDS_FREE(tmp); tmp = NULL;

        // Compute the alpha value for scaling
        y = AIDS_REALLOC(NULL, sizeof(double) * width * height);
        if (y == NULL) {
            steg__g_failure_reason = aids_failure_reason();
            return_defer(STEG_ERR);
        }
        double low, high;
        steg__centralize(fft_c, width, height, y, &low, &high);
        AIDS_FREE(y); y = NULL;
        double alpha = high - low;

        for (size_t row = 0; row < payload_height; row++) {
            for (size_t col = 0; col < payload_width; col++) {
                size_t index = row * payload_width + col;

                // Normalize the payload value to [0, 1] range
                double payload_value = (double)payload[index * payload_chan + 0] / 255.0;

                size_t t_row = margin_y + row;
                size_t t_col = margin_x + col;
                size_t t_col_mirror = width - 1 - t_col;

                // Insert the payload value into the FFT coefficients
                fft_c[t_row * width + t_col] += payload_value * alpha;
                fft_c[t_row * width + t_col_mirror] += payload_value * alpha;
            }
        }

        // Perform inverse FFT to get the modified image data
        tmp = AIDS_REALLOC(NULL, sizeof(complex double) * width * height);
        if (tmp == NULL) {
            steg__g_failure_reason = aids_failure_reason();
            return_defer(STEG_ERR);
        }
        ifft2d(fft_c, width, height, tmp);
        memcpy(fft_c, tmp, sizeof(complex double) * width * height);
        AIDS_FREE(tmp); tmp = NULL;

        // Normalize the modified image data back to [0, 255] range
        for (size_t i = 0; i < width * height; i++) {
            bytes[i * 3 + c] = fmin(fmax(creal(fft_c[i]) * 255.0, 0), 255);
        }

        AIDS_FREE(fft_c); fft_c = NULL;
        AIDS_FREE(tmp); tmp = NULL;
        AIDS_FREE(y); y = NULL;
    }

defer:
    if (fft_c != NULL) {
        AIDS_FREE(fft_c);
    }
    if (tmp != NULL) {
        AIDS_FREE(tmp);
    }
    if (y != NULL) {
        AIDS_FREE(y);
    }

    return result;
}

STEGDEF Steg_Result steg_show_fft(const uint8_t *og_bytes, const uint8_t *bytes,
                                  size_t width, size_t height, size_t num_chan, uint8_t **message) {
    complex double *fft_c = NULL;
    complex double *fft_ogc = NULL;
    complex double *tmp = NULL;
    double *y = NULL;

    Steg_Result result = STEG_OK;

    // Validate input dimensions
    if ((width & (width - 1)) != 0 || (height & (height - 1)) != 0) {
        steg__g_failure_reason = "The input data is not a power of 2 shape.";
        return_defer(STEG_ERR);
    }

    *message = AIDS_REALLOC(NULL, (width * height * num_chan) * sizeof(unsigned char));
    if (*message == NULL) {
        steg__g_failure_reason = aids_failure_reason();
        return_defer(STEG_ERR);
    }
    memset(*message, 0, (width * height * num_chan) * sizeof(unsigned char));

    for (size_t c = 0; c < num_chan; c++) {
        // Normalize the modified image and the original image to [0, 1] range
        fft_c = AIDS_REALLOC(NULL, sizeof(complex double) * width * height);
        if (fft_c == NULL) {
            steg__g_failure_reason = aids_failure_reason();
            return_defer(STEG_ERR);
        }
        fft_ogc = AIDS_REALLOC(NULL, sizeof(complex double) * width * height);
        if (fft_ogc == NULL) {
            steg__g_failure_reason = aids_failure_reason();
            return_defer(STEG_ERR);
        }
        for (size_t i = 0; i < width * height; i++) {
            fft_c[i] = (double)bytes[i * num_chan + c] / 255.0 + 0.0*I;
            fft_ogc[i] = (double)og_bytes[i * num_chan + c] / 255.0 + 0.0*I;
        }

        // Perform FFT on the modified image data
        tmp = AIDS_REALLOC(NULL, sizeof(complex double) * width * height);
        if (tmp == NULL) {
            steg__g_failure_reason = aids_failure_reason();
            return_defer(STEG_ERR);
        }
        fft2d(fft_c, width, height, tmp);
        memcpy(fft_c, tmp, sizeof(complex double) * width * height);
        AIDS_FREE(tmp); tmp = NULL;

        // Perform FFT on the original image data
        tmp = AIDS_REALLOC(NULL, sizeof(complex double) * width * height);
        if (tmp == NULL) {
            steg__g_failure_reason = aids_failure_reason();
            return_defer(STEG_ERR);
        }
        fft2d(fft_ogc, width, height, tmp);
        memcpy(fft_ogc, tmp, sizeof(complex double) * width * height);
        AIDS_FREE(tmp); tmp = NULL;

        // Get the difference between the FFT coefficients of the modified image and the original image
        for (size_t i = 0; i < width * height; i++) {
            fft_c[i] = fft_c[i] - fft_ogc[i];
        }

        // Centralize the FFT coefficients to [0, 1] range and extract the payload
        double low, high;
        y = AIDS_REALLOC(NULL, sizeof(double) * width * height);
        steg__centralize(fft_c, width, height, y, &low, &high);
        for (size_t i = 0; i < width * height; i++) {
            unsigned char payload_byte = (unsigned char)fmin(fmax(y[i] * 255.0, 0), 255);
            (*message)[i * num_chan + c] = payload_byte;
        }
        AIDS_FREE(y); y = NULL;

        AIDS_FREE(fft_c); fft_c = NULL;
        AIDS_FREE(fft_ogc); fft_ogc = NULL;
        AIDS_FREE(tmp); tmp = NULL;
        AIDS_FREE(y); y = NULL;
    }

defer:
    if (fft_c != NULL) {
        AIDS_FREE(fft_c);
    }
    if (fft_ogc != NULL) {
        AIDS_FREE(fft_ogc);
    }
    if (tmp != NULL) {
        AIDS_FREE(tmp);
    }
    if (y != NULL) {
        AIDS_FREE(y);
    }

    return result;
}

const size_t COEFF_Xs[4] = {4};
const size_t COEFF_Ys[4] = {3};

static bool steg__validate_compression_dct(size_t compression) {
    // TODO: Find some edge cases where this is not true
    AIDS_UNUSED(compression);
    return true;
}

static void block_from_array(const double *array, size_t stride, size_t block_x, size_t block_y, double block[BLOCK_SIZE][BLOCK_SIZE]) {
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        for (size_t j = 0; j < BLOCK_SIZE; j++) {
            block[i][j] = array[(block_y * BLOCK_SIZE + i) * stride + (block_x * BLOCK_SIZE + j)];
        }
    }
}

static void block_to_array(const double block[BLOCK_SIZE][BLOCK_SIZE], size_t stride, size_t block_x, size_t block_y, double *array) {
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        for (size_t j = 0; j < BLOCK_SIZE; j++) {
            array[(block_y * BLOCK_SIZE + i) * stride + (block_x * BLOCK_SIZE + j)] = block[i][j];
        }
    }
}

static void steg__hide_dct_helper(double *normalized, size_t width, size_t height, size_t num_chan,
                                  const uint8_t *payload, size_t payload_length, size_t compression) {
    size_t byte_index = 0;
    size_t bit_index = 0;
    for (size_t i = 0; i < width / BLOCK_SIZE && byte_index < payload_length; i++) {
        for (size_t j = 0; j < height / BLOCK_SIZE && byte_index < payload_length; j++) {
            double block[BLOCK_SIZE][BLOCK_SIZE] = {0};
            block_from_array(normalized, width * num_chan, i, j, block);

            double dct_block[BLOCK_SIZE][BLOCK_SIZE] = {0};
            dct2d(block, dct_block);

            for (size_t k = 0; k < compression; k++) {
                uint8_t byte = payload[byte_index];
                char bit = (byte >> (BYTE_SIZE - bit_index - 1)) & 0b00000001;

                double coeff = dct_block[COEFF_Xs[k]][COEFF_Ys[k]];

                coeff = round(coeff) - ((int)coeff % 2) + bit;
                dct_block[COEFF_Xs[k]][COEFF_Ys[k]] = coeff;

                bit_index++;
                if (bit_index >= BYTE_SIZE) {
                    bit_index = 0;
                    byte_index++;
                }
            }

            idct2d(dct_block, block);
            block_to_array(block, width * num_chan, i, j, normalized);
        }
    }
}

static void steg__show_dct_helper(const double *normalized, size_t width, size_t height, size_t num_chan,
                                  uint8_t *message, size_t message_length, size_t compression) {
    size_t bit_index = 0;
    size_t byte_index = 0;
    uint8_t byte = 0;
    for (size_t i = 0; i < width / BLOCK_SIZE && byte_index < message_length; i++) {
        for (size_t j = 0; j < height / BLOCK_SIZE && byte_index < message_length; j++) {
            double block[BLOCK_SIZE][BLOCK_SIZE] = {0};
            block_from_array(normalized, width * num_chan, i, j, block);

            double dct_block[BLOCK_SIZE][BLOCK_SIZE] = {0};
            dct2d(block, dct_block);

            for (size_t k = 0; k < compression; k++) {
                double coeff = dct_block[COEFF_Xs[k]][COEFF_Ys[k]];
                uint8_t bit = (uint8_t)((int)round(coeff) % 2) & 0b00000001;
                byte |= (bit << (BYTE_SIZE - bit_index - 1));

                bit_index += compression;
                if (bit_index >= BYTE_SIZE) {
                    message[byte_index] = byte;

                    byte_index++;
                    bit_index = 0;
                    byte = 0;
                }
            }
        }
    }
}

STEGDEF Steg_Result steg_hide_dct(uint8_t *bytes, size_t width, size_t height, size_t num_chan,
                                  const uint8_t *payload, size_t payload_length, size_t compression) {
    double *normalized = NULL;

    Steg_Result result = STEG_OK;

    if (!steg__validate_compression_dct(compression)) {
        steg__g_failure_reason = "Invalid compression value";
        return_defer(STEG_ERR);
    }
    if (width % BLOCK_SIZE != 0 || height % BLOCK_SIZE != 0) {
        steg__g_failure_reason = "The input data is not a multiple of the DCT block size.";
        return_defer(STEG_ERR);
    }
    if (payload_length > width * height * num_chan / BLOCK_SIZE / BLOCK_SIZE) {
        steg__g_failure_reason = "Payload is too large for the cover image";
        return_defer(STEG_ERR);
    }

    normalized = AIDS_REALLOC(NULL, sizeof(double) * width * height * num_chan);
    if (normalized == NULL) {
        steg__g_failure_reason = aids_failure_reason();
        return_defer(STEG_ERR);
    }
    for (size_t i = 0; i < width * height * num_chan; i++) {
        normalized[i] = (double)bytes[i] / 255.0;
    }

    steg__hide_dct_helper(normalized, width, height, num_chan, (uint8_t*)&payload_length, sizeof(size_t), compression);
    size_t offset = sizeof(size_t) * num_chan * BLOCK_SIZE * BLOCK_SIZE;
    steg__hide_dct_helper(normalized + offset, width, height, num_chan, payload, payload_length, compression);

    for (size_t i = 0; i < width * height * num_chan; i++) {
        bytes[i] = (unsigned char)fmin(fmax(normalized[i] * 255.0, 0), 255);
    }

defer:
    if (normalized != NULL) {
        AIDS_FREE(normalized);
    }

    return result;
}

STEGDEF Steg_Result steg_show_dct(const uint8_t *bytes, size_t width, size_t height, size_t num_chan,
                                  uint8_t **message, size_t *message_length, size_t compression) {
    double *normalized = NULL;

    Steg_Result result = STEG_OK;

    if (!steg__validate_compression(compression)) {
        steg__g_failure_reason = "Invalid compression value";
        return_defer(STEG_ERR);
    }
    if (width % BLOCK_SIZE != 0 || height % BLOCK_SIZE != 0) {
        steg__g_failure_reason = "The input data is not a multiple of the DCT block size.";
        return_defer(STEG_ERR);
    }

    normalized = AIDS_REALLOC(NULL, sizeof(double) * width * height * num_chan);
    if (normalized == NULL) {
        steg__g_failure_reason = aids_failure_reason();
        return_defer(STEG_ERR);
    }
    for (size_t i = 0; i < width * height * num_chan; i++) {
        normalized[i] = (double)bytes[i] / 255.0;
    }

    steg__show_dct_helper(normalized, width, height, num_chan, (uint8_t*)message_length, sizeof(size_t), compression);
    printf("Message length: %zu\n", *message_length);
    if (*message_length <= 0) {
        steg__g_failure_reason = "Message length is invalid";
        return_defer(STEG_ERR);
    }
    if (*message_length > width * height * num_chan / BLOCK_SIZE / BLOCK_SIZE) {
        steg__g_failure_reason = "Message length exceeds the maximum allowed size";
        return_defer(STEG_ERR);
    }
    *message = AIDS_REALLOC(NULL, (*message_length + 1) * sizeof(unsigned char));
    if (*message == NULL) {
        steg__g_failure_reason = aids_failure_reason();
        return_defer(STEG_ERR);
    }
    memset(*message, 0, (*message_length + 1) * sizeof(unsigned char));
    size_t offset = sizeof(size_t) * num_chan * BLOCK_SIZE * BLOCK_SIZE;
    steg__show_dct_helper(normalized + offset, width, height, num_chan, *message, *message_length, compression);

defer:
    if (normalized != NULL) {
        AIDS_FREE(normalized);
    }

    return result;
}

STEGDEF const char *steg_failure_reason(void) { return steg__g_failure_reason; }
