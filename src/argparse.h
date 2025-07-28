#ifndef ARGPARSE_H
#define ARGPARSE_H

#ifndef ARGHDEF
#ifdef ARGH_STATIC
#define ARGHDEF static
#else
#define ARGHDEF extern
#endif
#endif

#ifndef ARGPARSE_CAPACITY
#define ARGPARSE_CAPACITY 256
#endif // ARGPARSE_CAPACITY

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#ifndef CLITERAL
#define CLITERAL(type, value) ((type)(value))
#endif // CLITERAL

typedef enum {
    ARG_OK = 0,
    ARG_ERR = 1,
} Argparse_Result;

typedef enum {
    ARGUMENT_TYPE_VALUE,           // Argument with a value
    ARGUMENT_TYPE_FLAG,            // Flag argument
    ARGUMENT_TYPE_POSITIONAL,      // Positional argument
    ARGUMENT_TYPE_POSITIONAL_REST, // Positional argument that consumes the rest
    ARGUMENT_TYPE_VALUE_ARRAY,     // Argument with an array of values
} Argparse_Type;

typedef struct {
    char short_name;    // Short name of the argument
    char *long_name;    // Long name of the argument
    char *description;  // Description of the argument
    Argparse_Type type; // Type of the argument
    int required;       // Whether the argument is required
} Argparse_Options;

typedef struct {
    char *values[ARGPARSE_CAPACITY];
    unsigned long count;
} Argparse_Array;

typedef struct {
    Argparse_Options options;
    union {
        char *value;            // value for a named argument (or single positional)
        unsigned int flag;      // set value for a flag
        Argparse_Array values;  // values for array arguments
    };
} Argparse_Argument;

typedef struct {
    char *name;
    char *description;
    char *version;

    Argparse_Argument arguments[ARGPARSE_CAPACITY];
    unsigned long count;
} Argparse_Parser;

ARGHDEF void argparse_parser_init(Argparse_Parser *parser, char *name, char *description, char *version);
ARGHDEF void argparse_add_argument(Argparse_Parser *parser, Argparse_Options options);
ARGHDEF Argparse_Result argparse_parse(Argparse_Parser *parser, int argc, char **argv);
ARGHDEF char *argparse_get_value(Argparse_Parser *parser, char *name);
ARGHDEF char *argparse_get_value_or_default(Argparse_Parser *parser, char *name, char *default_value);
ARGHDEF unsigned int argparse_get_flag(Argparse_Parser *parser, char *name);
ARGHDEF unsigned long argparse_get_values(Argparse_Parser *parser, char *name, char **values);

ARGHDEF void argparse_print_help(Argparse_Parser *parser);
ARGHDEF void argparse_print_version(Argparse_Parser *parser);
ARGHDEF void argparse_parser_free(Argparse_Parser *parser);

#endif // ARGPARSE_H

#ifdef ARGPARSE_IMPLEMENTATION

ARGHDEF void argparse_parser_init(Argparse_Parser *parser, char *name, char *description, char *version) {
    parser->name = name;
    parser->description = description;
    parser->version = version;
    memset(&parser->arguments, 0, ARGPARSE_CAPACITY * sizeof(Argparse_Argument));
    parser->count = 0;

    argparse_add_argument(
        parser,
        (Argparse_Options){.short_name = 'v',
                           .long_name = "version",
                           .description = "print the program version",
                           .type = ARGUMENT_TYPE_FLAG,
                           .required = 0});
    argparse_add_argument(
        parser, (Argparse_Options){.short_name = 'h',
                                   .long_name = "help",
                                   .description = "print this help message",
                                   .type = ARGUMENT_TYPE_FLAG,
                                   .required = 0});
}


ARGHDEF void argparse_add_argument(Argparse_Parser *parser, Argparse_Options options) {
    assert((parser->count < ARGPARSE_CAPACITY) && "Maximum number of arguments exceeded");

    Argparse_Argument arg = {
        .options = options,
    };

    switch (options.type) {
    case ARGUMENT_TYPE_VALUE:
        arg.value = NULL;
        break;
    case ARGUMENT_TYPE_FLAG:
        arg.flag = 0;
        break;
    case ARGUMENT_TYPE_POSITIONAL:
        arg.value = NULL;
        break;
    case ARGUMENT_TYPE_POSITIONAL_REST:
        memset(&arg.values, 0, sizeof(Argparse_Array));
        arg.values.count = 0;
        break;
    case ARGUMENT_TYPE_VALUE_ARRAY:
        memset(&arg.values, 0, sizeof(Argparse_Array));
        arg.values.count = 0;
        break;
    default:
        fprintf(stderr, "Error: unknown argument type: %d\n", options.type);
        exit(EXIT_FAILURE);
    }

    parser->arguments[parser->count++] = arg;
}

