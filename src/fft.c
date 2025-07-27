#include <complex.h>
#include <stdlib.h>
#include <math.h>

#include "fft.h"

void fft_simple(complex double *x, unsigned long n, complex double *x_out) {
    for (size_t k = 0; k < n; k++) {
        x_out[k] = 0.0 + 0.0 * I;
        for (size_t m = 0; m < n; m++) {
            complex double exponent = I * 2 * M_PI * k * m / n;
            x_out[k] += x[m] * cexp(-exponent);
        }
    }
}

void ifft_simple(complex double *x, unsigned long n, complex double *x_out) {
    for (size_t k = 0; k < n; k++) {
        x_out[k] = 0.0 + 0.0 * I;
        for (size_t m = 0; m < n; m++) {
            complex double exponent = I * 2 * M_PI * k * m / n;
            x_out[k] += x[m] * cexp(exponent);
        }
        x_out[k] = x_out[k] / n;
    }
}

void fft_dit(complex double *x, unsigned long n, complex double *x_out) {
    if ((n & (n - 1)) != 0) {
        exit(1);
    }

    if (n == 1) {
        x_out[0] = x[0];
        return;
    }

    // Split input into even and odd elements
    unsigned long half = n / 2;
    complex double *even = malloc(half * sizeof(complex double));
    complex double *odd  = malloc(half * sizeof(complex double));
    for (unsigned long i = 0; i < half; ++i) {
        even[i] = x[2 * i];
        odd[i]  = x[2 * i + 1];
    }

    // Allocate temporary arrays for recursive output
    complex double *fft_even = malloc(half * sizeof(complex double));
    complex double *fft_odd  = malloc(half * sizeof(complex double));

    // Recursive calls
    fft_dit(even, half, fft_even);
    fft_dit(odd,  half, fft_odd);

    // Combine
    for (unsigned long k = 0; k < half; ++k) {
        complex double twiddle = cexp(-2.0 * I * M_PI * k / n) * fft_odd[k];
        x_out[k]       = fft_even[k] + twiddle;
        x_out[k + half] = fft_even[k] - twiddle;
    }

    // Clean up
    free(even);
    free(odd);
    free(fft_even);
    free(fft_odd);
}

static void ifft__dit_recursive(complex double *x, unsigned long n, complex double *x_out) {
    if (n == 1) {
        x_out[0] = x[0];
        return;
    }

    unsigned long half = n / 2;

    // Split even and odd
    complex double *even = malloc(half * sizeof(complex double));
    complex double *odd  = malloc(half * sizeof(complex double));
    for (unsigned long i = 0; i < half; ++i) {
        even[i] = x[2 * i];
        odd[i]  = x[2 * i + 1];
    }

    // Recursive outputs
    complex double *ifft_even = malloc(half * sizeof(complex double));
    complex double *ifft_odd  = malloc(half * sizeof(complex double));

    ifft__dit_recursive(even, half, ifft_even);
    ifft__dit_recursive(odd,  half, ifft_odd);

    // Combine with positive sign for IFFT
    for (unsigned long k = 0; k < half; ++k) {
        complex double twiddle = cexp(2.0 * I * M_PI * k / n) * ifft_odd[k];
        x_out[k]        = ifft_even[k] + twiddle;
        x_out[k + half] = ifft_even[k] - twiddle;
    }

    // Clean up
    free(even);
    free(odd);
    free(ifft_even);
    free(ifft_odd);
}

void ifft_dit(complex double *x, unsigned long n, complex double *x_out) {
    if ((n & (n - 1)) != 0) {
        exit(1);
    }

    ifft__dit_recursive(x, n, x_out);

    // Normalize the output
    for (unsigned long i = 0; i < n; ++i) {
        x_out[i] /= n;
    }
}
