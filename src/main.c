#include <time.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "steg.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "aids.h"
#include "argparse.h"

#define PROGRAM_NAME "steg"
#define PROGRAM_VERSION "v0.1.0"

#define COMMAND_HIDE_LSB "hide-lsb"
#define COMMAND_SHOW_LSB "show-lsb"
#define COMMAND_HIDE_FFT "hide-fft"
#define COMMAND_SHOW_FFT "show-fft"
#define COMMAND_HIDE_DCT "hide-dct"
#define COMMAND_SHOW_DCT "show-dct"
#define COMMAND_NOISE_LSB "noise-lsb"
#define COMMAND_VERSION "version"
#define COMMAND_HELP "help"

typedef struct {
    const char *image_path;  // Path to the image file
    const char *output_path; // Path to save the modified image
    const char *payload_path; // Path to the payload file (default: stdin)
    int compression_level;   // Compression level (default: 1)
    bool ecc;               // Use error correction
} Steg_Hide_Args_Lsb;

static int command_hide_lsb(int argc, char **argv) {
    Steg_Hide_Args_Lsb args = {0};

    Argparse_Parser parser = {0};
    argparse_parser_init(&parser, PROGRAM_NAME " " COMMAND_HIDE_LSB, "Hide a message in an image", PROGRAM_VERSION);

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'i',
                                    .long_name = "image",
                                    .description = "Path to the image file",
                                    .type = ARGUMENT_TYPE_POSITIONAL,
                                    .required = true});
    argparse_add_argument(
        &parser,
        (Argparse_Options){.short_name = 'o',
                           .long_name = "output",
                           .description = "Path to save the modified image (default: stdout)",
                           .type = ARGUMENT_TYPE_VALUE,
                           .required = true});

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'p',
                                    .long_name = "payload",
                                    .description = "Path to the payload file (default: stdin)",
                                    .type = ARGUMENT_TYPE_VALUE,
                                    .required = false});

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'c',
                                    .long_name = "compression",
                                    .description = "Compression level (default: 1)",
                                    .type = ARGUMENT_TYPE_VALUE,
                                    .required = false});

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'e',
                                    .long_name = "ecc",
                                    .description = "Use Error Correction (default: false)",
                                    .type = ARGUMENT_TYPE_FLAG,
                                    .required = false});


    if (argparse_parse(&parser, argc, argv) != ARG_OK) {
        argparse_print_help(&parser);
        exit(EXIT_FAILURE);
    }

    args.image_path = argparse_get_value(&parser, "image");
    args.output_path = argparse_get_value_or_default(&parser, "output", NULL);
    args.payload_path = argparse_get_value_or_default(&parser, "payload", NULL);
    const char *compression_str = argparse_get_value_or_default(&parser, "compression", "1");
    args.compression_level = atoi(compression_str);
    args.ecc = argparse_get_flag(&parser, "ecc");

    argparse_parser_free(&parser);

    int width, height, num_chan;
    uint8_t *bytes = stbi_load(args.image_path, &width, &height, &num_chan, 0);
    if (bytes == NULL) {
        aids_log(AIDS_ERROR, "Error loading image: %s", stbi_failure_reason());
        exit(EXIT_FAILURE);
    }
    size_t bytes_length = width * height * num_chan;

    Aids_String_Slice payload_slice = {0};
    if (aids_io_read(args.payload_path, &payload_slice, "rb") != AIDS_OK) {
        aids_log(AIDS_ERROR, "Error reading payload file: %s", aids_failure_reason());
        exit(EXIT_FAILURE);
    }
    uint8_t *payload = (uint8_t *)payload_slice.str;
    size_t payload_length = payload_slice.len;

    if (args.ecc) {
        uint8_t *enc = NULL;
        size_t enc_length = 0;
        if (hamming_encode(payload, payload_length, &enc, &enc_length) != ECC_OK) {
            aids_log(AIDS_ERROR, "Error encoding the message with Hamming Code");
            exit(EXIT_FAILURE);
        }

        AIDS_FREE(payload);
        payload = enc;
        payload_length = enc_length;
    }

    if (steg_hide_lsb(bytes, bytes_length, (const uint8_t *)payload, payload_length, args.compression_level) != STEG_OK) {
        aids_log(AIDS_ERROR, "Error hiding message in image: %s", steg_failure_reason());
        exit(EXIT_FAILURE);
    }

    if (stbi_write_png(args.output_path, width, height, num_chan, bytes,
                       width * num_chan) == 0) {
        aids_log(AIDS_ERROR, "Error saving modified image: %s", stbi_failure_reason());
        exit(EXIT_FAILURE);
    }

    aids_log(AIDS_INFO, "Message hidden successfully in %s", args.output_path);

    if (bytes != NULL) {
        stbi_image_free(bytes);
        bytes = NULL;
    }
    if (payload != NULL) {
        AIDS_FREE(payload);
        payload = NULL;
    }

    return 0;
}

