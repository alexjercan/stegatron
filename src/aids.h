#ifndef AIDS_H
#define AIDS_H

#ifndef AIDSHDEF
#ifdef AIDSH_STATIC
#define AIDSHDEF static
#else
#define AIDSHDEF extern
#endif
#endif

#ifndef AIDS_REALLOC
#include <stdlib.h>
#define AIDS_REALLOC realloc
#endif // AIDS_REALLOC

#ifndef AIDS_FREE
#include <stdlib.h>
#define AIDS_FREE free
#endif // AIDS_FREE

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

#if defined(__GNUC__) || defined(__clang__)
//   https://gcc.gnu.org/onlinedocs/gcc-4.7.2/gcc/Function-Attributes.html
#    ifdef __MINGW_PRINTF_FORMAT
#        define AIDS_PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (__MINGW_PRINTF_FORMAT, STRING_INDEX, FIRST_TO_CHECK)))
#    else
#        define AIDS_PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK) __attribute__ ((format (printf, STRING_INDEX, FIRST_TO_CHECK)))
#    endif // __MINGW_PRINTF_FORMAT
#else
//   TODO: implement AIDS_PRINTF_FORMAT for MSVC
#    define AIDS_PRINTF_FORMAT(STRING_INDEX, FIRST_TO_CHECK)
#endif

#define AIDS_UNUSED(x) (void)(x)
#define AIDS_TODO(message)                                                     \
    do {                                                                       \
        fprintf(stderr, "%s:%d: TODO: %s\n", __FILE__, __LINE__, message);     \
        exit(EXIT_FAILURE);                                                    \
    } while (0)
#define AIDS_UNREACHABLE(message)                                                \
    do {                                                                       \
        fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, message); \
        exit(EXIT_FAILURE);                                                    \
    } while (0)

typedef enum {
    AIDS_INFO,
    AIDS_WARNING,
    AIDS_ERROR,
    AIDS_NO_LOGS,
} Aids_Log_Level;

#ifdef AIDS_NO_TERMINAL_COLORS
#define AIDS_TERMINAL_RED ""
#define AIDS_TERMINAL_YELLOW ""
#define AIDS_TERMINAL_BLUE ""
#define AIDS_TERMINAL_RESET ""
#else
#define AIDS_TERMINAL_RED "\033[1;31m"
#define AIDS_TERMINAL_YELLOW "\033[1;33m"
#define AIDS_TERMINAL_BLUE "\033[1;34m"
#define AIDS_TERMINAL_RESET "\033[0m"
#endif

void aids_log_msg(Aids_Log_Level level, const char* file, int line, const char *fmt, ...) AIDS_PRINTF_FORMAT(4, 5);
#define aids_log(level, fmt, ...) aids_log_msg(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

typedef int bool;
#ifndef true
#define true 1
#endif // true
#ifndef false
#define false 0
#endif // false

typedef enum {
    AIDS_OK = 0,
    AIDS_ERR = 1,
} Aids_Result;

#ifndef return_defer
#define return_defer(code)                                                     \
    do {                                                                       \
        result = (code);                                                       \
        goto defer;                                                            \
    } while (0)
#endif // return_defer

#ifndef AIDS_TEMP_CAPACITY
#define AIDS_TEMP_CAPACITY (8*1024*1024)
#endif // AIDS_TEMP_CAPACITY
AIDSHDEF void *aids_temp_alloc(size_t size);
AIDSHDEF char *aids_temp_sprintf(const char *format, ...) AIDS_PRINTF_FORMAT(1, 2);
AIDSHDEF void aids_temp_reset(void);
AIDSHDEF const char *aids_failure_reason(void);

#ifndef AIDS_ARRAY_INIT_CAPACITY
#define AIDS_ARRAY_INIT_CAPACITY 16
#endif // AIDS_ARRAY_INIT_CAPACITY

typedef struct {
    unsigned char *items;
    unsigned long item_size;
    unsigned long count;
    unsigned long capacity;
} Aids_Array;

AIDSHDEF void aids_array_init(Aids_Array *da, unsigned long item_size);
AIDSHDEF Aids_Result aids_array_append(Aids_Array *da, const unsigned char *item);
AIDSHDEF Aids_Result aids_array_append_many(Aids_Array *da, const unsigned char *items, unsigned long count);
AIDSHDEF Aids_Result aids_array_get(const Aids_Array *da, unsigned long index, unsigned char **item);
AIDSHDEF void aids_array_free(Aids_Array *da);

typedef struct {
    unsigned char *str;
    unsigned long len;
} Aids_String_Slice;

