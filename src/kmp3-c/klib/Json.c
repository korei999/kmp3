#include "Json.h"

#include "Ctx.h"

#include <ctype.h>

#define TOKEN_EOF 0
#define TOKEN_STRING 1
#define TOKEN_QSTRING 2
#define TOKEN_BRACE_OPEN 3
#define TOKEN_BRACE_CLOSE 4
#define TOKEN_BRACKET_OPEN 5
#define TOKEN_BRACKET_CLOSE 6
#define TOKEN_NUMBER 7
#define TOKEN_COMMA 8
#define TOKEN_COLON 9

static const char* aTOKEN_STRINGS[] = {
    "TOKEN_EOF",
    "TOKEN_STRING",
    "TOKEN_QSTRING",
    "TOKEN_BRACE_OPEN",
    "TOKEN_BRACE_CLOSE",
    "TOKEN_BRACKET_OPEN",
    "TOKEN_BRACKET_CLOSE",
    "TOKEN_NUMBER",
    "TOKEN_COMMA",
    "TOKEN_COLON",
};

static void expectErrorLog(k_JsonParser* s, const int* pTokens, int nTokens, bool bNot);
static bool parseObject(k_JsonParser* s, k_IAllocator* pAlloc, k_JsonObject* pObj);

static void
skipWhiteSpace(k_JsonParser* s)
{
    while (s->i < s->svText.size &&
        (s->svText.pData[s->i] == ' ' ||
        s->svText.pData[s->i] == '\n' ||
        s->svText.pData[s->i] == '\r' ||
        s->svText.pData[s->i] == '\t')
    )
    {
        if (s->svText.pData[s->i] == '\n')
        {
            s->x = 0;
            ++s->y;
        }

        ++s->i;
        ++s->x;
    }
}

static bool
tokQuotedString(k_JsonParser* s)
{
    const ssize_t pos = s->i++;
    s->tok.x = s->x++;
    s->tok.y = s->y;

    bool bEscape = false;
    while (s->i < s->svText.size)
    {
        if (s->svText.pData[s->i] == '"' && !bEscape)
        {
            ++s->i;
            ++s->x;
            s->tok.eType = TOKEN_QSTRING;
            s->tok.sv = (k_StringView){s->svText.pData + pos, s->i - pos};
            return true;
        }

        if (s->svText.pData[s->i] == '\\')
            bEscape = true;
        else bEscape = false;

        if (s->svText.pData[s->i] == '\n')
        {
            s->x = 0;
            ++s->y;
        }

        ++s->i;
        ++s->x;
    }

    K_CTX_LOG_ERROR("unterminated string (at: {i}, {i})", s->tok.x, s->tok.y);
    return false;
}

static void
tokString(k_JsonParser* s)
{
    const ssize_t pos = s->i++;
    s->tok.x = s->x++;
    s->tok.y = s->y;

    while (s->i < s->svText.size &&
        k_noneOfChars(s->svText.pData[s->i], K_SV(" \n\r\t,}]"))
    )
    {
        ++s->i;
        ++s->x;
    }

    s->tok.eType = TOKEN_STRING;
    s->tok.sv = (k_StringView){s->svText.pData + pos, s->i - pos};
}

static void
tokNumber(k_JsonParser* s)
{
    const ssize_t pos = s->i;
    s->tok.x = s->x;
    s->tok.y = s->y;

    while (s->i < s->svText.size)
    {
        if (!isxdigit(s->svText.pData[s->i]) &&
            s->svText.pData[s->i] != '.' &&
            s->svText.pData[s->i] != '-' &&
            s->svText.pData[s->i] != '+'
        )
        {
            s->tok.eType = TOKEN_NUMBER;
            s->tok.sv = (k_StringView){s->svText.pData + pos, s->i - pos};
            return;
        }
        ++s->i;
        ++s->x;
    }
}

static void
tokChar(k_JsonParser* s)
{
    s->tok.x = s->x;
    s->tok.y = s->y;
    s->tok.sv = (k_StringView){s->svText.pData + s->i, 1};

    ++s->i;
    ++s->x;
}