typedef struct {
    const char *image_path; // Path to the image file
    const char *output_path; // Path to save the modified image (default: stdout)
    int compression_level;  // Compression level (default: 1)
    bool ecc;               // Use error correction
} Steg_Show_Args_Lsb;

static int command_show_lsb(int argc, char **argv) {
    Steg_Show_Args_Lsb args = {0};
    uint8_t *message = NULL;

    Argparse_Parser parser = {0};
    argparse_parser_init(&parser, PROGRAM_NAME " " COMMAND_SHOW_LSB, "Show a hidden message in an image", PROGRAM_VERSION);

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'i',
                                    .long_name = "image",
                                    .description = "Path to the image file",
                                    .type = ARGUMENT_TYPE_POSITIONAL,
                                    .required = true});

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'o',
                                    .long_name = "output",
                                    .description = "Path to save the modified image (default: stdout)",
                                    .type = ARGUMENT_TYPE_VALUE,
                                    .required = false});

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'c',
                                    .long_name = "compression",
                                    .description = "Compression level (default: 1)",
                                    .type = ARGUMENT_TYPE_VALUE,
                                    .required = false});

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'e',
                                    .long_name = "ecc",
                                    .description = "Use Error Correction (default: false)",
                                    .type = ARGUMENT_TYPE_FLAG,
                                    .required = false});

    if (argparse_parse(&parser, argc, argv) != ARG_OK) {
        argparse_print_help(&parser);
        exit(EXIT_FAILURE);
    }

    args.image_path = argparse_get_value(&parser, "image");
    args.output_path = argparse_get_value_or_default(&parser, "output", NULL);
    const char *compression_str = argparse_get_value_or_default(&parser, "compression", "1");
    args.compression_level = atoi(compression_str);
    args.ecc = argparse_get_flag(&parser, "ecc");

    argparse_parser_free(&parser);

    int width, height, num_chan;
    uint8_t *bytes = stbi_load(args.image_path, &width, &height, &num_chan, 0);
    if (bytes == NULL) {
        aids_log(AIDS_ERROR, "Error loading image: %s", stbi_failure_reason());
        exit(EXIT_FAILURE);
    }
    size_t bytes_length = width * height * num_chan;

    size_t message_length = 0;
    if (steg_show_lsb(bytes, bytes_length, &message, &message_length, args.compression_level) != STEG_OK) {
        aids_log(AIDS_ERROR, "Error showing message from image: %s", steg_failure_reason());
        exit(EXIT_FAILURE);
    }

    if (args.ecc) {
        uint8_t *dec = NULL;
        size_t dec_length = 0;
        if (hamming_decode(message, message_length, &dec, &dec_length) != ECC_OK) {
            aids_log(AIDS_ERROR, "Error decoding the message with Hamming Code");
            exit(EXIT_FAILURE);
        }

        AIDS_FREE(message);
        message = dec;
        message_length = dec_length;
    }

    if (message_length > 0) {
        if (args.output_path == NULL) {
            if (message_length > 32) {
                printf("Hidden message (first 32 bytes): ");
                for (size_t i = 0; i < 32 && i < message_length; i++) {
                    printf("%02x", message[i]);
                }
                printf("... (%zu bytes total)\n", message_length);
            } else {
                printf("Hidden message: %.*s\n", (int)message_length, message);
            }
        } else {
            Aids_String_Slice message_slice = {
                .str = (unsigned char *)message,
                .len = message_length
            };
            if (aids_io_write(args.output_path, &message_slice, "wb") != AIDS_OK) {
                aids_log(AIDS_ERROR, "Error writing message to output file: %s", aids_failure_reason());
                exit(EXIT_FAILURE);
            }
            aids_log(AIDS_INFO, "Hidden message written to %s", args.output_path);
        }
    } else {
        printf("No hidden message found in the image.\n");
    }

    if (bytes != NULL) {
        stbi_image_free(bytes);
        bytes = NULL;
    }
    if (message != NULL) {
        AIDS_FREE(message);
        message = NULL;
    }

    return 0;
}

