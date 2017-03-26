#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

struct MString {
    int references;
    unsigned int length;
    const char *str;
};

struct MString *
_AllocString(unsigned int length)
{
    int string_length = length + 1;
    size_t alloc_size = sizeof(struct MString) + string_length;
    struct MString *result = calloc(1, alloc_size);

    result->references = 1;
    result->length = length;
    result->str = (char *)(result + 1);

    return result;
}

struct MString *
CreateStringN(const char *str, unsigned int length)
{
    struct MString *result = _AllocString(length);

    memcpy((char *)(result->str), str, length);

    return result;
}

struct MString *
CreateString(char *str)
{
    return CreateStringN(str, strlen(str));
}

void
CreateStringReferenceN(char *str, unsigned int length, struct MString *string)
{
    memset(string, 0, sizeof(struct MString));
    string->references = -1;
    string->length = length;
    string->str = str;
}

void
CreateStringReference(char *str, struct MString *string)
{
    CreateStringReferenceN(str, strlen(str), string);
}

void
FreeString(struct MString **string_ptr)
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

struct MString *
CopyString(struct MString *string)
{
    if (!string) { return NULL; }
    if (string->references == -1) {
        return CreateStringN(string->str, string->length);
    }
    string->references += 1;
    return string;
}

struct MString *
StringCatV(int count, ...)
{
    va_list ap;
    unsigned int total_length = 0;

    va_start (ap, count);
    for(int i = 0; i < count; i++) {
        struct MString *arg = va_arg(ap, struct MString *);
        total_length += arg->length;
    }
    va_end(ap);

    struct MString *result = _AllocString(total_length);
    char *ptr = (char *)(result->str);

    va_start(ap, count);
    for(int i = 0; i < count; i++) {
        struct MString *arg = va_arg(ap, struct MString *);
        memcpy(ptr, arg->str, arg->length);
        ptr += arg->length;
    }
    va_end(ap);

    return result;
}

struct MString *
StringCat(struct MString *first, struct MString *second)
{
    return StringCatV(2, first, second);
}

struct MString *
StringPrintFV(const char *format, va_list args)
{
    va_list ap;
    va_copy(ap, args);
    int total_size = vsnprintf(NULL, 0, format, ap);
    va_end(ap);

    struct MString *result = _AllocString(total_size);

    va_copy(ap, args);
    vsnprintf((char *)result->str, total_size, format, ap);
    va_end(ap);

    return result;
}

struct MString *
StringPrintF(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    struct MString *result = StringPrintFV(format, ap);
    va_end(ap);

    return result;
}

unsigned int
StringLength(struct MString *string)
{
    return string->length;
}

const char *
StringData(struct MString *string)
{
    return string->str;
}