static bool
nextToken(k_JsonParser* s)
{
    s->tok = (k_JsonToken){0};

    if (s->i < s->svText.size)
    {
        skipWhiteSpace(s);
        if (s->i >= s->svText.size)
        {
            s->tok.x = s->x;
            s->tok.y = s->y;
            return true; /* EOF */
        }

        switch (s->svText.pData[s->i])
        {
            default:
            tokString(s);
            break;

            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': case '.':
            tokNumber(s);
            break;

            case '"':
            if (!tokQuotedString(s))
                return false;
            break;

            case ',':
            s->tok.eType = TOKEN_COMMA;
            tokChar(s);
            break;

            case ':':
            s->tok.eType = TOKEN_COLON;
            tokChar(s);
            break;

            case '[':
            s->tok.eType = TOKEN_BRACKET_OPEN;
            tokChar(s);
            break;

            case ']':
            s->tok.eType = TOKEN_BRACKET_CLOSE;
            tokChar(s);
            break;

            case '{':
            s->tok.eType = TOKEN_BRACE_OPEN;
            tokChar(s);
            break;

            case '}':
            s->tok.eType = TOKEN_BRACE_CLOSE;
            tokChar(s);
            break;
        }
    }

    return true;
}

static void
expectErrorLog(k_JsonParser* s, const int* pTokens, int nTokens, bool bNot)
{
    k_Arena* pArena = k_CtxArena();
    K_ARENA_SCOPE(pArena)
    {
        k_print_Builder b;
        k_print_BuilderInitOpts opts = {.pAllocOrNull = &pArena->base, .preallocOrBufferSize = 256};
        if (k_print_BuilderInit(&b, opts))
        {
            const k_StringView svExpected = bNot ? K_SV("expected not: [") : K_SV("expected: [");
            k_print_BuilderPushSv(&b, svExpected);
            for (int i = 0; i < nTokens; ++i)
            {
                k_print_BuilderPushSv(&b, K_NTS(aTOKEN_STRINGS[pTokens[i]]));
                if (i != nTokens - 1) k_print_BuilderPushSv(&b, K_SV(" or "));
            }
            k_print_BuilderPrint(&b, "], got: {s} '{PSv}' (at: {i}, {i})", aTOKEN_STRINGS[s->tok.eType], &s->tok.sv, s->tok.x, s->tok.y);
        }

        const k_StringView svPrinted = k_print_BuilderToSv(&b);
        K_CTX_LOG_ERROR("{PSv}", &svPrinted);
    }
}

static bool
expect(k_JsonParser* s, const int* pTokens, int nTokens)
{
    for (int i = 0; i < nTokens; ++i)
    {
        if (pTokens[i] != s->tok.eType)
        {
            expectErrorLog(s, pTokens, nTokens, false);
            return false;
        }
    }

    return true;
}

K_UNUSED static bool
expectNot(k_JsonParser* s, const int* pTokens, int nTokens)
{
    for (int i = 0; i < nTokens; ++i)
    {
        if (pTokens[i] == s->tok.eType)
        {
            expectErrorLog(s, pTokens, nTokens, true);
            return false;
        }
    }

    return true;
}

K_NO_DISCARD static bool
setStringType(k_JsonValue* pVal)
{
    if (pVal->svValue.size > 0)
    {
        if (k_StringViewEq(pVal->svValue, K_SV("false")))
            pVal->eType = K_JSON_TYPE_BOOL;
        else if (k_StringViewEq(pVal->svValue, K_SV("true")))
            pVal->eType = K_JSON_TYPE_BOOL;
        else if (k_StringViewEq(pVal->svValue, K_SV("null")))
            pVal->eType = K_JSON_TYPE_NULL;
    }

    if (pVal->eType == 0)
    {
        K_CTX_LOG_ERROR("failed to set type for '{PSv}'", &pVal->svValue);
        return false;
    }

    return true;
}

K_NO_DISCARD static bool
setNumberType(k_JsonValue* pVal)
{
    if (pVal->svValue.size > 0)
    {
        if (isxdigit(pVal->svValue.pData[0]) ||
            pVal->svValue.pData[0] == '+' ||
            pVal->svValue.pData[0] == '-' ||
            pVal->svValue.pData[0] == '.'
        )
        {
            if (k_StringViewHasOneOf(pVal->svValue, K_SV(".eE")))
                pVal->eType = K_JSON_TYPE_FLOAT;
            else pVal->eType = K_JSON_TYPE_INT;
        }
    }

    if (pVal->eType == 0)
    {
        K_CTX_LOG_ERROR("failed to set type for '{PSv}'", &pVal->svValue);
        return false;
    }

    return true;
}