typedef struct {
    const char *image_path;  // Path to the image file
    const char *output_path; // Path to save the modified image
    const char *payload_path; // Path to the payload file (default: stdin)
} Steg_Hide_Args_Fft;

static int command_hide_fft(int argc, char **argv) {
    Steg_Hide_Args_Fft args = {0};

    Argparse_Parser parser = {0};
    argparse_parser_init(&parser, PROGRAM_NAME " " COMMAND_HIDE_FFT, "Hide a message in an image using FFT", PROGRAM_VERSION);

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'i',
                                    .long_name = "image",
                                    .description = "Path to the image file",
                                    .type = ARGUMENT_TYPE_POSITIONAL,
                                    .required = true});
    argparse_add_argument(
        &parser,
        (Argparse_Options){.short_name = 'o',
                           .long_name = "output",
                           .description = "Path to save the modified image",
                           .type = ARGUMENT_TYPE_VALUE,
                           .required = false});
    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'p',
                                    .long_name = "payload",
                                    .description = "Path to the payload file (default: stdin)",
                                    .type = ARGUMENT_TYPE_VALUE,
                                    .required = false});

    if (argparse_parse(&parser, argc, argv) != ARG_OK) {
        argparse_print_help(&parser);
        return AIDS_ERR;
    }

    args.image_path = argparse_get_value(&parser, "image");
    args.output_path = argparse_get_value_or_default(&parser, "output", NULL);
    args.payload_path = argparse_get_value_or_default(&parser, "payload", NULL);

    argparse_parser_free(&parser);

    int width, height, num_chan;
    uint8_t *bytes = stbi_load(args.image_path, &width, &height, &num_chan, 0);
    if (bytes == NULL) {
        aids_log(AIDS_ERROR, "Error loading image: %s", stbi_failure_reason());
        exit(EXIT_FAILURE);
    }

    const uint8_t *payload = NULL;
    size_t payload_width = 0, payload_height = 0, payload_chan = 0;
    if (args.payload_path != NULL) {
        int width, height, num_chan;
        uint8_t *pimage_bytes = stbi_load(args.payload_path, &width, &height, &num_chan, 0);
        if (pimage_bytes == NULL) {
            aids_log(AIDS_ERROR, "Error loading payload image: %s", stbi_failure_reason());
            exit(EXIT_FAILURE);
        }

        payload = pimage_bytes;
        payload_width = width;
        payload_height = height;
        payload_chan = num_chan;
    } else {
        AIDS_TODO("Reading payload from stdin is not implemented for FFT hiding");
    }

    if (steg_hide_fft(bytes, width, height, num_chan, (const uint8_t *)payload, payload_width, payload_height, payload_chan) != STEG_OK) {
        aids_log(AIDS_ERROR, "Error hiding message in image: %s", steg_failure_reason());
        exit(EXIT_FAILURE);
    }

    if (stbi_write_png(args.output_path, width, height, num_chan, bytes, width * num_chan) == 0) {
        aids_log(AIDS_ERROR, "Error saving modified image: %s", stbi_failure_reason());
        exit(EXIT_FAILURE);
    }

    aids_log(AIDS_INFO, "Message hidden successfully in %s", args.output_path);

    if (bytes != NULL) {
        stbi_image_free(bytes);
    }

    if (payload != NULL) {
        if (args.payload_path != NULL) {
            stbi_image_free((void *)payload);
        } else {
            AIDS_FREE((void *)payload);
        }
    }

    return 0;
}

