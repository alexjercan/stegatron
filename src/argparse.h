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

#include "aids.h"
#include <stddef.h>

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
    char short_name;            // Short name of the argument
    char *long_name;            // Long name of the argument
    char *description;          // Description of the argument
    Argparse_Type type;         // Type of the argument
    bool required;              // Whether the argument is required
} Argparse_Options;

typedef struct {
    Argparse_Options options;
    union {
        char *value;
        unsigned int flag;
        Aids_Array values;
    };
} Argparse_Argument;

typedef struct {
    char *name;
    char *description;
    char *version;

    Aids_Array arguments;
} Argparse_Parser;

ARGHDEF void argparse_parser_init(Argparse_Parser *parser, char *name, char *description, char *version);
ARGHDEF Argparse_Result argparse_add_argument(Argparse_Parser *parser, Argparse_Options options);
ARGHDEF Argparse_Result argparse_parse(Argparse_Parser *parser, int argc, char **argv);
ARGHDEF char *argparse_get_value(Argparse_Parser *parser, char *name);
ARGHDEF char *argparse_get_value_or_default(Argparse_Parser *parser, char *name, char *default_value);
ARGHDEF unsigned int argparse_get_flag(Argparse_Parser *parser, char *name);
ARGHDEF Argparse_Result argparse_get_values(Argparse_Parser *parser, char *name, Aids_Array *values);
ARGHDEF void argparse_print_help(Argparse_Parser *parser);
ARGHDEF void argparse_print_version(Argparse_Parser *parser);
ARGHDEF void argparse_parser_free(Argparse_Parser *parser);

ARGHDEF const char * argparse_failure_reason(void);

#endif // ARGPARSE_H

#ifdef ARGPARSE_IMPLEMENTATION

static const char *argparse__g_failure_reason;

ARGHDEF void argparse_parser_init(Argparse_Parser *parser, char *name, char *description, char *version) {
    parser->name = name;
    parser->description = description;
    parser->version = version;
    aids_array_init(&parser->arguments, sizeof(Argparse_Argument));

    argparse_add_argument(
        parser,
        (Argparse_Options){.short_name = 'v',
                              .long_name = "version",
                              .description = "print the program version",
                              .type = ARGUMENT_TYPE_FLAG,
                              .required = false});
    argparse_add_argument(
        parser, (Argparse_Options){.short_name = 'h',
                                      .long_name = "help",
                                      .description = "print this help message",
                                      .type = ARGUMENT_TYPE_FLAG,
                                      .required = false});
}

ARGHDEF Argparse_Result argparse_add_argument(Argparse_Parser *parser, Argparse_Options options) {
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
        aids_array_init(&arg.values, sizeof(char *));
        break;
    case ARGUMENT_TYPE_VALUE_ARRAY:
        aids_array_init(&arg.values, sizeof(char *));
        break;
    }

    if (aids_array_append(&parser->arguments, (unsigned char *)&arg) != AIDS_OK) {
        argparse__g_failure_reason = "Failed to add argument";
        return ARG_ERR;
    }

    return ARG_OK;
}

static Argparse_Result argparse__validate_parser(Argparse_Parser *parser) {
    Argparse_Result result = ARG_OK;
    bool found_optional_positional = false;
    bool found_positional_rest = false;

    for (unsigned int i = 0; i < parser->arguments.count; i++) {
        Argparse_Argument *item = NULL;
        if (aids_array_get(&parser->arguments, i, (unsigned char **)&item) != AIDS_OK) {
            argparse__g_failure_reason = "Failed to get argument";
            return_defer(ARG_ERR);
        }
        Argparse_Options options = item->options;

        if (options.type == ARGUMENT_TYPE_POSITIONAL && found_positional_rest) {
            argparse__g_failure_reason = aids_temp_sprintf(
                "positional argument after positional rest: %s", options.long_name);
            result = ARG_ERR;
        }

        if (options.type == ARGUMENT_TYPE_POSITIONAL_REST &&
            found_positional_rest) {
            argparse__g_failure_reason = "multiple positional rest arguments";
            result = ARG_ERR;
        }

        if (options.type == ARGUMENT_TYPE_POSITIONAL_REST && !found_positional_rest) {
            found_positional_rest = true;
        }

        if (options.type == ARGUMENT_TYPE_POSITIONAL && !options.required) {
            found_optional_positional = true;
        }

        if (options.short_name == '\0' && options.long_name == NULL) {
            argparse__g_failure_reason = aids_temp_sprintf(
                "no short_name and long_name for argument %du", i);
            result = ARG_ERR;
        }
        if (options.type == ARGUMENT_TYPE_FLAG && options.required) {
            argparse__g_failure_reason = aids_temp_sprintf(
                "flag argument cannot be required: %s", options.long_name);
            result = ARG_ERR;
        }
        if (options.type == ARGUMENT_TYPE_POSITIONAL && options.required && found_optional_positional) {
            argparse__g_failure_reason = aids_temp_sprintf(
                "required positional argument after optional: %s", options.long_name);
            result = ARG_ERR;
        }
    }

defer:
    return result;
}

