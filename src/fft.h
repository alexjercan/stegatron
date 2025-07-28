#ifndef FFT_H
#define FFT_H

#include <complex.h>

void fft_simple(complex double *x, unsigned long n, complex double *x_out);
void ifft_simple(complex double *x, unsigned long n, complex double *x_out);

void fft_dit(complex double *x, unsigned long n, complex double *x_out);
void ifft_dit(complex double *x, unsigned long n, complex double *x_out);

void fft2d(complex double *x, unsigned long width, unsigned long height, complex double *x_out);
void ifft2d(complex double *x, unsigned long width, unsigned long height, complex double *x_out);

void dft_naive(const complex double *x, size_t N, complex double *X);
void idft_naive(const complex double *X, size_t N, complex double *x);

void fft_CooleyTukey(const complex double *x, size_t width, size_t height, complex double *X);
void ifft_CooleyTukey(const complex double *X, size_t width, size_t height, complex double *x);

#endif // FFT_H