typedef struct {
    const char *og_image_path; // Path to the original image file
    const char *image_path; // Path to the image file
    const char *output_path; // Path to save the modified image (default: stdout)
} Steg_Show_Args_Fft;

static int command_show_fft(int argc, char **argv) {
    Steg_Show_Args_Fft args = {0};
    uint8_t *message = NULL;

    Argparse_Parser parser = {0};
    argparse_parser_init(&parser, PROGRAM_NAME " " COMMAND_SHOW_FFT, "Show a hidden message in an image using FFT", PROGRAM_VERSION);

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'g',
                                    .long_name = "og-image",
                                    .description = "Path to the original image file",
                                    .type = ARGUMENT_TYPE_POSITIONAL,
                                    .required = true});

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'i',
                                    .long_name = "image",
                                    .description = "Path to the image file",
                                    .type = ARGUMENT_TYPE_POSITIONAL,
                                    .required = true});
    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'o',
                                    .long_name = "output",
                                    .description = "Path to save the modified image (default: stdout)",
                                    .type = ARGUMENT_TYPE_VALUE,
                                    .required = false});

    if (argparse_parse(&parser, argc, argv) != ARG_OK) {
        argparse_print_help(&parser);
        exit(EXIT_FAILURE);
    }

    args.og_image_path = argparse_get_value(&parser, "og-image");
    args.image_path = argparse_get_value(&parser, "image");
    args.output_path = argparse_get_value_or_default(&parser, "output", NULL);

    argparse_parser_free(&parser);

    int width, height, num_chan;
    uint8_t *bytes = stbi_load(args.image_path, &width, &height, &num_chan, 0);
    if (bytes == NULL) {
        aids_log(AIDS_ERROR, "Error loading image: %s", stbi_failure_reason());
        exit(EXIT_FAILURE);
    }

    int og_width, og_height, og_num_chan;
    uint8_t *og_bytes = stbi_load(args.og_image_path, &og_width, &og_height, &og_num_chan, 0);
    if (og_bytes == NULL) {
        aids_log(AIDS_ERROR, "Error loading original image: %s", stbi_failure_reason());
        exit(EXIT_FAILURE);
    }
    AIDS_ASSERT(og_width == width && og_height == height, "Original image dimensions do not match the modified image dimensions");

    if (steg_show_fft(og_bytes, bytes, width, height, num_chan, &message) != STEG_OK) {
        aids_log(AIDS_ERROR, "Error showing message from image: %s", steg_failure_reason());
        exit(EXIT_FAILURE);
    }
    size_t message_length = width * height;

    if (message_length > 0) {
        if (args.output_path == NULL) {
            if (message_length > 32) {
                printf("Hidden message (first 32 bytes): ");
                for (size_t i = 0; i < 32 && i < message_length; i++) {
                    printf("%02x", message[i]);
                }
                printf("... (%zu bytes total)\n", message_length);
            } else {
                printf("Hidden message: %.*s\n", (int)message_length, message);
            }
        } else {
            stbi_write_png(args.output_path, width, height, num_chan, message, width * num_chan);
        }
    } else {
        printf("No hidden message found in the image.\n");
    }

    if (og_bytes != NULL) {
        stbi_image_free(og_bytes);
    }
    if (bytes != NULL) {
        stbi_image_free(bytes);
    }
    if (message != NULL) {
        AIDS_FREE(message);
    }

    return 0;
}

typedef struct {
    const char *image_path;  // Path to the image file
    const char *output_path; // Path to save the modified image
    const char *payload_path; // Path to the payload file (default: stdin)
    size_t compression_level; // Compression level to use (default: 1)
} Steg_Hide_Args_Dct;

