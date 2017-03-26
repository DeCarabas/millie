#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

struct Errors {
    struct ErrorReport *first;
    struct ErrorReport *last;
};

void AddError(struct Errors **errors_ptr, unsigned int start_pos,
              unsigned int end_pos, struct MString *message)
{
    struct Errors *errors = *errors_ptr;
    if (!errors) {
        errors = *errors_ptr = calloc(1, sizeof(struct Errors));
    }

    struct ErrorReport *report = calloc(1, sizeof(struct ErrorReport));
    report->message = MStringCopy(message);
    report->start_pos = start_pos;
    report->end_pos = end_pos;
    if (errors->last) {
        errors->last->next = report;
        errors->last = report;
    } else {
        errors->first = errors->last = report;
    }
}

void AddErrorF(struct Errors **errors_ptr, unsigned int start_pos,
               unsigned int end_pos, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    struct MString *message = MStringPrintFV(format, ap);
    va_end(ap);

    AddError(errors_ptr, start_pos, end_pos, message);
    MStringFree(&message);
}

void FreeErrors(struct Errors **errors_ptr)
{
    struct Errors *errors = *errors_ptr;
    *errors_ptr = NULL;

    if (errors) {
        struct ErrorReport *current = errors->first;
        while(current) {
            struct ErrorReport *next = current->next;
            MStringFree(&current->message);
            free(current);
            current = next;
        }
    }
}

struct ErrorReport *FirstError(struct Errors *errors)
{
    if (!errors) { return NULL; }
    return errors->first;
}
