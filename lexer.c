#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

struct KeywordToken {
    const char *str;
    MILLIE_TOKEN token;
};

#pragma GCC diagnostic pop

static struct MillieTokens *_CreateTokens(struct MString *buffer)
{
    struct MillieTokens *result = calloc(1, sizeof(struct MillieTokens));
    result->token_array = ArrayListCreate(sizeof(struct MillieToken), 200);
    result->line_array = ArrayListCreate(sizeof(unsigned int), 100);
    result->buffer = MStringCopy(buffer);
    return result;
}

static void _AddToken(struct MillieTokens *tokens, MILLIE_TOKEN type,
                      unsigned int start, unsigned int length)
{
    struct MillieToken token;
    token.type = type;
    token.start = start;
    token.length = length;

    ArrayListAdd(tokens->token_array, &token);
}

static int _IsDigit(char c)
{
    return (c >= '0') && (c <= '9');
}

static int _IsIdentifierCharacter(char c)
{
    return
        ((c >= '0') && (c <= '9')) ||
        ((c >= 'A') && (c <= 'Z')) ||
        ((c >= 'a') && (c <= 'z')) ||
        (c == '_') ||
        (c == '\'') ||
        (c == '\"');
}

static int _IsIdentifierStart(char c)
{
    return
        ((c >= 'A') && (c <= 'Z')) ||
        ((c >= 'a') && (c <= 'z')) ||
        (c == '_');
}

static void _AddIntegerLiteral(struct MillieTokens *tokens, const char *start,
                               unsigned int length, const char **ptr_ptr)
{
    const char *limit = start + length;
    const char *ptr = *ptr_ptr;
    const char *token_start = ptr;

    while (ptr < limit) {
        if (!_IsDigit(*ptr)) {
            break;
        }
        ptr++;
    }

    _AddToken(
        tokens,
        TOK_INT_LITERAL,
        (unsigned int)(token_start - start),
        (unsigned int)(ptr - token_start)
    );
    *ptr_ptr = ptr;
}

static int _TryAddKeyword(struct KeywordToken keyword,
                          struct MillieTokens *tokens,
                          const char *start,
                          unsigned int length,
                          const char **ptr_ptr)
{
    const char *ptr = *ptr_ptr;
    const char *limit = start + length;
    const char *keyword_str = keyword.str;
    while(ptr < limit && *ptr == *keyword_str) {
        ptr++;
        keyword_str++;
    }

    if (*keyword_str == '\0' && !_IsIdentifierCharacter(*ptr)) {
        const char *token_start = *ptr_ptr;
        unsigned int pos = (unsigned int)(token_start - start);
        unsigned int len = (unsigned int)(ptr - token_start);
        _AddToken(tokens, keyword.token, pos, len);
        *ptr_ptr = ptr;
        return 1;
    }

    return 0;
}

// TODO: Parse as identifier and then classify keywords; use length &c.
static int _TryAddKeywords(struct KeywordToken *keywords,
                           struct MillieTokens *tokens,
                           const char *start,
                           unsigned int length,
                           const char **ptr_ptr)
{
    while(keywords->str) {
        if (_TryAddKeyword(*keywords, tokens, start, length, ptr_ptr)) {
            return 1;
        }
        keywords++;
    }

    return 0;
}

static void _AddIdentifierToken(struct MillieTokens *tokens, const char *start,
                                unsigned int length, const char **ptr_ptr)
{
    const char *limit = start + length;
    const char *ptr = *ptr_ptr;
    const char *token_start = ptr;

    while (ptr < limit) {
        if (!_IsIdentifierCharacter(*ptr)) {
            break;
        }
        ptr++;
    }

    _AddToken(
        tokens,
        TOK_ID,
        (unsigned int)(token_start - start),
        (unsigned int)(ptr - token_start)
    );
    *ptr_ptr = ptr;
}