static Argparse_Result argparse__post_validate_parser(Argparse_Parser *parser) {
    Argparse_Result result = ARG_OK;

    for (unsigned int i = 0; i < parser->arguments.count; i++) {
        Argparse_Argument *item = NULL;
        if (aids_array_get(&parser->arguments, i, (unsigned char **)&item) != AIDS_OK) {
            argparse__g_failure_reason = "Failed to get argument";
            return_defer(ARG_ERR);
        }
        Argparse_Options options = item->options;

        if (options.type == ARGUMENT_TYPE_POSITIONAL && options.required) {
            if (item->value == NULL) {
                argparse__g_failure_reason = aids_temp_sprintf(
                    "missing required positional argument: %s", options.long_name);
                return_defer(ARG_ERR);
            }
        }

        if (options.type == ARGUMENT_TYPE_VALUE && options.required) {
            if (item->value == NULL) {
                argparse__g_failure_reason = aids_temp_sprintf(
                    "missing required argument: --%s", options.long_name);
                return_defer(ARG_ERR);
            }
        }

        if (options.type == ARGUMENT_TYPE_VALUE_ARRAY && options.required) {
            if (item->values.count == 0) {
                argparse__g_failure_reason = aids_temp_sprintf(
                    "missing required argument: --%s", options.long_name);
                return_defer(ARG_ERR);
            }
        }

        if (options.type == ARGUMENT_TYPE_POSITIONAL_REST && options.required) {
            if (item->values.count == 0) {
                argparse__g_failure_reason = aids_temp_sprintf(
                    "missing required positional rest argument: %s", options.long_name);
                return_defer(ARG_ERR);
            }
        }
    }

defer:
    return result;
}

static Argparse_Argument *argparse__get_option_arg(Argparse_Parser *parser, const char *name) {
    if (name[0] != '-') {
        argparse__g_failure_reason = aids_temp_sprintf("invalid argument name: %s", name);
        return NULL;
    }
    Argparse_Argument *arg = NULL;

    for (unsigned int j = 0; j < parser->arguments.count; j++) {
        Argparse_Argument *item = NULL;
        if (aids_array_get(&parser->arguments, j, (unsigned char **)&item) != AIDS_OK) {
            argparse__g_failure_reason = "Failed to get argument";
            break;
        }

        if ((name[1] == '-' && item->options.long_name != NULL &&
             strcmp(name + 2, item->options.long_name) == 0) ||
            (name[1] != '-' && item->options.short_name != '\0' &&
             name[1] == item->options.short_name)) {
            arg = item;
            break;
        }
    }

    if (arg == NULL) {
        argparse__g_failure_reason = aids_temp_sprintf("unknown argument: %s", name);
        return NULL;
    }

    return arg;
}

static Argparse_Argument *argparse__get_positional_arg(Argparse_Parser *parser, const char *name) {
    if (name[0] == '-') {
        argparse__g_failure_reason = aids_temp_sprintf("provided name is not a positional argument: %s", name);
        return NULL;
    }
    Argparse_Argument *arg = NULL;

    for (unsigned int j = 0; j < parser->arguments.count; j++) {
        Argparse_Argument *item = NULL;
        if (aids_array_get(&parser->arguments, j, (unsigned char **)&item) != AIDS_OK) {
            argparse__g_failure_reason = "Failed to get argument";
            break;
        }

        if (item->options.type == ARGUMENT_TYPE_POSITIONAL &&
            item->value == NULL) {
            arg = item;
            break;
        }

        if (item->options.type == ARGUMENT_TYPE_POSITIONAL_REST) {
            arg = item;
            break;
        }
    }

    if (arg == NULL) {
        argparse__g_failure_reason = aids_temp_sprintf("no positional argument available for: %s", name);
        return NULL;
    }

    return arg;
}