AIDSHDEF void aids_string_slice_init(Aids_String_Slice *ss, const char *str, unsigned long len);
AIDSHDEF bool ds_string_slice_tokenize(Aids_String_Slice *ss, char delimiter, Aids_String_Slice *token);
AIDSHDEF Aids_Result aids_string_slice_to_cstr(const Aids_String_Slice *ss, char **cstr);
AIDSHDEF void aids_string_slice_free(Aids_String_Slice *ss);

typedef struct {
    Aids_Array items;
} Aids_String_Builder;

AIDSHDEF void aids_string_builder_init(Aids_String_Builder *sb);
AIDSHDEF Aids_Result aids_string_builder_append(Aids_String_Builder *sb, const char *format, ...) AIDS_PRINTF_FORMAT(2, 3);
AIDSHDEF Aids_Result aids_string_builder_append_slice(Aids_String_Builder *sb, const Aids_String_Slice *slice);
AIDSHDEF Aids_Result aids_string_builder_appendc(Aids_String_Builder *sb, char c);
AIDSHDEF Aids_Result aids_string_builder_to_cstr(const Aids_String_Builder *sb, char **cstr);
AIDSHDEF void aids_string_builder_to_slice(const Aids_String_Builder *sb, Aids_String_Slice *slice);
AIDSHDEF void aids_string_builder_free(Aids_String_Builder *sb);

#ifndef LINE_MAX
#define LINE_MAX 1024
#endif // LINE_MAX

AIDSHDEF Aids_Result aids_io_read(const char *filename, Aids_String_Slice *ss, const char *mode);
AIDSHDEF Aids_Result aids_io_write(const char *filename, const Aids_String_Slice *ss, const char *mode);

#endif // AIDS_H

#ifdef AIDS_IMPLEMENTATION

// TODO: Maybe include arena.h here
static size_t aids_temp_size = 0;
static char aids_temp[AIDS_TEMP_CAPACITY] = {0};

static const char *aids__g_failure_reason;

