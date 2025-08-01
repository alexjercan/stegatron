#ifndef ERROR_H
#define ERROR_H

typedef enum {
    ECC_OK = 0,
    ECC_ERR = 1,
} Ecc_Result;

Ecc_Result hamming_encode(const unsigned char *a, unsigned long a_length, unsigned char **x, unsigned long *x_length);
Ecc_Result hamming_decode(const unsigned char *x, unsigned long x_length, unsigned char **a, unsigned long *a_length);

#endif // ERROR_H
