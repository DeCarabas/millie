#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

struct MString {
    int references;
    unsigned int length;
    const char *str;
};

struct MString *_MStringAlloc(unsigned int length, char **buffer);

struct MString *_MStringAlloc(unsigned int length, char **buffer)
{
    unsigned int string_length = length + 1; // ALWAYS NULL TERMINATE
    unsigned int alloc_size = sizeof(struct MString) + string_length;
    struct MString *result = calloc((size_t)alloc_size, 1);

    result->references = 1;
    result->length = length;
    result->str = *buffer = (char *)(result + 1);

    return result;
}

struct MString *MStringCreateN(const char *str, unsigned int length)
{
    char *buffer;
    struct MString *result = _MStringAlloc(length, &buffer);

    memcpy(buffer, str, length);

    return result;
}

struct MString *MStringCreate(const char *str)
{
    return MStringCreateN(str, (unsigned int)strlen(str));
}

struct MString *MStringCreateStatic(const char *str, struct MStringStatic *blk)
{
    struct MString *result = (struct MString *)blk;
    result->references = -1;
    result->length = strlen(str);
    result->str = str;
    return result;
}

void MStringFree(struct MString **string_ptr)
{
    struct MString *string = *string_ptr;
    *string_ptr = NULL;

    if (!string) { return; }
    if (string->references == -1) { return; }

    string->references -= 1;
    if (string->references == 0) {
        free(string);
    }
}

struct MString *MStringCopy(struct MString *string)
{
    if (!string) { return NULL; }
    if (string->references == -1) {
        return MStringCreateN(string->str, string->length);
    }
    string->references += 1;
    return string;
}

struct MString *MStringCatV(int count, ...)
{
    va_list ap;
    unsigned int total_length = 0;

    va_start (ap, count);
    for(int i = 0; i < count; i++) {
        struct MString *arg = va_arg(ap, struct MString *);
        total_length += arg->length;
    }
    va_end(ap);

    char *ptr;
    struct MString *result = _MStringAlloc(total_length, &ptr);

    va_start(ap, count);
    for(int i = 0; i < count; i++) {
        struct MString *arg = va_arg(ap, struct MString *);
        memcpy(ptr, arg->str, arg->length);
        ptr += arg->length;
    }
    va_end(ap);

    return result;
}

struct MString *MStringCat(struct MString *first, struct MString *second)
{
    return MStringCatV(2, first, second);
}

struct MString *MStringPrintFV(const char *format, va_list args)
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat-nonliteral"
    va_list ap;
    va_copy(ap, args);
    int total_size = vsnprintf(NULL, 0, format, ap);
    va_end(ap);

    char *ptr;
    struct MString *result = _MStringAlloc((unsigned int)total_size, &ptr);

    va_copy(ap, args);
    vsnprintf(ptr, total_size+1, format, ap);
    va_end(ap);

    return result;
    #pragma GCC diagnostic pop
}

struct MString *MStringPrintF(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    struct MString *result = MStringPrintFV(format, ap);
    va_end(ap);

    return result;
}

unsigned int MStringLength(struct MString *string)
{
    return string->length;
}

const char *MStringData(struct MString *string)
{
    return string->str;
}

unsigned int MStringHash32(struct MString *string)
{
    return CityHash32(string->str, string->length);
}

bool MStringEquals(struct MString *a, struct MString *b)
{
    if (a->length != b->length) { return false; }
    return memcmp(a->str, b->str, a->length) == 0;
}
