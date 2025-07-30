#ifndef SIGNAL_H
#define SIGNAL_H

#include <complex.h>

void fft_simple(const complex double *x, unsigned long n, complex double *x_out);
void ifft_simple(const complex double *x, unsigned long n, complex double *x_out);

void fft_dit(const complex double *x, unsigned long n, complex double *x_out);
void ifft_dit(const complex double *x, unsigned long n, complex double *x_out);

void fft2d(const complex double *x, unsigned long width, unsigned long height, complex double *x_out);
void ifft2d(const complex double *x, unsigned long width, unsigned long height, complex double *x_out);

#define BLOCK_SIZE 8

void dct2d(const double x[BLOCK_SIZE][BLOCK_SIZE], double X[BLOCK_SIZE][BLOCK_SIZE]);
void idct2d(const double X[BLOCK_SIZE][BLOCK_SIZE], double x[BLOCK_SIZE][BLOCK_SIZE]);

#endif // SIGNAL_H