static Argparse_Result argparse__validate_parser(Argparse_Parser *parser) {
    int found_optional_positional = 0;
    int found_positional_rest = 0;

    for (unsigned long i = 0; i < parser->count; i++) {
        Argparse_Argument item = parser->arguments[i];
        Argparse_Options options = item.options;

        if (options.type == ARGUMENT_TYPE_POSITIONAL && found_positional_rest) {
            fprintf(stderr, "Error: positional argument after positional rest: %s\n", options.long_name);
            return ARG_ERR;
        }

        if (options.type == ARGUMENT_TYPE_POSITIONAL_REST &&
            found_positional_rest) {
            fprintf(stderr, "Error: multiple positional rest arguments: %s\n", options.long_name);
            return ARG_ERR;
        }

        if (options.type == ARGUMENT_TYPE_POSITIONAL_REST && !found_positional_rest) {
            found_positional_rest = 1;
        }

        if (options.type == ARGUMENT_TYPE_POSITIONAL && !options.required) {
            found_optional_positional = 1;
        }

        if (options.short_name == '\0' && options.long_name == NULL) {
            fprintf(stderr, "Error: no short_name and long_name for argument %lu\n", i);
            return ARG_ERR;
        }
        if (options.type == ARGUMENT_TYPE_FLAG && options.required) {
            fprintf(stderr, "Error: flag argument cannot be required: %s\n", options.long_name);
            return ARG_ERR;
        }
        if (options.type == ARGUMENT_TYPE_POSITIONAL && options.required && found_optional_positional) {
            fprintf(stderr, "Error: required positional argument after optional: %s\n", options.long_name);
            return ARG_ERR;
        }
    }

    return ARG_OK;
}

static Argparse_Result argparse__post_validate_parser(Argparse_Parser *parser) {
    for (unsigned int i = 0; i < parser->count; i++) {
        Argparse_Argument item = parser->arguments[i];
        Argparse_Options options = item.options;

        if (options.type == ARGUMENT_TYPE_POSITIONAL && options.required) {
            if (item.value == NULL) {
                fprintf(stderr, "Error: missing required positional argument: %s\n", options.long_name);
                return ARG_ERR;
            }
        }

        if (options.type == ARGUMENT_TYPE_VALUE && options.required) {
            if (item.value == NULL) {
                fprintf(stderr, "Error: missing required argument: --%s\n", options.long_name);
                return ARG_ERR;
            }
        }

        if (options.type == ARGUMENT_TYPE_VALUE_ARRAY && options.required) {
            if (item.values.count == 0) {
                fprintf(stderr, "Error: missing required argument: --%s\n", options.long_name);
                return ARG_ERR;
            }
        }

        if (options.type == ARGUMENT_TYPE_POSITIONAL_REST && options.required) {
            if (item.values.count == 0) {
                fprintf(stderr, "Error: missing required positional rest argument: %s\n", options.long_name);
                return ARG_ERR;
            }
        }
    }

    return ARG_OK;
}

static Argparse_Result argparse__get_option_arg(Argparse_Parser *parser, const char *name, Argparse_Argument **arg) {
    assert(name[0] == '-' && "argument name must start with '-'");

    for (unsigned int j = 0; j < parser->count; j++) {
        Argparse_Argument *item = parser->arguments + j;
        if ((name[1] == '-' && item->options.long_name != NULL &&
             strcmp(name + 2, item->options.long_name) == 0) ||
            (name[1] != '-' && item->options.short_name != '\0' &&
             name[1] == item->options.short_name)) {
            *arg = item;
            break;
        }
    }

    if (*arg == NULL) {
        fprintf(stderr, "Error: unknown argument: %s\n", name);
        return ARG_ERR;
    }

    return ARG_OK;
}