static bool
parseArray(k_JsonParser* s, k_IAllocator* pAlloc, k_JsonArray* pArr)
{
gotComma:
    if (!nextToken(s)) return false;

    k_JsonValue val = {0};

    switch (s->tok.eType)
    {
        case TOKEN_NUMBER:
        {
            val.svValue = s->tok.sv;
            if (!setNumberType(&val)) return false;
            const ssize_t n = k_VecPush(&pArr->vValues, pAlloc, sizeof(val), &val);
            if (n < 0) return false;
        }
        break;

        case TOKEN_QSTRING:
        {
            val.eType = K_JSON_TYPE_STRING;
            val.svValue = k_StringViewSubString(s->tok.sv, 1, s->tok.sv.size - 2);
            if (k_VecPush(&pArr->vValues, pAlloc, sizeof(val), &val) < 0)
                return false;
        }
        break;

        case TOKEN_STRING:
        {
            val.eType = K_JSON_TYPE_STRING;
            val.svValue = s->tok.sv;
            if (!setStringType(&val)) return false;
            if (k_VecPush(&pArr->vValues, pAlloc, sizeof(val), &val) < 0)
                return false;
        }
        break;

        case TOKEN_BRACE_OPEN:
        {
            val.eType = K_JSON_TYPE_OBJECT;
            const ssize_t n = k_VecPush(&pArr->vValues, pAlloc, sizeof(val), &val);
            if (n < 0) return false;

            k_JsonValue* pNewVal = (k_JsonValue*)pArr->vValues.pData + n;
            if (!parseObject(s, pAlloc, &pNewVal->object))
                return false;
        }
        break;

        case TOKEN_BRACKET_CLOSE:
        return true;

        default:
        {
            const int aTokens[] = {
                TOKEN_NUMBER,
                TOKEN_QSTRING,
                TOKEN_STRING,
                TOKEN_BRACE_OPEN,
                TOKEN_BRACKET_CLOSE,
            };
            expectErrorLog(s, aTokens, K_ASIZE(aTokens), false);
            return false;
        }
    }

    if (!nextToken(s)) return false;

    switch (s->tok.eType)
    {
        case TOKEN_COMMA:
        goto gotComma;

        case TOKEN_BRACKET_CLOSE:
        break;

        default:
        {
            const int aTokens[] = {TOKEN_COMMA, TOKEN_BRACKET_CLOSE};
            expectErrorLog(s, aTokens, K_ASIZE(aTokens), false);
        }
        return false;
    }

    return true;
}