static int command_hide_dct(int argc, char **argv) {
    Steg_Hide_Args_Dct args = {0};

    Argparse_Parser parser = {0};
    argparse_parser_init(&parser, PROGRAM_NAME " " COMMAND_HIDE_DCT, "Hide a message in an image using DCT", PROGRAM_VERSION);

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'i',
                                    .long_name = "image",
                                    .description = "Path to the image file",
                                    .type = ARGUMENT_TYPE_POSITIONAL,
                                    .required = true});
    argparse_add_argument(
        &parser,
        (Argparse_Options){.short_name = 'o',
                           .long_name = "output",
                           .description = "Path to save the modified image",
                           .type = ARGUMENT_TYPE_VALUE,
                           .required = false});
    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'p',
                                    .long_name = "payload",
                                    .description = "Path to the payload file (default: stdin)",
                                    .type = ARGUMENT_TYPE_VALUE,
                                    .required = false});

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'c',
                                    .long_name = "compression",
                                    .description = "Compression level (default: 1)",
                                    .type = ARGUMENT_TYPE_VALUE,
                                    .required = false});

    if (argparse_parse(&parser, argc, argv) != ARG_OK) {
        argparse_print_help(&parser);
        return AIDS_ERR;
    }

    args.image_path = argparse_get_value(&parser, "image");
    args.output_path = argparse_get_value_or_default(&parser, "output", NULL);
    args.payload_path = argparse_get_value_or_default(&parser, "payload", NULL);
    const char *compression_str = argparse_get_value_or_default(&parser, "compression", "1");
    args.compression_level = atoi(compression_str);

    argparse_parser_free(&parser);

    int width, height, num_chan;
    uint8_t *bytes = stbi_load(args.image_path, &width, &height, &num_chan, 0);
    if (bytes == NULL) {
        aids_log(AIDS_ERROR, "Error loading image: %s", stbi_failure_reason());
        exit(EXIT_FAILURE);
    }

    Aids_String_Slice payload_slice = {0};
    if (aids_io_read(args.payload_path, &payload_slice, "rb") != AIDS_OK) {
        aids_log(AIDS_ERROR, "Error reading payload file: %s", aids_failure_reason());
        exit(EXIT_FAILURE);
    }
    const uint8_t *payload = (const uint8_t *)payload_slice.str;
    size_t payload_length = payload_slice.len;

    if (steg_hide_dct(bytes, width, height, num_chan, (const uint8_t *)payload, payload_length, args.compression_level) != STEG_OK) {
        aids_log(AIDS_ERROR, "Error hiding message in image: %s", steg_failure_reason());
        exit(EXIT_FAILURE);
    }

    if (stbi_write_png(args.output_path, width, height, num_chan, bytes, width * num_chan) == 0) {
        aids_log(AIDS_ERROR, "Error saving modified image: %s", stbi_failure_reason());
        exit(EXIT_FAILURE);
    }

    aids_log(AIDS_INFO, "Message hidden successfully in %s", args.output_path);

    if (bytes != NULL) {
        stbi_image_free(bytes);
    }

    if (payload != NULL) {
        if (args.payload_path != NULL) {
            stbi_image_free((void *)payload);
        } else {
            AIDS_FREE((void *)payload);
        }
    }

    return 0;
}

typedef struct {
    const char *image_path; // Path to the image file
    const char *output_path; // Path to save the modified image (default: stdout)
    size_t compression_level; // Compression level to use (default: 1)
} Steg_Show_Args_Dct;