static Argparse_Result argparse__get_positional_arg(Argparse_Parser *parser, const char *name, Argparse_Argument **arg) {
    assert(name[0] != '-' && "positional argument must not start with '-'");

    for (unsigned int j = 0; j < parser->count; j++) {
        Argparse_Argument *item = parser->arguments + j;

        if (item->options.type == ARGUMENT_TYPE_POSITIONAL && item->value == NULL) {
            *arg = item;
            return ARG_OK;
        }

        if (item->options.type == ARGUMENT_TYPE_POSITIONAL_REST) {
            *arg = item;
            return ARG_OK;
        }
    }

    if (*arg == NULL) {
        fprintf(stderr, "Error: no positional argument available for: %s\n", name);
        return ARG_ERR;
    }

    return ARG_OK;
}

ARGHDEF Argparse_Result argparse_parse(Argparse_Parser *parser, int argc, char *argv[]) {
    if (argparse__validate_parser(parser) != ARG_OK) {
        return ARG_ERR;
    }

    for (int i = 1; i < argc; i++) {
        char *name = argv[i];

        if (strcmp(name, "-h") == 0 || strcmp(name, "--help") == 0) {
            argparse_print_help(parser);
            exit(0);
        }

        if (strcmp(name, "-v") == 0 || strcmp(name, "--version") == 0) {
            argparse_print_version(parser);
            exit(0);
        }

        if (name[0] == '-') {
            Argparse_Argument *arg = NULL;
            if (argparse__get_option_arg(parser, name, &arg) != ARG_OK) {
                return ARG_ERR;
            }

            switch (arg->options.type) {
            case ARGUMENT_TYPE_FLAG:
                arg->flag = 1;
                break;
            case ARGUMENT_TYPE_VALUE:
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: missing value for argument: %s\n", name);
                    return ARG_ERR;
                }
                arg->value = argv[++i];
                break;
            case ARGUMENT_TYPE_VALUE_ARRAY:
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: missing value for argument: %s\n", name);
                    return ARG_ERR;
                }

                assert(arg->values.count < ARGPARSE_CAPACITY && "Maximum number of values exceeded for argument");
                arg->values.values[arg->values.count++] = argv[++i];
                break;
            default:
                fprintf(stderr, "Error: argument type not supported: %s\n", name);
                return ARG_ERR;
            }
        } else {
            Argparse_Argument *arg = NULL;
            if (argparse__get_positional_arg(parser, name, &arg) != ARG_OK) {
                return ARG_ERR;
            }

            switch (arg->options.type) {
            case ARGUMENT_TYPE_POSITIONAL:
                arg->value = name;
                break;
            case ARGUMENT_TYPE_POSITIONAL_REST:
                assert(arg->values.count < ARGPARSE_CAPACITY && "Maximum number of values exceeded for positional rest argument");
                arg->values.values[arg->values.count++] = name;
                break;
            default:
                fprintf(stderr, "Error: argument type not supported: %s\n", name);
                return ARG_ERR;
            }
        }
    }

    if (argparse__post_validate_parser(parser) != ARG_OK) {
        return ARG_ERR;
    }

    return ARG_OK;
}

ARGHDEF char *argparse_get_value(Argparse_Parser *parser, char *long_name) {
    for (unsigned int i = 0; i < parser->count; i++) {
        Argparse_Argument item = parser->arguments[i];
        if (item.options.long_name != NULL &&
            strcmp(long_name, item.options.long_name) == 0) {
            return item.value;
        }
    }

    return NULL;
}

ARGHDEF char *argparse_get_value_or_default(Argparse_Parser *parser, char *name, char *default_) {
    char *value = argparse_get_value(parser, name);
    if (value == NULL) {
        value = default_;
    }

    return value;
}

ARGHDEF unsigned int argparse_get_flag(Argparse_Parser *parser, char *long_name) {
    for (unsigned int i = 0; i < parser->count; i++) {
        Argparse_Argument item = parser->arguments[i];
        if (item.options.long_name != NULL &&
            strcmp(long_name, item.options.long_name) == 0) {
            return item.flag;
        }
    }

    return 0;
}