static bool
parseObject(k_JsonParser* s, k_IAllocator* pAlloc, k_JsonObject* pObj)
{
gotComma:
    if (!nextToken(s)) return false;

    const k_StringView svObjName = k_StringViewSubString(s->tok.sv, 1, s->tok.sv.size - 2);

    switch (s->tok.eType)
    {
        case TOKEN_QSTRING:
        {
            if (!nextToken(s)) return false;
            if (!expect(s, &(int){TOKEN_COLON}, 1)) return false;
            if (!nextToken(s)) return false;

            k_JsonNameValue nameVal = {0};
            nameVal.svName = svObjName;

            switch (s->tok.eType)
            {
                case TOKEN_QSTRING:
                {
                    nameVal.val.svValue = k_StringViewSubString(s->tok.sv, 1, s->tok.sv.size - 2);
                    nameVal.val.eType = K_JSON_TYPE_STRING;
                    if (k_VecPush(&pObj->vNameValues, pAlloc, sizeof(nameVal), &nameVal) < 0)
                        return false;
                }
                break;

                case TOKEN_NUMBER:
                {
                    nameVal.val.svValue = s->tok.sv;
                    if (!setNumberType(&nameVal.val)) return false;
                    if (k_VecPush(&pObj->vNameValues, pAlloc, sizeof(nameVal), &nameVal) < 0)
                        return false;
                }
                break;

                case TOKEN_STRING:
                {
                    nameVal.val.svValue = s->tok.sv;
                    if (!setStringType(&nameVal.val)) return false;
                    if (k_VecPush(&pObj->vNameValues, pAlloc, sizeof(nameVal), &nameVal) < 0)
                        return false;
                }
                break;

                case TOKEN_BRACE_OPEN:
                {
                    nameVal.val.eType = K_JSON_TYPE_OBJECT;
                    ssize_t n = k_VecPush(&pObj->vNameValues, pAlloc, sizeof(nameVal), &nameVal);
                    if (n < 0) return false;

                    k_JsonNameValue* pNewNameVal = (k_JsonNameValue*)pObj->vNameValues.pData + n;
                    if (!parseObject(s, pAlloc, &pNewNameVal->val.object))
                        return false;
                }
                break;

                case TOKEN_BRACKET_OPEN:
                {
                    nameVal.val.eType = K_JSON_TYPE_ARRAY;
                    ssize_t n = k_VecPush(&pObj->vNameValues, pAlloc, sizeof(nameVal), &nameVal);
                    if (n < 0) return false;

                    k_JsonNameValue* pNewNameVal = (k_JsonNameValue*)pObj->vNameValues.pData + n;
                    if (!parseArray(s, pAlloc, &pNewNameVal->val.array))
                        return false;
                }
                break;

                default:
                {
                    const int aTokens[] = {
                        TOKEN_QSTRING,
                        TOKEN_NUMBER,
                        TOKEN_STRING,
                        TOKEN_BRACE_OPEN,
                        TOKEN_BRACKET_OPEN,
                    };
                    expectErrorLog(s, aTokens, K_ASIZE(aTokens), false);
                    return false;
                }
            }
        }
        break;

        case TOKEN_BRACE_CLOSE:
        return true;

        default:
        {
            const int aTokens[] = {TOKEN_QSTRING, TOKEN_BRACE_CLOSE};
            expectErrorLog(s, aTokens, K_ASIZE(aTokens), false);
            return false;
        }
    }

    if (!nextToken(s)) return false;

    switch (s->tok.eType)
    {
        case TOKEN_COMMA:
        goto gotComma;

        case TOKEN_BRACE_CLOSE:
        break;

        default:
        {
            const int aTokens[] = {TOKEN_COMMA, TOKEN_BRACE_CLOSE};
            expectErrorLog(s, aTokens, K_ASIZE(aTokens), false);
            return false;
        }
    }

    return true;
}

k_JsonTraverseResult
k_JsonTraverse(
    k_JsonValue* pVal,
    const k_StringView svNameOrEmpty,
    bool (*pfn)(k_JsonValue* pNV, const k_StringView svNameOrEmpty, void* pArg),
    void* pArg
)
{
    if (pfn(pVal, svNameOrEmpty, pArg))
    {
        return (k_JsonTraverseResult){
            .pValOrNull = pVal,
            .svNameOrEmpty = svNameOrEmpty,
            .bReturn = true
        };
    }

    switch (pVal->eType)
    {
        case K_JSON_TYPE_ARRAY:
        K_VEC_FOR_EACH(&pVal->array.vValues, k_JsonValue, pIt)
        {
            k_JsonTraverseResult res = k_JsonTraverse(pIt, (k_StringView){0}, pfn, pArg);
            if (res.bReturn) return res;
        }
        break;

        case K_JSON_TYPE_OBJECT:
        K_VEC_FOR_EACH(&pVal->object.vNameValues, k_JsonNameValue, pIt)
        {
            k_JsonTraverseResult res = k_JsonTraverse(&pIt->val, pIt->svName, pfn, pArg);
            if (res.bReturn) return res;
        }
        break;
    }

    return (k_JsonTraverseResult){0};
}

