#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define AIDS_IMPLEMENTATION
#include "aids.h"

#define BYTE_SIZE 8
#define BYTE_COUNT 2

static void util_hide_lsbn(unsigned char *ptr, unsigned char byte, int count) {
    assert(count <= BYTE_SIZE);
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

static void util_show_lsbn(const unsigned char *ptr, unsigned char *byte, int count) {
    assert(count <= BYTE_SIZE);
    for (size_t i = 0; i < BYTE_SIZE / count; i++) {
        unsigned char ptr_byte = ptr[i];
        for (size_t j = 0; j < count; j++) {
            unsigned char mask = 0b00000001 << (count - j - 1);
            unsigned char bit = ((ptr_byte & mask) >> (count - j - 1)) & 0b00000001;
            *byte |= (bit << (BYTE_SIZE - i * count - j - 1));
        }
    }
}

static int steg_hide_lsb(unsigned char *cover, size_t cover_length,
                         const unsigned char *message, size_t message_length) {
    size_t byte_count = BYTE_SIZE / BYTE_COUNT;
    if (message_length * byte_count + sizeof(size_t) * byte_count > cover_length) {
        fprintf(stderr, "Error: Data is too big\n");
        return 1;
    }

    for (size_t i = 0; i < sizeof(size_t); i++) {
        unsigned char length_byte = ((unsigned char*)&message_length)[i];
        util_hide_lsbn(cover + i * byte_count, length_byte, BYTE_COUNT);
    }

    for (size_t i = 0; i < message_length; i++) {
        util_hide_lsbn(cover + i * byte_count + sizeof(size_t) * byte_count, message[i], BYTE_COUNT);
    }

    return 0;
}

static int steg_show_lsb(const unsigned char *data, size_t data_length,
                         unsigned char **message, size_t *message_length) {
    size_t byte_count = BYTE_SIZE / BYTE_COUNT;

    for (size_t i = 0; i < sizeof(size_t); i++) {
        util_show_lsbn(data + i * byte_count, ((unsigned char*)message_length) + i, BYTE_COUNT);
    }

    *message = calloc((*message_length + 1), sizeof(unsigned char));
    for (size_t i = 0; i < *message_length; i++) {
        util_show_lsbn(data + i * byte_count + sizeof(size_t) * byte_count, (*message) + i, BYTE_COUNT);
    }

    return 0;
}

int main() {
    int x = 0;
    int y = 0;
    int comp = 0;

    unsigned char *cover = stbi_load("Lenna.png", &x, &y, &comp, 0);
    if (cover == NULL) {
        printf("Failed to load image: %s\n", stbi_failure_reason());
        return 1;
    }
    printf("Image loaded, width %d, height %d, comp %d\n", x, y, comp);

    Aids_String_Slice ss = {0};
    aids_io_read("test.bin", &ss, "rb");
    printf("Message loaded, size %lu\n", ss.len);

    size_t cover_length = x * y * comp;
    steg_hide_lsb(cover, cover_length, ss.str, ss.len);

    unsigned char *message = NULL;
    size_t message_length = 0;
    steg_show_lsb(cover, cover_length, &message, &message_length);
    printf("Retrieved message: %s with length %lu\n", message, message_length);

     if (!stbi_write_png("Lenna_out.png", x, y, comp, cover, x * comp)) {
        printf("Failed to write image: %s\n", stbi_failure_reason());
        return 1;
     }

     free(message);
     free(cover);
     free(ss.str);

    return 0;
}