ARGHDEF unsigned long argparse_get_values(Argparse_Parser *parser, char *name, char **values) {
    for (unsigned int i = 0; i < parser->count; i++) {
        Argparse_Argument item = parser->arguments[i];
        if (item.options.long_name != NULL &&
            strcmp(name, item.options.long_name) == 0) {
            memcpy(values, item.values.values, item.values.count * sizeof(char *));
            return item.values.count;
        }
    }

    assert(0 && "No values found for the given name");
}

ARGHDEF void argparse_print_help(Argparse_Parser *parser) {
    fprintf(stdout, "usage: %s [options]", parser->name);

    for (unsigned int i = 0; i < parser->count; i++) {
        Argparse_Argument item = parser->arguments[i];
        Argparse_Options options = item.options;

        if (options.type == ARGUMENT_TYPE_VALUE && options.required) {
            fprintf(stdout, " -%c <%s>", options.short_name, options.long_name);
        }
    }

    for (unsigned int i = 0; i < parser->count; i++) {
        Argparse_Argument item = parser->arguments[i];
        Argparse_Options options = item.options;

        if (options.type == ARGUMENT_TYPE_POSITIONAL) {
            if (options.required) {
                fprintf(stdout, " <%s>", options.long_name);
            } else {
                fprintf(stdout, " [%s]", options.long_name);
            }
        }
    }

    for (unsigned int i = 0; i < parser->count; i++) {
        Argparse_Argument item = parser->arguments[i];
        Argparse_Options options = item.options;

        if (options.type == ARGUMENT_TYPE_VALUE_ARRAY) {
            if (options.required) {
                fprintf(stdout, " -%c <%s>...", options.short_name,
                        options.long_name);
            } else {
                fprintf(stdout, " -%c [%s]...", options.short_name,
                        options.long_name);
            }
        }
    }

    for (unsigned int i = 0; i < parser->count; i++) {
        Argparse_Argument item = parser->arguments[i];
        Argparse_Options options = item.options;

        if (options.type == ARGUMENT_TYPE_POSITIONAL_REST) {
            if (options.required) {
                fprintf(stdout, " <%s>...", options.long_name);
            } else {
                fprintf(stdout, " [%s]...", options.long_name);
            }
        }
    }

    fprintf(stdout, "\n");
    fprintf(stdout, "%s\n", parser->description);
    fprintf(stdout, "\n");
    fprintf(stdout, "options:\n");

    for (unsigned int i = 0; i < parser->count; i++) {
        Argparse_Argument item = parser->arguments[i];
        Argparse_Options options = item.options;

        switch (options.type) {
        case ARGUMENT_TYPE_POSITIONAL:
            fprintf(stdout, "  %c, %s\n", options.short_name, options.long_name);
            fprintf(stdout, "      %s\n", options.description);
            fprintf(stdout, "\n");
            break;
        case ARGUMENT_TYPE_POSITIONAL_REST:
            fprintf(stdout, "  %c, %s\n", options.short_name, options.long_name);
            fprintf(stdout, "      %s\n", options.description);
            fprintf(stdout, "\n");
            break;
        case ARGUMENT_TYPE_FLAG:
            fprintf(stdout, "  -%c, --%s\n", options.short_name, options.long_name);
            fprintf(stdout, "      %s\n", options.description);
            fprintf(stdout, "\n");
            break;
        case ARGUMENT_TYPE_VALUE:
            fprintf(stdout, "  -%c, --%s <value>\n", options.short_name, options.long_name);
            fprintf(stdout, "      %s\n", options.description);
            fprintf(stdout, "\n");
            break;
        case ARGUMENT_TYPE_VALUE_ARRAY:
            fprintf(stdout, "  -%c, --%s <value>...\n", options.short_name, options.long_name);
            fprintf(stdout, "      %s\n", options.description);
            fprintf(stdout, "\n");
            break;
        default:
            fprintf(stderr, "Error: unknown argument type for: %s\n", options.long_name);
            break;
        }
    }
}

ARGHDEF void argparse_print_version(Argparse_Parser *parser) {
    fprintf(stdout, "%s %s\n", parser->name, parser->version);
}

ARGHDEF void argparse_parser_free(Argparse_Parser *parser) {
    parser->name = NULL;
    parser->description = NULL;
    parser->version = NULL;
    memset(&parser->arguments, 0, ARGPARSE_CAPACITY * sizeof(Argparse_Argument));
    parser->count = 0;
}

#endif // ARGPARSE_IMPLEMENTATION