void
k_JsonValuePrint(k_JsonValue* pVal, k_print_Builder* pBuilder, int depth)
{
    if (!pVal)
    {
        k_print_BuilderPushSv(pBuilder, K_SV("null"));
        return;
    }

    switch (pVal->eType)
    {
        case K_JSON_TYPE_OBJECT:
        k_JsonObjectPrint(&pVal->object, pBuilder, depth);
        break;

        case K_JSON_TYPE_ARRAY:
        k_JsonArrayPrint(&pVal->array, pBuilder, depth);
        break;

        case K_JSON_TYPE_UNKNOWN:
        k_print_BuilderPushSv(pBuilder, K_SV("UNKNOWN"));
        break;

        default:
        k_print_BuilderPushSv(pBuilder, pVal->svValue);
        break;
    }
}

void
k_JsonValueDestroy(k_JsonValue* pVal, k_IAllocator* pAlloc)
{
    if (!pVal) return;

    switch (pVal->eType)
    {
        case K_JSON_TYPE_OBJECT:
        k_JsonObjectDestroy(&pVal->object, pAlloc);
        break;

        case K_JSON_TYPE_ARRAY:
        k_JsonArrayDestroy(&pVal->array, pAlloc);
        break;
    }
}

bool
k_JsonParserParse(k_JsonParser* s, k_IAllocator* pAlloc, const k_StringView svText)
{
    s->svText = svText;
    s->i = 0;
    s->x = 1;
    s->y = 1;
    s->root.vValues = (k_Vec){0};

    if (!nextToken(s)) return false;

    if (s->tok.eType == TOKEN_EOF)
        return true;

    if (s->tok.eType != TOKEN_BRACE_OPEN && s->tok.eType != TOKEN_BRACKET_OPEN)
    {
        const int aTokens[] = {TOKEN_BRACE_OPEN, TOKEN_BRACKET_OPEN};
        expectErrorLog(s, aTokens, K_ASIZE(aTokens), false);
        return false;
    }

    while (s->tok.eType == TOKEN_BRACE_OPEN || s->tok.eType == TOKEN_BRACKET_OPEN)
    {
        const K_JSON_TYPE eType = s->tok.eType == TOKEN_BRACE_OPEN ? K_JSON_TYPE_OBJECT : K_JSON_TYPE_ARRAY;
        const ssize_t n = k_VecPush(&s->root.vValues, pAlloc, sizeof(k_JsonValue), &(k_JsonValue){.eType = eType});
        if (n < 0) return false;
        k_JsonValue* pNewVal = (k_JsonValue*)s->root.vValues.pData + n;

        if (eType == K_JSON_TYPE_OBJECT)
        {
            if (!parseObject(s, pAlloc, &pNewVal->object)) return false;
        }
        else
        {
            if (!parseArray(s, pAlloc, &pNewVal->array)) return false;
        }

        if (!nextToken(s)) return false;
    }

    return true;
}

void
k_JsonParserDestroy(k_JsonParser* s, k_IAllocator* pAlloc)
{
    k_JsonArrayDestroy(&s->root, pAlloc);
}

void
k_JsonArrayPrint(const k_JsonArray* pArr, k_print_Builder* pBuilder, int depth)
{
    k_print_FmtArgs fmtArgs = k_print_FmtArgsCreate();
    fmtArgs.padSize = depth;
    k_print_FmtArgs fmtArgsNext = fmtArgs;
    fmtArgsNext.padSize += 2;

    if (pArr->vValues.size <= 0)
    {
        k_print_BuilderPushSv(pBuilder, K_SV("[]"));
        return;
    }

    k_print_BuilderPushSv(pBuilder, K_SV("[\n"));

    K_VEC_FOR_EACH(&pArr->vValues, k_JsonValue, pIt)
    {
        k_print_BuilderPushSvPaddedFmtArgs(pBuilder, &fmtArgsNext, K_SV(""));

        switch (pIt->eType)
        {
            case K_JSON_TYPE_OBJECT:
            k_JsonObjectPrint(&pIt->object, pBuilder, depth + 2);
            break;

            case K_JSON_TYPE_ARRAY:
            k_JsonArrayPrint(&pIt->array, pBuilder, depth + 2);
            break;

            case K_JSON_TYPE_STRING:
            k_print_BuilderPushSv(pBuilder, K_SV("\""));
            k_print_BuilderPushSv(pBuilder, pIt->svValue);
            k_print_BuilderPushSv(pBuilder, K_SV("\""));
            break;

            default:
            k_print_BuilderPushSv(pBuilder, pIt->svValue);
            break;
        }

        if (pIt - (k_JsonValue*)pArr->vValues.pData != pArr->vValues.size - 1)
            k_print_BuilderPushSv(pBuilder, K_SV(",\n"));
        else k_print_BuilderPushSv(pBuilder, K_SV("\n"));
    }

    k_print_BuilderPushSvPaddedFmtArgs(pBuilder, &fmtArgs, K_SV(""));
    k_print_BuilderPushSv(pBuilder, K_SV("]"));
}

