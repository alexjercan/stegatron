#include <complex.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "signal.h"

void fft_simple(const complex double *x, unsigned long n, complex double *x_out) {
    for (size_t k = 0; k < n; k++) {
        x_out[k] = 0.0 + 0.0 * I;
        for (size_t m = 0; m < n; m++) {
            complex double exponent = I * 2 * M_PI * k * m / n;
            x_out[k] += x[m] * cexp(-exponent);
        }
    }
}

void ifft_simple(const complex double *x, unsigned long n, complex double *x_out) {
    for (size_t k = 0; k < n; k++) {
        x_out[k] = 0.0 + 0.0 * I;
        for (size_t m = 0; m < n; m++) {
            complex double exponent = I * 2 * M_PI * k * m / n;
            x_out[k] += x[m] * cexp(exponent);
        }
        x_out[k] = x_out[k] / n;
    }
}

static void fft__dit_internal(const complex double *x, unsigned long n, complex double *x_out) {
    if (n == 1) {
        x_out[0] = x[0];
        return;
    }

    unsigned long half = n / 2;
    complex double *even = malloc(half * sizeof(complex double));
    complex double *odd  = malloc(half * sizeof(complex double));
    for (unsigned long i = 0; i < half; ++i) {
        even[i] = x[2 * i];
        odd[i]  = x[2 * i + 1];
    }

    complex double *fft_even = malloc(half * sizeof(complex double));
    complex double *fft_odd  = malloc(half * sizeof(complex double));

    fft_dit(even, half, fft_even);
    fft_dit(odd,  half, fft_odd);

    for (unsigned long k = 0; k < half; ++k) {
        complex double twiddle = cexp(-2.0 * I * M_PI * k / n) * fft_odd[k];
        x_out[k]       = fft_even[k] + twiddle;
        x_out[k + half] = fft_even[k] - twiddle;
    }

    free(even);
    free(odd);
    free(fft_even);
    free(fft_odd);
}

void fft_dit(const complex double *x, unsigned long n, complex double *x_out) {
    assert((n & (n - 1)) == 0 && "n must be a power of 2");
    fft__dit_internal(x, n, x_out);
}

static void ifft__dit_recursive(const complex double *x, unsigned long n, complex double *x_out) {
    if (n == 1) {
        x_out[0] = x[0];
        return;
    }

    unsigned long half = n / 2;

    complex double *even = malloc(half * sizeof(complex double));
    complex double *odd  = malloc(half * sizeof(complex double));
    for (unsigned long i = 0; i < half; ++i) {
        even[i] = x[2 * i];
        odd[i]  = x[2 * i + 1];
    }

    complex double *ifft_even = malloc(half * sizeof(complex double));
    complex double *ifft_odd  = malloc(half * sizeof(complex double));

    ifft__dit_recursive(even, half, ifft_even);
    ifft__dit_recursive(odd,  half, ifft_odd);

    for (unsigned long k = 0; k < half; ++k) {
        complex double twiddle = cexp(2.0 * I * M_PI * k / n) * ifft_odd[k];
        x_out[k]        = ifft_even[k] + twiddle;
        x_out[k + half] = ifft_even[k] - twiddle;
    }

    free(even);
    free(odd);
    free(ifft_even);
    free(ifft_odd);
}

void ifft_dit(const complex double *x, unsigned long n, complex double *x_out) {
    assert((n & (n - 1)) == 0 && "n must be a power of 2");

    ifft__dit_recursive(x, n, x_out);

    for (unsigned long i = 0; i < n; ++i) {
        x_out[i] /= n;
    }
}

void fft2d(const complex double *x, unsigned long width, unsigned long height, complex double *x_out) {
    assert((width & (width - 1)) == 0 && "width must be a power of 2");
    assert((height & (height - 1)) == 0 && "height must be a power of 2");

    complex double *tmp = malloc(sizeof(complex double) * width * height);

    for (size_t j = 0; j < height; j++) {
        fft_dit(x + j * width, width, tmp + j * width);
    }

    for (size_t i = 0; i < width; i++) {
        complex double col_in[height], col_out[height];
        for (size_t j = 0; j < height; j++) {
            col_in[j] = tmp[j * width + i];
        }

        fft_dit(col_in, height, col_out);

        for (size_t j = 0; j < height; j++) {
            x_out[j * width + i] = col_out[j];
        }
    }

    free(tmp);
}

void ifft2d(const complex double *x, unsigned long width, unsigned long height, complex double *x_out) {
    assert((width & (width - 1)) == 0 && "width must be a power of 2");
    assert((height & (height - 1)) == 0 && "height must be a power of 2");

    complex double *tmp = malloc(sizeof(complex double) * width * height);

    for (size_t j = 0; j < height; j++) {
        ifft_dit(x + j * width, width, tmp + j * width);
    }

    for (size_t i = 0; i < width; i++) {
        complex double col_in[height], col_out[height];
        for (size_t j = 0; j < height; j++) {
            col_in[j] = tmp[j * width + i];
        }

        ifft_dit(col_in, height, col_out);

        for (size_t j = 0; j < height; j++) {
            x_out[j * width + i] = col_out[j];
        }
    }

    free(tmp);
}

static inline double alpha(int k) {
    return (k == 0) ? sqrt(1.0 / BLOCK_SIZE) : sqrt(2.0 / BLOCK_SIZE);
}

void dct2d(const double x[BLOCK_SIZE][BLOCK_SIZE], double X[BLOCK_SIZE][BLOCK_SIZE]) {
    for (size_t k1 = 0; k1 < BLOCK_SIZE; k1++) {
        for (size_t k2 = 0; k2 < BLOCK_SIZE; k2++) {
            double sum = 0;
            for (size_t n1 = 0; n1 < BLOCK_SIZE; n1++) {
                for (size_t n2 = 0; n2 < BLOCK_SIZE; n2++) {
                    sum += x[n1][n2] *
                        cos(M_PI / BLOCK_SIZE * (n1 + 0.5) * k1) *
                        cos(M_PI / BLOCK_SIZE * (n2 + 0.5) * k2);
                }
            }

            X[k1][k2] = alpha(k1) * alpha(k2) * sum;
        }
    }
}

void idct2d(const double X[BLOCK_SIZE][BLOCK_SIZE], double x[BLOCK_SIZE][BLOCK_SIZE]) {
    for (size_t n1 = 0; n1 < BLOCK_SIZE; n1++) {
        for (size_t n2 = 0; n2 < BLOCK_SIZE; n2++) {
            double sum = 0;
            for (size_t k1 = 0; k1 < BLOCK_SIZE; k1++) {
                for (size_t k2 = 0; k2 < BLOCK_SIZE; k2++) {
                    sum += alpha(k1) * alpha(k2) * X[k1][k2] *
                        cos(M_PI / BLOCK_SIZE * (n1 + 0.5) * k1) *
                        cos(M_PI / BLOCK_SIZE * (n2 + 0.5) * k2);
                }
            }

            x[n1][n2] = sum;
        }
    }
}
