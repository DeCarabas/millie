#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NORETURN  __attribute__((noreturn))

/*
 * Assertion and failure
 */
void Fail(char *message);

/*
 * Memory allocation
 */

/*
 * Hash functions
 */
uint32_t CityHash32(const char *s, size_t len);

/*
 * String functions
 */
struct MString;

struct MString *MStringCreate(char *str);
struct MString *MStringCreateN(const char *str, unsigned int length);
void MStringFree(struct MString **string_ptr);
struct MString *MStringCopy(struct MString *string);
struct MString *MStringCatV(int count, ...);
struct MString *MStringCat(struct MString *first, struct MString *second);
struct MString *MStringPrintF(const char *format, ...);
struct MString *MStringPrintFV(const char *format, va_list args);
unsigned int MStringLength(struct MString *string);
const char *MStringData(struct MString *string);
unsigned int MStringHash32(struct MString *string);
bool MStringEquals(struct MString *a, struct MString *b);

/*
 * List functions
 */
struct ArrayList {
    unsigned int item_count;
    unsigned int capacity;
    size_t item_size;
    void *buffer;
};

struct ArrayList *CreateArrayList(size_t item_size,
                                  unsigned int capacity);
void FreeArrayList(struct ArrayList **array_ptr);
void *ArrayListIndex(struct ArrayList *array, unsigned int index);
unsigned int ArrayListAdd(struct ArrayList *array, void *item);


/*
 * Error functions
 */
struct ErrorReport {
    struct ErrorReport *next;
    struct MString *message;
    unsigned int start_pos;
    unsigned int end_pos;
};

struct Errors;

void AddError(struct Errors **errors_ptr, unsigned int start_pos,
              unsigned int end_pos, struct MString *message);
void AddErrorF(struct Errors **errors_ptr, unsigned int start_pos,
               unsigned int end_pos, const char *format, ...);
void FreeErrors(struct Errors **errors_ptr);
struct ErrorReport *FirstError(struct Errors *errors);

/*
 * Symbol Tables
 */
typedef uint32_t Symbol;
#define INVALID_SYMBOL (0)
struct SymbolTable;

struct SymbolTable *SymbolTableCreate(void);
void SymbolTableFree(struct SymbolTable **table_ptr);
Symbol FindOrCreateSymbol(struct SymbolTable *table, struct MString *key);


#define PLATFORM_INCLUDED