void
k_JsonObjectPrint(const k_JsonObject* pObj, k_print_Builder* pBuilder, int depth)
{
    k_print_FmtArgs fmtArgs = k_print_FmtArgsCreate();
    fmtArgs.padSize = depth;
    k_print_FmtArgs fmtArgsNext = fmtArgs;
    fmtArgsNext.padSize += 2;

    if (pObj->vNameValues.size <= 0)
    {
        k_print_BuilderPushSv(pBuilder, K_SV("{}"));
        return;
    }

    k_print_BuilderPushSv(pBuilder, K_SV("{\n"));
    K_VEC_FOR_EACH(&pObj->vNameValues, k_JsonNameValue, pIt)
    {
        k_print_BuilderPushSvPaddedFmtArgs(pBuilder, &fmtArgsNext, K_SV(""));
        k_print_BuilderPushSv(pBuilder, K_SV("\""));
        k_print_BuilderPushSv(pBuilder, pIt->svName);
        k_print_BuilderPushSv(pBuilder, K_SV("\": "));

        switch (pIt->val.eType)
        {
            case K_JSON_TYPE_OBJECT:
            k_JsonObjectPrint(&pIt->val.object, pBuilder, depth + 2);
            break;

            case K_JSON_TYPE_ARRAY:
            k_JsonArrayPrint(&pIt->val.array, pBuilder, depth + 2);
            break;

            case K_JSON_TYPE_STRING:
            k_print_BuilderPushSv(pBuilder, K_SV("\""));
            k_print_BuilderPushSv(pBuilder, pIt->val.svValue);
            k_print_BuilderPushSv(pBuilder, K_SV("\""));
            break;

            default:
            k_print_BuilderPushSv(pBuilder, pIt->val.svValue);
            break;
        }

        if (pIt - (k_JsonNameValue*)pObj->vNameValues.pData != pObj->vNameValues.size - 1)
            k_print_BuilderPushSv(pBuilder, K_SV(",\n"));
        else k_print_BuilderPushSv(pBuilder, K_SV("\n"));
    }

    k_print_BuilderPushSvPaddedFmtArgs(pBuilder, &fmtArgs, K_SV(""));
    k_print_BuilderPushSv(pBuilder, K_SV("}"));
}

void
k_JsonParserPrint(const k_JsonParser* s, k_print_Builder* pBuilder)
{
    k_JsonArrayPrint(&s->root, pBuilder, 0);
}

void
k_JsonObjectDestroy(k_JsonObject* s, k_IAllocator* pAlloc)
{
    K_VEC_FOR_EACH(&s->vNameValues, k_JsonNameValue, pIt)
    {
        switch (pIt->val.eType)
        {
            case K_JSON_TYPE_OBJECT:
            k_JsonObjectDestroy(&pIt->val.object, pAlloc);
            break;

            case K_JSON_TYPE_ARRAY:
            k_JsonArrayDestroy(&pIt->val.array, pAlloc);
            break;
        }
    }

    k_VecDestroy(&s->vNameValues, pAlloc);
}

void
k_JsonArrayDestroy(k_JsonArray* s, k_IAllocator* pAlloc)
{
    K_VEC_FOR_EACH(&s->vValues, k_JsonValue, pIt)
    {
        switch (pIt->eType)
        {
            case K_JSON_TYPE_OBJECT:
            k_JsonObjectDestroy(&pIt->object, pAlloc);
            break;

            case K_JSON_TYPE_ARRAY:
            k_JsonArrayDestroy(&pIt->array, pAlloc);
            break;
        }
    }

    k_VecDestroy(&s->vValues, pAlloc);
}
