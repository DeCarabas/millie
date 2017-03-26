#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

/*
 * Lexer
 */
typedef enum {
    TOK_EOF = 0,
    TOK_ID,
    TOK_INT_LITERAL,
    TOK_TRUE,
    TOK_FALSE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_FN,
    TOK_IF,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_EQUALS,
    TOK_ARROW,
    TOK_LET,
    TOK_REC,
} MILLIE_TOKEN;

struct MillieToken {
    MILLIE_TOKEN type;
    unsigned int start;
    unsigned int length;
};

struct MillieTokens {
    struct ArrayList *token_array; // of MillieToken
    struct ArrayList *line_array;  // of int
    struct MString *buffer;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct KeywordToken {
    const char *str;
    MILLIE_TOKEN token;
};

#pragma GCC diagnostic pop


struct MillieTokens *CreateTokens(struct MString *buffer);
void FreeTokens(struct MillieTokens **tokens_ptr);
void AddToken(struct MillieTokens *tokens, MILLIE_TOKEN type,
              unsigned int start, unsigned int length);
int IsDigit(char c);
int IsIdentifierCharacter(char c);
int IsIdentifierStart(char c);
void AddIntegerLiteral(struct MillieTokens *tokens, const char *start,
                       unsigned int length, const char **ptr_ptr);
int TryAddKeyword(struct KeywordToken keyword, struct MillieTokens *tokens,
                  const char *start, unsigned int length, const char **ptr_ptr);
int TryAddKeywords(struct KeywordToken *keywords, struct MillieTokens *tokens,
                   const char *start, unsigned int length, const char **ptr_ptr);
void AddIdentifierToken(struct MillieTokens *tokens, const char *start,
                        unsigned int length, const char **ptr_ptr);
struct MillieTokens *LexBuffer(struct MString *buffer, struct Errors **errors);
void GetLineColumnForPosition(struct MillieTokens *tokens, unsigned int position,
                              unsigned int *line, unsigned int *col);
struct MString *ExtractLine(struct MillieTokens *tokens, unsigned int line);

struct MillieTokens *
CreateTokens(struct MString *buffer)
{
    struct MillieTokens *result = calloc(1, sizeof(struct MillieTokens));
    result->token_array = CreateArrayList(sizeof(struct MillieToken), 200);
    result->line_array = CreateArrayList(sizeof(unsigned int), 100);
    result->buffer = MStringCopy(buffer);
    return result;
}

void
FreeTokens(struct MillieTokens **tokens_ptr)
{
    struct MillieTokens *tokens = *tokens_ptr;
    *tokens_ptr = NULL;

    if (!tokens) { return; }
    FreeArrayList(&tokens->token_array);
    FreeArrayList(&tokens->line_array);
    free(tokens);
}

void
AddToken(struct MillieTokens *tokens, MILLIE_TOKEN type, unsigned int start,
         unsigned int length)
{
    struct MillieToken token;
    token.type = type;
    token.start = start;
    token.length = length;

    ArrayListAdd(tokens->token_array, &token);
}

int
IsDigit(char c)
{
    return (c >= '0') && (c <= '9');
}

int
IsIdentifierCharacter(char c)
{
    return
        ((c >= '0') && (c <= '9')) ||
        ((c >= 'A') && (c <= 'Z')) ||
        ((c >= 'a') && (c <= 'z')) ||
        (c == '_') ||
        (c == '\'') ||
        (c == '\"');
}

int
IsIdentifierStart(char c)
{
    return
        ((c >= 'A') && (c <= 'Z')) ||
        ((c >= 'a') && (c <= 'z')) ||
        (c == '_');
}

void
AddIntegerLiteral(struct MillieTokens *tokens, const char *start,
                  unsigned int length, const char **ptr_ptr)
{
    const char *limit = start + length;
    const char *ptr = *ptr_ptr;
    const char *token_start = ptr;

    while (ptr < limit) {
        if (!IsDigit(*ptr)) {
            break;
        }
        ptr++;
    }

    AddToken(
        tokens,
        TOK_INT_LITERAL,
        (unsigned int)(token_start - start),
        (unsigned int)(ptr - token_start)
    );
    *ptr_ptr = ptr;
}

int
TryAddKeyword(struct KeywordToken keyword, struct MillieTokens *tokens,
              const char *start, unsigned int length, const char **ptr_ptr)
{
    const char *ptr = *ptr_ptr;
    const char *limit = start + length;
    const char *keyword_str = keyword.str;
    while(ptr < limit && *ptr == *keyword_str) {
        ptr++;
        keyword_str++;
    }

    if (*keyword_str == '\0' && !IsIdentifierCharacter(*ptr)) {
        const char *token_start = *ptr_ptr;
        unsigned int pos = (unsigned int)(token_start - start);
        unsigned int len = (unsigned int)(ptr - token_start);
        AddToken(tokens, keyword.token, pos, len);
        *ptr_ptr = ptr;
        return 1;
    }

    return 0;
}

int
TryAddKeywords(struct KeywordToken *keywords, struct MillieTokens *tokens,
               const char *start, unsigned int length, const char **ptr_ptr)
{
    while(keywords->str) {
        if (TryAddKeyword(*keywords, tokens, start, length, ptr_ptr)) {
            return 1;
        }
        keywords++;
    }

    return 0;
}

void
AddIdentifierToken(struct MillieTokens *tokens, const char *start,
                   unsigned int length, const char **ptr_ptr)
{
    const char *limit = start + length;
    const char *ptr = *ptr_ptr;
    const char *token_start = ptr;

    while (ptr < limit) {
        if (!IsIdentifierCharacter(*ptr)) {
            break;
        }
        ptr++;
    }

    AddToken(
        tokens,
        TOK_ID,
        (unsigned int)(token_start - start),
        (unsigned int)(ptr - token_start)
    );
    *ptr_ptr = ptr;
}

struct MillieTokens *
LexBuffer(struct MString *buffer, struct Errors **errors)
{
    struct MillieTokens *tokens = CreateTokens(buffer);
    *errors = NULL;

    const unsigned int length = MStringLength(buffer);
    const char *start = MStringData(buffer);
    const char *ptr = start;
    unsigned int error_start = UINT_MAX;

    while(ptr - start < length) {
        const char *token_start = ptr;
        unsigned int pos = (unsigned int)(token_start - start);

        int error_now = 0;
        switch(*ptr) {
        case '(': AddToken(tokens, TOK_LPAREN, pos, 1); ptr++; break;
        case ')': AddToken(tokens, TOK_RPAREN, pos, 1); ptr++; break;
        case '+': AddToken(tokens, TOK_PLUS, pos, 1); ptr++; break;
        case '-': AddToken(tokens, TOK_MINUS, pos, 1); ptr++; break;
        case '*': AddToken(tokens, TOK_STAR, pos, 1); ptr++; break;
        case '=':
            {
                ptr++;
                if (ptr - start < length && *ptr == '>') {
                    AddToken(tokens, TOK_ARROW, pos, 2);
                    ptr++;
                } else {
                    AddToken(tokens, TOK_EQUALS, pos, 1);
                }
            }
            break;

        case ' ':
        case '\t':
        case '\r':
            ptr++;
            break;

        case '\n':
            ArrayListAdd(tokens->line_array, &pos);
            ptr++;
            break;

        case 'f':
            {
                struct KeywordToken kws[] = {
                    { "false", TOK_FALSE },
                    { "fn", TOK_FN },
                    { NULL, 0 },
                };
                if (!TryAddKeywords(kws, tokens, start, length, &ptr)) {
                    AddIdentifierToken(tokens, start, length, &ptr);
                }
            }
            break;

        case 'i':
            {
                struct KeywordToken kws[] = {
                    { "if", TOK_IF },
                    { NULL, 0 },
                };
                if (!TryAddKeywords(kws, tokens, start, length, &ptr)) {
                    AddIdentifierToken(tokens, start, length, &ptr);
                }
            }
            break;

        case 'l':
            {
                struct KeywordToken kws[] = {
                    { "let", TOK_LET },
                    { NULL, 0 },
                };
                if (!TryAddKeywords(kws, tokens, start, length, &ptr)) {
                    AddIdentifierToken(tokens, start, length, &ptr);
                }
            }
            break;

        case 'r':
            {
                struct KeywordToken kws[] = {
                    { "rec", TOK_REC },
                    { NULL, 0 },
                };
                if (!TryAddKeywords(kws, tokens, start, length, &ptr)) {
                    AddIdentifierToken(tokens, start, length, &ptr);
                }
            }
            break;

        case 't':
            {
                struct KeywordToken kws[] = {
                    { "true", TOK_TRUE },
                    { NULL, 0 },
                };
                if (!TryAddKeywords(kws, tokens, start, length, &ptr)) {
                    AddIdentifierToken(tokens, start, length, &ptr);
                }
            }
            break;

        default:
            if (IsDigit(*ptr)) {
                AddIntegerLiteral(tokens, start, length, &ptr);
            } else if (IsIdentifierStart(*ptr)) {
                AddIdentifierToken(tokens, start, length, &ptr);
            } else {
                error_now = 1;
                ptr++;
            }
        }

        // Error state machine; collapse all errors until we scan
        // *something*.
        if (error_now) {
            if (UINT_MAX == error_start) {
                error_start = pos;
            }
        } else if (error_start != UINT_MAX) {
            // We were in an error state, now we aren't; report the
            // scanning error.
            struct MString *msg = MStringCreate("Unexpected characters");
            AddError(errors, error_start, pos, msg);
            MStringFree(&msg);

            error_start = UINT_MAX;
        }
    }

    return tokens;
}

void
GetLineColumnForPosition(struct MillieTokens *tokens, unsigned int position,
                         unsigned int *line, unsigned int *col)
{
    unsigned int *line_array = (unsigned int *)(tokens->line_array->buffer);
    int min = 0;
    int max = ((int)tokens->line_array->item_count) - 1;
    while(min <= max) {
        int mid = (max + min) / 2;
        unsigned int line_position = line_array[mid];
        if (line_position == position) {
            min = mid;
            break;
        } else if (line_position < position) {
            min = mid + 1;
        } else {
            max = mid - 1;
        }
    }

    *line = (unsigned int)(min + 1);
    if (min > 0) {
        *col = position - line_array[min - 1];
    } else {
        *col = position + 1;
    }
}

struct MString *
ExtractLine(struct MillieTokens *tokens, unsigned int line)
{
    if (line == 0) {
        Fail("Expect line > 0");
    }

    struct ArrayList *line_array = tokens->line_array;
    if (line_array->item_count == 0) {
        return MStringCopy(tokens->buffer);
    }

    unsigned int *line_ends = (unsigned int *)(line_array->buffer);

    unsigned int line_index = line - 1;
    unsigned int line_start = 0;
    if (line_index > 0) {
        line_start = line_ends[line_index - 1] + 1;
    }
    unsigned int line_end = line_ends[line_index];

    return MStringCreateN(
        MStringData(tokens->buffer) + line_start,
        line_end - line_start
    );
}