ARGHDEF Argparse_Result argparse_parse(Argparse_Parser *parser, int argc, char *argv[]) {
    Argparse_Result result = ARG_OK;

    if (argparse__validate_parser(parser) != ARG_OK) {
        return_defer(ARG_ERR);
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
            Argparse_Argument *arg = argparse__get_option_arg(parser, name);
            if (arg == NULL) {
                return_defer(ARG_ERR);
            }

            switch (arg->options.type) {
            case ARGUMENT_TYPE_FLAG: {
                arg->flag = 1;
                break;
            }
            case ARGUMENT_TYPE_VALUE: {
                if (i + 1 >= argc) {
                    argparse__g_failure_reason = aids_temp_sprintf("missing value for argument: %s", name);
                    return_defer(ARG_ERR);
                }

                arg->value = argv[++i];
                break;
            }
            case ARGUMENT_TYPE_VALUE_ARRAY: {
                if (i + 1 >= argc) {
                    argparse__g_failure_reason = aids_temp_sprintf("missing value for argument: %s", name);
                    return_defer(ARG_ERR);
                }

                if (aids_array_append(&arg->values, (const unsigned char *)&argv[++i]) != AIDS_OK) {
                    argparse__g_failure_reason = "failed to append value to value array";
                    return_defer(ARG_ERR);
                }
                break;
            }
            default: {
                argparse__g_failure_reason = aids_temp_sprintf("type not supported for argument: %s", name);
                return_defer(ARG_ERR);
            }
            }
        } else {
            Argparse_Argument *arg = argparse__get_positional_arg(parser, name);

            if (arg == NULL) {
                argparse__g_failure_reason = aids_temp_sprintf("no positional argument available for: %s", name);
                return_defer(ARG_ERR);
            }

            switch (arg->options.type) {
            case ARGUMENT_TYPE_POSITIONAL: {
                arg->value = name;
                break;
            }
            case ARGUMENT_TYPE_POSITIONAL_REST: {
                if (aids_array_append(&arg->values, (const unsigned char *)&name) != AIDS_OK) {
                    argparse__g_failure_reason = "failed to append value to positional rest array";
                    return_defer(ARG_ERR);
                }
                break;
            }
            default: {
                argparse__g_failure_reason = aids_temp_sprintf("type not supported for positional argument: %s", name);
                return_defer(ARG_ERR);
            }
            }

            arg->value = name;
        }
    }

    if (argparse__post_validate_parser(parser) != ARG_OK) {
        return_defer(ARG_ERR);
    }

defer:
    return result;
}

