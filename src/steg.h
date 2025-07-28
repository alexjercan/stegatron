#ifndef STEG_H
#define STEG_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    STEG_OK = 0,
    STEG_ERR = 1,
} Steg_Result;

#ifndef STEGDEF
#ifdef STEG_STATIC
#define STEGDEF static
#else
#define STEGDEF extern
#endif
#endif

STEGDEF Steg_Result steg_hide_lsb(uint8_t *bytes, size_t bytes_length,
                                  const uint8_t *payload, size_t payload_length,
                                  int compression);
STEGDEF Steg_Result steg_show_lsb(const uint8_t *bytes, size_t bytes_length,
                                  uint8_t **message, size_t *message_length,
                                  int compression);

STEGDEF Steg_Result steg_hide_fft(uint8_t *bytes, size_t width, size_t height,
                                  const uint8_t *payload, size_t payload_length);
STEGDEF Steg_Result steg_show_fft(const uint8_t *og_bytes, const uint8_t *bytes,
                                  size_t width, size_t height, uint8_t **message,
                                  size_t *message_length);

STEGDEF const char *steg_failure_reason(void);

#endif // STEG_H