static int command_show_dct(int argc, char **argv) {
    Steg_Show_Args_Dct args = {0};
    uint8_t *message = NULL;
    size_t message_length = 0;

    Argparse_Parser parser = {0};
    argparse_parser_init(&parser, PROGRAM_NAME " " COMMAND_SHOW_DCT, "Show a hidden message in an image using DCT", PROGRAM_VERSION);

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'i',
                                    .long_name = "image",
                                    .description = "Path to the image file",
                                    .type = ARGUMENT_TYPE_POSITIONAL,
                                    .required = true});
    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'o',
                                    .long_name = "output",
                                    .description = "Path to save the modified image (default: stdout)",
                                    .type = ARGUMENT_TYPE_VALUE,
                                    .required = false});
    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'c',
                                    .long_name = "compression",
                                    .description = "Compression level (default: 1)",
                                    .type = ARGUMENT_TYPE_VALUE,
                                    .required = false});

    if (argparse_parse(&parser, argc, argv) != ARG_OK) {
        argparse_print_help(&parser);
        exit(EXIT_FAILURE);
    }

    args.image_path = argparse_get_value(&parser, "image");
    args.output_path = argparse_get_value_or_default(&parser, "output", NULL);
    const char *compression_str = argparse_get_value_or_default(&parser, "compression", "1");
    args.compression_level = atoi(compression_str);

    argparse_parser_free(&parser);

    int width, height, num_chan;
    uint8_t *bytes = stbi_load(args.image_path, &width, &height, &num_chan, 0);
    if (bytes == NULL) {
        aids_log(AIDS_ERROR, "Error loading image: %s", stbi_failure_reason());
        exit(EXIT_FAILURE);
    }

    if (steg_show_dct(bytes, width, height, num_chan, &message, &message_length, args.compression_level) != STEG_OK) {
        aids_log(AIDS_ERROR, "Error showing message from image: %s", steg_failure_reason());
        exit(EXIT_FAILURE);
    }

    if (message_length > 0) {
        if (args.output_path == NULL) {
            if (message_length > 32) {
                printf("Hidden message (first 32 bytes): ");
                for (size_t i = 0; i < 32 && i < message_length; i++) {
                    printf("%02x", message[i]);
                }
                printf("... (%zu bytes total)\n", message_length);
            } else {
                printf("Hidden message: %.*s\n", (int)message_length, message);
            }
        } else {
            Aids_String_Slice message_slice = {
                .str = (unsigned char *)message,
                .len = message_length
            };
            if (aids_io_write(args.output_path, &message_slice, "wb") != AIDS_OK) {
                aids_log(AIDS_ERROR, "Error writing message to output file: %s", aids_failure_reason());
                exit(EXIT_FAILURE);
            }
            aids_log(AIDS_INFO, "Hidden message written to %s", args.output_path);
        }
    } else {
        printf("No hidden message found in the image.\n");
    }

    if (bytes != NULL) {
        stbi_image_free(bytes);
    }
    if (message != NULL) {
        AIDS_FREE(message);
    }

    return 0;
}

typedef struct {
    const char *image_path;  // Path to the image file
    const char *output_path; // Path to save the modified image
    int compression_level;   // Compression level (default: 1)
} Steg_Noise_Args_Lsb;