void TokensFree(struct MillieTokens **tokens_ptr)
{
    struct MillieTokens *tokens = *tokens_ptr;
    *tokens_ptr = NULL;

    if (!tokens) { return; }
    ArrayListFree(&tokens->token_array);
    ArrayListFree(&tokens->line_array);
    free(tokens);
}

struct MillieTokens *LexBuffer(struct MString *buffer, struct Errors **errors)
{
    struct MillieTokens *tokens = _CreateTokens(buffer);
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
        case '(': _AddToken(tokens, TOK_LPAREN, pos, 1); ptr++; break;
        case ')': _AddToken(tokens, TOK_RPAREN, pos, 1); ptr++; break;
        case '+': _AddToken(tokens, TOK_PLUS, pos, 1); ptr++; break;
        case '-': _AddToken(tokens, TOK_MINUS, pos, 1); ptr++; break;
        case '*': _AddToken(tokens, TOK_STAR, pos, 1); ptr++; break;
        case '/': _AddToken(tokens, TOK_SLASH, pos, 1); ptr++; break;
        case '=':
            {
                ptr++;
                if (ptr - start < length && *ptr == '>') {
                    _AddToken(tokens, TOK_ARROW, pos, 2);
                    ptr++;
                } else {
                    _AddToken(tokens, TOK_EQUALS, pos, 1);
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

        case 'e':
            {
                struct KeywordToken kws[] = {
                    { "else", TOK_ELSE },
                    { NULL, 0 },
                };
                if (!_TryAddKeywords(kws, tokens, start, length, &ptr)) {
                    _AddIdentifierToken(tokens, start, length, &ptr);
                }
            }
            break;

        case 'f':
            {
                struct KeywordToken kws[] = {
                    { "false", TOK_FALSE },
                    { "fn", TOK_FN },
                    { NULL, 0 },
                };
                if (!_TryAddKeywords(kws, tokens, start, length, &ptr)) {
                    _AddIdentifierToken(tokens, start, length, &ptr);
                }
            }
            break;

        case 'i':
            {
                struct KeywordToken kws[] = {
                    { "if", TOK_IF },
                    { "in", TOK_IN },
                    { NULL, 0 },
                };
                if (!_TryAddKeywords(kws, tokens, start, length, &ptr)) {
                    _AddIdentifierToken(tokens, start, length, &ptr);
                }
            }
            break;

        case 'l':
            {
                struct KeywordToken kws[] = {
                    { "let", TOK_LET },
                    { NULL, 0 },
                };
                if (!_TryAddKeywords(kws, tokens, start, length, &ptr)) {
                    _AddIdentifierToken(tokens, start, length, &ptr);
                }
            }
            break;

        case 'r':
            {
                struct KeywordToken kws[] = {
                    { "rec", TOK_REC },
                    { NULL, 0 },
                };
                if (!_TryAddKeywords(kws, tokens, start, length, &ptr)) {
                    _AddIdentifierToken(tokens, start, length, &ptr);
                }
            }
            break;

        case 't':
            {
                struct KeywordToken kws[] = {
                    { "then", TOK_THEN },
                    { "true", TOK_TRUE },
                    { NULL, 0 },
                };
                if (!_TryAddKeywords(kws, tokens, start, length, &ptr)) {
                    _AddIdentifierToken(tokens, start, length, &ptr);
                }
            }
            break;

        default:
            if (_IsDigit(*ptr)) {
                _AddIntegerLiteral(tokens, start, length, &ptr);
            } else if (_IsIdentifierStart(*ptr)) {
                _AddIdentifierToken(tokens, start, length, &ptr);
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

    _AddToken(tokens, TOK_EOF, length, 0);
    return tokens;
}

void GetLineColumnForPosition(struct MillieTokens *tokens,
                              unsigned int position,
                              unsigned int *line,
                              unsigned int *col)
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

struct MString *ExtractLine(struct MillieTokens *tokens, unsigned int line)
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