void aids_log_msg(Aids_Log_Level level, const char* file, int line, const char *fmt, ...)
{
    switch (level) {
    case AIDS_INFO:
        fprintf(stderr, AIDS_TERMINAL_BLUE "INFO " AIDS_TERMINAL_RESET ": %s:%d: ", file, line);
        break;
    case AIDS_WARNING:
        fprintf(stderr, AIDS_TERMINAL_YELLOW "[WARNING] " AIDS_TERMINAL_RESET ": %s:%d: ", file, line);
        break;
    case AIDS_ERROR:
        fprintf(stderr, AIDS_TERMINAL_RED "[ERROR] " AIDS_TERMINAL_RESET ": %s:%d: ", file, line);
        break;
    case AIDS_NO_LOGS: return;
    default:
        AIDS_UNREACHABLE("aids_log");
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

AIDSHDEF void *aids_temp_alloc(size_t size) {
    if (size == 0 || size > AIDS_TEMP_CAPACITY - aids_temp_size) {
        aids__g_failure_reason = "Temporary allocation size exceeds capacity";
        return NULL;
    }

    void *ptr = aids_temp + aids_temp_size;
    aids_temp_size += size;
    return ptr;
}

AIDSHDEF char *aids_temp_sprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (needed < 0) {
        aids__g_failure_reason = "Formatting error";
        return NULL;
    }

    char *buffer = (char *)aids_temp_alloc(needed + 1);
    if (buffer == NULL) {
        return NULL;
    }

    va_start(args, format);
    vsnprintf(buffer, needed + 1, format, args);
    va_end(args);

    return buffer;
}

AIDSHDEF void aids_temp_reset(void) {
    aids_temp_size = 0;
    memset(aids_temp, 0, AIDS_TEMP_CAPACITY);
}

AIDSHDEF const char *aids_failure_reason(void) {
    return aids__g_failure_reason;
}

AIDSHDEF void aids_array_init(Aids_Array *da, unsigned long item_size) {
    da->items = NULL;
    da->item_size = item_size;
    da->count = 0;
    da->capacity = 0;
}

AIDSHDEF Aids_Result aids_array_append(Aids_Array *da, const unsigned char *item) {
    Aids_Result result = AIDS_OK;

    if (da->count >= da->capacity) {
        unsigned long new_capacity = da->capacity * 2;
        if (new_capacity == 0) {
            new_capacity = AIDS_ARRAY_INIT_CAPACITY;
        }

        da->items = AIDS_REALLOC(da->items, new_capacity * da->item_size);

        if (da->items == NULL) {
            aids__g_failure_reason = "Memory allocation failed";
            return_defer(AIDS_ERR);
        }

        da->capacity = new_capacity;
    }

    memcpy(da->items + da->count * da->item_size, item, da->item_size);
    da->count++;

defer:
    return result;
}

AIDSHDEF Aids_Result aids_array_append_many(Aids_Array *da, const unsigned char *items, unsigned long count) {
    Aids_Result result = AIDS_OK;

    if (da->count + count > da->capacity) {
        unsigned long new_capacity = da->capacity * 2;
        if (new_capacity == 0) {
            new_capacity = AIDS_ARRAY_INIT_CAPACITY;
        }

        while (new_capacity < da->count + count) {
            new_capacity *= 2;
        }

        da->items = AIDS_REALLOC(da->items, new_capacity * da->item_size);

        if (da->items == NULL) {
            aids__g_failure_reason = "Memory allocation failed";
            return_defer(AIDS_ERR);
        }

        da->capacity = new_capacity;
    }

    memcpy(da->items + da->count * da->item_size, items, count * da->item_size);
    da->count += count;

defer:
    return result;
}

AIDSHDEF Aids_Result aids_array_get(const Aids_Array *da, unsigned long index, unsigned char **item) {
    Aids_Result result = AIDS_OK;

    if (index >= da->count) {
        aids__g_failure_reason = aids_temp_sprintf("Index %lu out of bounds (count: %lu)", index, da->count);
        return_defer(AIDS_ERR);
    }

    *item = da->items + index * da->item_size;

defer:
    return result;
}

AIDSHDEF void aids_array_free(Aids_Array *da) {
    if (da->items != NULL) {
        AIDS_FREE(da->items);
        da->items = NULL;
    }
    da->count = 0;
    da->capacity = 0;
}

AIDSHDEF void aids_string_slice_init(Aids_String_Slice *ss, const char *str, unsigned long len) {
    ss->str = (unsigned char *)str;
    ss->len = len;
}

AIDSHDEF bool ds_string_slice_tokenize(Aids_String_Slice *ss, char delimiter, Aids_String_Slice *token) {
    if (ss->len == 0 || ss->str == NULL) {
        return false;
    }

    token->str = ss->str;
    token->len = 0;

    for (unsigned long i = 0; i < ss->len; i++) {
        if (ss->str[i] == delimiter) {
            token->len = i;
            ss->str += (i + 1);
            ss->len -= (i + 1);
            return true;
        }
        token->len++;
    }

    token->len = ss->len;
    ss->str += ss->len;
    ss->len = 0;

    return true;
}

AIDSHDEF Aids_Result aids_string_slice_to_cstr(const Aids_String_Slice *ss, char **cstr) {
    Aids_Result result = AIDS_OK;

    *cstr = (char *)AIDS_REALLOC(NULL, ss->len + 1);
    if (*cstr == NULL) {
        aids__g_failure_reason = "Memory allocation failed";
        return_defer(AIDS_ERR);
    }

    memcpy(*cstr, ss->str, ss->len);
    (*cstr)[ss->len] = '\0';

defer:
    return result;
}

AIDSHDEF void aids_string_slice_free(Aids_String_Slice *ss) {
    ss->str = NULL;
    ss->len = 0;
}

AIDSHDEF void aids_string_builder_init(Aids_String_Builder *sb) {
    aids_array_init(&sb->items, sizeof(unsigned char));
}

AIDSHDEF Aids_Result aids_string_builder_append(Aids_String_Builder *sb, const char *format, ...) {
    Aids_Result result = AIDS_OK;

    char *buffer = NULL;

    va_list args;
    va_start(args, format);
    int needed = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (needed < 0) {
        aids__g_failure_reason = "Formatting error";
        return_defer(AIDS_ERR);
    }

    buffer = (char *)AIDS_REALLOC(NULL, needed + 1);
    if (buffer == NULL) {
        aids__g_failure_reason = "Memory allocation failed";
        return_defer(AIDS_ERR);
    }

    va_start(args, format);
    vsnprintf(buffer, needed + 1, format, args);
    va_end(args);

    if (aids_array_append_many(&sb->items, (unsigned char *)buffer, needed) != AIDS_OK) {
        return_defer(AIDS_ERR);
    }

defer:
    if (buffer != NULL) {
        AIDS_FREE(buffer);
    }

    return result;
}

AIDSHDEF Aids_Result aids_string_builder_append_slice(Aids_String_Builder *sb, const Aids_String_Slice *slice) {
    return aids_array_append_many(&sb->items, slice->str, slice->len);
}

AIDSHDEF Aids_Result aids_string_builder_appendc(Aids_String_Builder *sb, char c) {
    return aids_array_append(&sb->items, (const unsigned char *)&c);
}

AIDSHDEF Aids_Result aids_string_builder_to_cstr(const Aids_String_Builder *sb, char **cstr) {
    Aids_Result result = AIDS_OK;

    // TODO: Do I want to allocate memory for the new cstr?

    *cstr = (char *)AIDS_REALLOC(NULL, sb->items.count + 1);
    if (*cstr == NULL) {
        aids__g_failure_reason = "Memory allocation failed";
        return_defer(AIDS_ERR);
    }

    memcpy(*cstr, sb->items.items, sb->items.count);
    (*cstr)[sb->items.count] = '\0';

defer:
    return result;
}

AIDSHDEF void aids_string_builder_to_slice(const Aids_String_Builder *sb, Aids_String_Slice *slice) {
    slice->str = sb->items.items;
    slice->len = sb->items.count;
}

AIDSHDEF void aids_string_builder_free(Aids_String_Builder *sb) {
    aids_array_free(&sb->items);
}

AIDSHDEF Aids_Result aids_io_read(const char *filename, Aids_String_Slice *ss, const char *mode) {
    Aids_Result result = AIDS_OK;

    unsigned long line_size;
    FILE *file = NULL;
    Aids_String_Builder sb = {0};
    aids_string_builder_init(&sb);

    if (filename != NULL) {
        file = fopen(filename, mode);
        if (file == NULL) {
            aids__g_failure_reason = aids_temp_sprintf("Failed to open file '%s' for reading", filename);
            return_defer(AIDS_ERR);
        }
    } else {
        file = stdin;
    }

    char line[LINE_MAX] = {0};
    do {
        line_size = fread(line, sizeof(char), LINE_MAX, file);
        Aids_String_Slice slice = { .str = (unsigned char *)line, .len = line_size };

        if (aids_string_builder_append_slice(&sb, &slice) != AIDS_OK) {
            aids__g_failure_reason = "Failed to append slice to string builder";
            return_defer(AIDS_ERR);
        }

        memset(line, 0, LINE_MAX); // Clear the line buffer
    } while (line_size == LINE_MAX);

    aids_string_builder_to_slice(&sb, ss);

defer:
    if (file != NULL && file != stdin) {
        fclose(file);
    }
    if (result != AIDS_OK) {
        aids_string_builder_free(&sb);
    }

    return result;
}

AIDSHDEF Aids_Result aids_io_write(const char *filename, const Aids_String_Slice *ss, const char *mode) {
    Aids_Result result = AIDS_OK;

    FILE *file = NULL;
    if (filename != NULL) {
        file = fopen(filename, mode);
        if (file == NULL) {
            aids__g_failure_reason = aids_temp_sprintf("Failed to open file '%s' for writing", filename);
            return_defer(AIDS_ERR);
        }
    } else {
        file = stdout;
    }

    unsigned long written = fwrite(ss->str, sizeof(unsigned char), ss->len, file);
    if (written != ss->len) {
        aids__g_failure_reason = "Failed to write all data to file";
        return_defer(AIDS_ERR);
    }

defer:
    if (file != NULL && file != stdout) {
        fclose(file);
    }

    return result;
}

#endif // AIDS_IMPLEMENTATION

#ifndef AIDS_STRIP_PREFIX_GUARD_
#define AIDS_STRIP_PREFIX_GUARD_
#   ifdef AIDS_STRIP_PREFIX
#       define Array Aids_Array
#       define array_init aids_array_init
#       define array_append aids_array_append
#       define array_append_many aids_array_append_many
#       define array_get aids_array_get
#       define array_free aids_array_free
#       define String_Slice Aids_String_Slice
#       define string_slice_init aids_string_slice_init
#       define string_slice_tokenize ds_string_slice_tokenize
#       define string_slice_to_cstr aids_string_slice_to_cstr
#       define string_slice_free aids_string_slice_free
#       define String_Builder Aids_String_Builder
#       define string_builder_init aids_string_builder_init
#       define string_builder_append aids_string_builder_append
#       define string_builder_append_slice aids_string_builder_append_slice
#       define string_builder_to_cstr aids_string_builder_to_cstr
#       define string_builder_to_slice aids_string_builder_to_slice
#       define string_builder_free aids_string_builder_free
#       define io_read aids_io_read
#       define io_write aids_io_write
#   endif // AIDS_STRIP_PREFIX
#endif // AIDS_STRIP_PREFIX_GUARD_