static int command_noise_lsb(int argc, char **argv) {
    srand(time(NULL));

    Steg_Noise_Args_Lsb args = {0};

    Argparse_Parser parser = {0};
    argparse_parser_init(&parser, PROGRAM_NAME " " COMMAND_NOISE_LSB, "Simulate noise in LSB", PROGRAM_VERSION);

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'i',
                                    .long_name = "image",
                                    .description = "Path to the image file",
                                    .type = ARGUMENT_TYPE_POSITIONAL,
                                    .required = true});
    argparse_add_argument(
        &parser,
        (Argparse_Options){.short_name = 'o',
                           .long_name = "output",
                           .description = "Path to save the modified image (default: stdout)",
                           .type = ARGUMENT_TYPE_VALUE,
                           .required = true});

    argparse_add_argument(
        &parser, (Argparse_Options){.short_name = 'c',
                                    .long_name = "compression",
                                    .description = "Compression level (default: 1)",
                                    .type = ARGUMENT_TYPE_VALUE,
                                    .required = false});

    if (argparse_parse(&parser, argc, argv) != ARG_OK) {
        argparse_print_help(&parser);
        exit(EXIT_FAILURE);
    }

    args.image_path = argparse_get_value(&parser, "image");
    args.output_path = argparse_get_value_or_default(&parser, "output", NULL);
    const char *compression_str = argparse_get_value_or_default(&parser, "compression", "1");
    args.compression_level = atoi(compression_str);

    argparse_parser_free(&parser);

    int width, height, num_chan;
    uint8_t *bytes = stbi_load(args.image_path, &width, &height, &num_chan, 0);
    if (bytes == NULL) {
        aids_log(AIDS_ERROR, "Error loading image: %s", stbi_failure_reason());
        exit(EXIT_FAILURE);
    }

    // Insert dummy error
    size_t stride = 8 / args.compression_level;
    // TODO: add ecc on the length aswell to avoid this
    for (size_t i = 8 * sizeof(size_t); i < (size_t)(width * height * num_chan); i += stride) {
        if ((double)rand() / RAND_MAX >= 0.5) {
            size_t byte_index = i + rand() % stride;
            uint8_t byte = bytes[byte_index];
            size_t index = rand() % args.compression_level;
            byte = byte ^ (1 << index);
            bytes[byte_index] = byte;
            printf("%zu %zu\n", i, byte_index);
        }
    }

    if (stbi_write_png(args.output_path, width, height, num_chan, bytes, width * num_chan) == 0) {
        aids_log(AIDS_ERROR, "Error saving modified image: %s", stbi_failure_reason());
        exit(EXIT_FAILURE);
    }

    aids_log(AIDS_INFO, "Noise added successfully in %s", args.output_path);

    if (bytes != NULL) {
        stbi_image_free(bytes);
        bytes = NULL;
    }

    return 0;
}

static void usage() {
    fprintf(stdout, "usage: %s <SUBCOMMAND> [OPTIONS]\n", PROGRAM_NAME);
    fprintf(stdout, "    %s - Hide a message in an image using LSB\n", COMMAND_HIDE_LSB);
    fprintf(stdout, "    %s - Show a hidden message in an image using LSB\n", COMMAND_SHOW_LSB);
    fprintf(stdout, "    %s - Hide a message in an image using FFT\n", COMMAND_HIDE_FFT);
    fprintf(stdout, "    %s - Show a hidden message in an image using FFT\n", COMMAND_SHOW_FFT);
    fprintf(stdout, "    %s - Add noise in the LSB of the image\n", COMMAND_NOISE_LSB);
    fprintf(stdout, "    %s - Show the version of the program\n", COMMAND_VERSION);
    fprintf(stdout, "    %s - Show this help message\n", COMMAND_HELP);
    fprintf(stdout, "\n");
    fprintf(stdout, "You can use --help for more information on each command.\n");
    fprintf(stdout, "\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], COMMAND_HELP) == 0) {
        usage();
        return 0;
    } else if (strcmp(argv[1], COMMAND_VERSION) == 0) {
        fprintf(stdout, "%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
        return 0;
    } else if (strcmp(argv[1], COMMAND_HIDE_LSB) == 0) {
        return command_hide_lsb(argc - 1, argv + 1);
    } else if (strcmp(argv[1], COMMAND_SHOW_LSB) == 0) {
        return command_show_lsb(argc - 1, argv + 1);
    } else if (strcmp(argv[1], COMMAND_HIDE_FFT) == 0) {
        return command_hide_fft(argc - 1, argv + 1);
    } else if (strcmp(argv[1], COMMAND_SHOW_FFT) == 0) {
        return command_show_fft(argc - 1, argv + 1);
    } else if (strcmp(argv[1], COMMAND_HIDE_DCT) == 0) {
        return command_hide_dct(argc - 1, argv + 1);
    } else if (strcmp(argv[1], COMMAND_SHOW_DCT) == 0) {
        return command_show_dct(argc - 1, argv + 1);
    } else if (strcmp(argv[1], COMMAND_NOISE_LSB) == 0) {
        return command_noise_lsb(argc - 1, argv + 1);
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        usage();
        return 1;
    }
}

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define AIDS_IMPLEMENTATION
#include "aids.h"
#define ARGPARSE_IMPLEMENTATION
#include "argparse.h"
