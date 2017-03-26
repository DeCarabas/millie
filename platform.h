#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Assertion and failure
 */
void Fail(char *message);

/*
 * String functions
 */
struct MString;

struct MString *CreateString(char *str);
struct MString *CreateStringN(const char *str, unsigned int length);
void FreeString(struct MString **string_ptr);
struct MString *CopyString(struct MString *string);
struct MString *StringCatV(int count, ...);
struct MString *StringCat(struct MString *first, struct MString *second);
struct MString *StringPrintF(const char *format, ...);
struct MString *StringPrintFV(const char *format, va_list args);
unsigned int StringLength(struct MString *string);
const char *StringData(struct MString *string);

/*
 * List functions
 */
struct ArrayList {
    unsigned int item_size;
    unsigned int item_count;
    unsigned int capacity;
    void *buffer;
};

struct ArrayList *CreateArrayList(unsigned int item_size,
                                  unsigned int capacity);
void FreeArrayList(struct ArrayList **array_ptr);
void *ArrayListIndex(struct ArrayList *array, unsigned int index);
int ArrayListAdd(struct ArrayList *array, void *item);


/*
 * Error functions
 */
struct ErrorReport {
    struct ErrorReport *next;
    struct MString *message;
    int start_pos;
    int end_pos;
};

struct Errors;

void AddError(struct Errors **errors_ptr, int start_pos, int end_pos,
              struct MString *message);
void AddErrorF(struct Errors **errors_ptr, int start_pos, int end_pos,
               const char *format, ...);
void FreeErrors(struct Errors **errors_ptr);
struct ErrorReport *FirstError(struct Errors *errors);


#define PLATFORM_INCLUDED
