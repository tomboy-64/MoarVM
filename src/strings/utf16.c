#include "moar.h"

#define BOM_UTF16LE "\xff\xfe"
#define BOM_UTF16BE "\xfe\xff"

/* mostly from YAML-LibYAML */

/* Decodes the specified number of bytes of utf16 into an NFG string, creating
 * a result of the specified type. The type must have the MVMString REPR. */
MVMString * MVM_string_utf16_decode(MVMThreadContext *tc,
        const MVMObject *result_type, char *utf16_chars, size_t bytes) {
    MVMString *result = (MVMString *)REPR(result_type)->allocate(tc, STABLE(result_type));
    size_t str_pos = 0;
    MVMuint8 *utf16 = (MVMuint8 *)utf16_chars;
    MVMuint8 *utf16_end;
    /* set the default byte order */
#ifdef MVM_BIGENDIAN
    int low = 1;
    int high = 0;
#else
    int low = 0;
    int high = 1;
#endif
    MVMNormalizer norm;
    MVMint32 ready;

    if (bytes % 2) {
        MVM_exception_throw_adhoc(tc, "Malformed UTF-16; odd number of bytes");
    }

    /* set the byte order if there's a BOM */
    if (bytes >= 2) {
        if (!memcmp(utf16, BOM_UTF16LE, 2)) {
            low = 0;
            high = 1;
            utf16 += 2;
            bytes -= 2;
        }
        else if (!memcmp(utf16, BOM_UTF16BE, 2)) {
            low = 1;
            high = 0;
            utf16 += 2;
            bytes -= 2;
        }
    }
    utf16_end = utf16 + bytes;

    /* possibly allocating extra space; oh well */
    result->body.storage.blob_32 = MVM_malloc(sizeof(MVMGrapheme32) * bytes / 2);

    /* Need to normalize to NFG as we decode. */
    MVM_unicode_normalizer_init(tc, &norm, MVM_NORMALIZE_NFG);

    for (; utf16 < utf16_end; utf16 += 2) {
        MVMuint32 value = (utf16[high] << 8) + utf16[low];
        MVMuint32 value2;
        MVMGrapheme32 g;

        if ((value & 0xFC00) == 0xDC00) {
            MVM_unicode_normalizer_cleanup(tc, &norm);
            MVM_exception_throw_adhoc(tc, "Malformed UTF-16; unexpected low surrogate");
        }

        if ((value & 0xFC00) == 0xD800) { /* high surrogate */
            utf16 += 2;
            if (utf16 == utf16_end) {
                MVM_unicode_normalizer_cleanup(tc, &norm);
                MVM_exception_throw_adhoc(tc, "Malformed UTF-16; incomplete surrogate pair");
            }
            value2 = (utf16[high] << 8) + utf16[low];
            if ((value2 & 0xFC00) != 0xDC00) {
                MVM_unicode_normalizer_cleanup(tc, &norm);
                MVM_exception_throw_adhoc(tc, "Malformed UTF-16; incomplete surrogate pair");
            }
            value = 0x10000 + ((value & 0x3FF) << 10) + (value2 & 0x3FF);
        }

        /* TODO: check for invalid values */
        ready = MVM_unicode_normalizer_process_codepoint_to_grapheme(tc, &norm, value, &g);
        if (ready) {
            result->body.storage.blob_32[str_pos++] = g;
            while (--ready > 0)
                result->body.storage.blob_32[str_pos++] = MVM_unicode_normalizer_get_grapheme(tc, &norm);
        }
    }

    /* Get any final graphemes from the normalizer, and clean it up. */
    MVM_unicode_normalizer_eof(tc, &norm);
    ready = MVM_unicode_normalizer_available(tc, &norm);
    while (ready--)
        result->body.storage.blob_32[str_pos++] = MVM_unicode_normalizer_get_grapheme(tc, &norm);
    MVM_unicode_normalizer_cleanup(tc, &norm);

    result->body.storage_type = MVM_STRING_GRAPHEME_32;
    result->body.num_graphs   = str_pos;

    return result;
}

/* Encodes the specified substring to utf16. The result string is NULL terminated, but
 * the specified size is the non-null part. (This being UTF-16, there are 2 null bytes
 * on the end.) */
char * MVM_string_utf16_encode_substr(MVMThreadContext *tc, MVMString *str, MVMuint64 *output_size, MVMint64 start, MVMint64 length, MVMString *replacement, MVMint32 translate_newlines) {
    MVMStringIndex strgraphs = MVM_string_graphs(tc, str);
    MVMuint32 lengthu = (MVMuint32)(length == -1 ? strgraphs - start : length);
    MVMuint16 *result;
    MVMuint16 *result_pos;
    MVMCodepointIter ci;
    MVMuint8 *repl_bytes = NULL;
    MVMuint64 repl_length = 0;
    MVMint32 alloc_size;

    /* must check start first since it's used in the length check */
    if (start < 0 || start > strgraphs)
        MVM_exception_throw_adhoc(tc, "start out of range");
    if (start + lengthu > strgraphs)
        MVM_exception_throw_adhoc(tc, "length out of range");

    if (replacement)
        repl_bytes = (MVMuint8 *) MVM_string_utf16_encode_substr(tc,
            replacement, &repl_length, 0, -1, NULL, translate_newlines);

    alloc_size = lengthu * 2;
    result = MVM_malloc(alloc_size + 2);
    result_pos = result;
    MVM_string_ci_init(tc, &ci, str, translate_newlines);
    while (MVM_string_ci_has_more(tc, &ci)) {
        int bytes_needed;
        MVMCodepoint value = MVM_string_ci_get_codepoint(tc, &ci);

        if (value < 0x10000) {
            bytes_needed = 2;
        }
        else if (value <= 0x1FFFFF) {
            bytes_needed = 4;
        }
        else {
            bytes_needed = repl_length;
        }

        while ((alloc_size - 2 * (result_pos - result)) < bytes_needed) {
            MVMuint16 *new_result;

            alloc_size *= 2;
            new_result  = MVM_realloc(result, alloc_size + 2);

            result_pos = new_result + (result_pos - result);
            result     = new_result;
        }

        if (value < 0x10000) {
            result_pos[0] = value;
            result_pos++;
        }
        else if (value <= 0x1FFFFF) {
            value -= 0x10000;
            result_pos[0] = 0xD800 + (value >> 10);
            result_pos[1] = 0xDC00 + (value & 0x3FF);
            result_pos += 2;
        }
        else if (replacement) {
            memcpy(result_pos, repl_bytes, repl_length);
            result_pos += repl_length/2;
        }
        else {
            MVM_free(result);
            MVM_free(repl_bytes);
            MVM_exception_throw_adhoc(tc,
                "Error encoding UTF-16 string: could not encode codepoint %d",
                value);
        }
    }
    result_pos[0] = 0;
    if (output_size)
        *output_size = (char *)result_pos - (char *)result;
    result = MVM_realloc(result, *output_size);
    MVM_free(repl_bytes);
    return (char *)result;
}

/* Encodes the whole string, double-NULL terminated. */
char * MVM_string_utf16_encode(MVMThreadContext *tc, MVMString *str, MVMint32 translate_newlines) {
    return MVM_string_utf16_encode_substr(tc, str, NULL, 0, -1, NULL, translate_newlines);
}