ARGHDEF char *argparse_get_value(Argparse_Parser *parser, char *long_name) {
    for (unsigned int i = 0; i < parser->arguments.count; i++) {
        Argparse_Argument *item = NULL;

        if (aids_array_get(&parser->arguments, i, (unsigned char **)&item) != AIDS_OK) {
            argparse__g_failure_reason = "Failed to get argument";
            return NULL;
        }

        if (item->options.long_name != NULL &&
            strcmp(long_name, item->options.long_name) == 0) {
            return item->value;
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
    for (unsigned int i = 0; i < parser->arguments.count; i++) {
        Argparse_Argument *item = NULL;
        if (aids_array_get(&parser->arguments, i, (unsigned char **)&item) != AIDS_OK) {
            argparse__g_failure_reason = "Failed to get argument";
            return 0;
        }

        if (item->options.long_name != NULL &&
            strcmp(long_name, item->options.long_name) == 0) {
            return item->flag;
        }
    }

    return 0;
}


ARGHDEF Argparse_Result argparse_get_values(Argparse_Parser *parser, char *name, Aids_Array *values) {
    Argparse_Result result = ARG_OK;

    for (unsigned int i = 0; i < parser->arguments.count; i++) {
        Argparse_Argument *item = NULL;
        if (aids_array_get(&parser->arguments, i, (unsigned char **)&item) != AIDS_OK) {
            argparse__g_failure_reason = "Failed to get argument";
            return_defer(ARG_ERR);
        }

        if (item->options.long_name != NULL &&
            strcmp(name, item->options.long_name) == 0) {
            *values = item->values;
            return_defer(ARG_OK);
        }
    }

defer:
    return result;
}

ARGHDEF void argparse_print_help(Argparse_Parser *parser) {
    fprintf(stdout, "usage: %s [options]", parser->name);

    for (unsigned int i = 0; i < parser->arguments.count; i++) {
        Argparse_Argument *item = NULL;
        if (aids_array_get(&parser->arguments, i, (unsigned char **)&item) != AIDS_OK) {
            break;
        }
        Argparse_Options options = item->options;

        if (options.type == ARGUMENT_TYPE_VALUE && options.required) {
            fprintf(stdout, " -%c <%s>", options.short_name, options.long_name);
        }
    }

    for (unsigned int i = 0; i < parser->arguments.count; i++) {
        Argparse_Argument *item = NULL;
        if (aids_array_get(&parser->arguments, i, (unsigned char **)&item) != AIDS_OK) {
            break;
        }
        Argparse_Options options = item->options;

        if (options.type == ARGUMENT_TYPE_POSITIONAL) {
            if (options.required) {
                fprintf(stdout, " <%s>", options.long_name);
            } else {
                fprintf(stdout, " [%s]", options.long_name);
            }
        }
    }

    for (unsigned int i = 0; i < parser->arguments.count; i++) {
        Argparse_Argument *item = NULL;
        if (aids_array_get(&parser->arguments, i, (unsigned char **)&item) != AIDS_OK) {
            break;
        }
        Argparse_Options options = item->options;

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

    for (unsigned int i = 0; i < parser->arguments.count; i++) {
        Argparse_Argument *item = NULL;
        if (aids_array_get(&parser->arguments, i, (unsigned char **)&item) != AIDS_OK) {
            break;
        }
        Argparse_Options options = item->options;

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

    for (unsigned int i = 0; i < parser->arguments.count; i++) {
        Argparse_Argument *item = NULL;
        if (aids_array_get(&parser->arguments, i, (unsigned char **)&item) != AIDS_OK) {
            break;
        }

        switch (item->options.type) {
        case ARGUMENT_TYPE_POSITIONAL: {
            fprintf(stdout, "  %c, %s\n", item->options.short_name,
                    item->options.long_name);
            fprintf(stdout, "      %s\n", item->options.description);
            fprintf(stdout, "\n");
            break;
        }
        case ARGUMENT_TYPE_POSITIONAL_REST: {
            fprintf(stdout, "  %c, %s\n", item->options.short_name,
                    item->options.long_name);
            fprintf(stdout, "      %s\n", item->options.description);
            fprintf(stdout, "\n");
            break;
        }
        case ARGUMENT_TYPE_FLAG: {
            fprintf(stdout, "  -%c, --%s\n", item->options.short_name,
                    item->options.long_name);
            fprintf(stdout, "      %s\n", item->options.description);
            fprintf(stdout, "\n");
            break;
        }
        case ARGUMENT_TYPE_VALUE: {
            fprintf(stdout, "  -%c, --%s <value>\n", item->options.short_name,
                    item->options.long_name);
            fprintf(stdout, "      %s\n", item->options.description);
            fprintf(stdout, "\n");
            break;
        }
        case ARGUMENT_TYPE_VALUE_ARRAY: {
            fprintf(stdout, "  -%c, --%s <value>...\n",
                    item->options.short_name, item->options.long_name);
            fprintf(stdout, "      %s\n", item->options.description);
            fprintf(stdout, "\n");
            break;
        }
        default: {
            AIDS_UNREACHABLE("Unknown argument type");
        }
        }
    }
}


ARGHDEF void argparse_print_version(Argparse_Parser *parser) {
    fprintf(stdout, "%s %s\n", parser->name, parser->version);
}

ARGHDEF void argparse_parser_free(Argparse_Parser *parser) {
    aids_array_free(&parser->arguments);

    parser->name = NULL;
    parser->description = NULL;
    parser->version = NULL;
}

ARGHDEF const char *argparse_failure_reason(void) { return argparse__g_failure_reason; }

#endif // ARGPARSE_IMPLEMENTATION
