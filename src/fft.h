#ifndef FFT_H
#define FFT_H

#include <complex.h>

void fft_simple(const complex double *x, unsigned long n, complex double *x_out);
void ifft_simple(const complex double *x, unsigned long n, complex double *x_out);

void fft_dit(const complex double *x, unsigned long n, complex double *x_out);
void ifft_dit(const complex double *x, unsigned long n, complex double *x_out);

void fft2d(const complex double *x, unsigned long width, unsigned long height, complex double *x_out);
void ifft2d(const complex double *x, unsigned long width, unsigned long height, complex double *x_out);

#endif // FFT_H
