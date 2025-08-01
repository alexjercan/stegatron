#include <stdlib.h>

#include "error.h"

#define HAMMING_N 7
#define HAMMING_K 4

const unsigned char G[HAMMING_K][HAMMING_N] = {
    { 1, 0, 0, 0, 1, 1, 0 },
    { 0, 1, 0, 0, 1, 0, 1 },
    { 0, 0, 1, 0, 0, 1, 1 },
    { 0, 0, 0, 1, 1, 1, 1 },
};

static void hamming__helper_encode(unsigned char nibble, unsigned char *byte) {
    unsigned char a[HAMMING_K] = {
        (nibble >> 3) & 1,
        (nibble >> 2) & 1,
        (nibble >> 1) & 1,
        (nibble >> 0) & 1
    };
    unsigned char x[HAMMING_N] = {0};

    for (size_t i = 0; i < HAMMING_N; i++) {
        unsigned char sum = 0;
        for (size_t j = 0; j < HAMMING_K; j++) {
            sum += a[j] * G[j][i];
        }
        x[i] = (sum % 2) & 1;
    }

    *byte = 0;
    for (size_t i = 0; i < HAMMING_N; i++) {
        *byte = *byte | (x[i] << (HAMMING_N - i));
    }
}

static void hamming__helper_decode(unsigned char byte, unsigned char *nibble) {
    *nibble = (byte >> 4) & 0b00001111;
    //TODO: actually check for errors
}

Ecc_Result hamming_encode(const unsigned char *a, unsigned long a_length, unsigned char **x, unsigned long *x_length) {
    *x_length = a_length * 2;
    *x = malloc(*x_length * sizeof(unsigned char));
    if (*x == NULL) {
        return ECC_ERR;
    }

    for (size_t i = 0; i < a_length; i++) {
        unsigned char byte = a[i];

        unsigned char high = (byte >> 4) & 0b00001111;
        size_t high_index = i * 2;
        hamming__helper_encode(high, &(*x)[high_index]);

        unsigned char lower = byte & 0b00001111;
        size_t lower_index = i * 2 + 1;
        hamming__helper_encode(lower, &(*x)[lower_index]);
    }

    return ECC_OK;
}

Ecc_Result hamming_decode(const unsigned char *x, unsigned long x_length, unsigned char **a, unsigned long *a_length) {
    *a_length = x_length / 2;
    *a = malloc(*a_length * sizeof(unsigned char));
    if (*a == NULL) {
        return ECC_ERR;
    }

    for (size_t i = 0; i < *a_length; i++) {
        unsigned char high = 0;
        size_t high_index = i * 2;
        unsigned char high_byte = x[high_index];
        hamming__helper_decode(high_byte, &high);

        unsigned char lower = 0;
        size_t lower_index = i * 2 + 1;
        unsigned char lower_byte = x[lower_index];
        hamming__helper_decode(lower_byte, &lower);

        (*a)[i] = (high << 4) | lower;
    }

    return ECC_OK;
}
