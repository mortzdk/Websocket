/**
 * Thanks to the AUTHORS of https://github.com/lemire/fastvalidate-utf-8/
 *
 * Permission is hereby granted, free of charge, to any
 * person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the
 * Software without restriction, including without
 * limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission notice
 * shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
/* Microsoft C/C++-compatible compiler */
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
/* GCC-compatible compiler, targeting x86/x86-64 */
#include <x86intrin.h>
#elif defined(__GNUC__) && defined(__ARM_NEON__)
/* GCC-compatible compiler, targeting ARM with NEON */
#include <arm_neon.h>
#elif defined(__GNUC__) && defined(__IWMMXT__)
/* GCC-compatible compiler, targeting ARM with WMMX */
#include <mmintrin.h>
#elif (defined(__GNUC__) || defined(__xlC__)) && (defined(__VEC__) || defined(__ALTIVEC__))
/* XLC or GCC-compatible compiler, targeting PowerPC with VMX/VSX */
#include <altivec.h>
#elif defined(__GNUC__) && defined(__SPE__)
/* GCC-compatible compiler, targeting PowerPC with SPE */
#include <spe.h>
#endif

#if defined(__AVX512F__) && defined(__AVX512VL__) && defined(__AVX512VBMI__)

/*****************************/
static inline __m512i avx512_push_last_byte_of_a_to_b(__m512i a, __m512i b) {
    __m512i indexes = _mm512_set_epi64(0x3E3D3C3B3A393837, 0x363534333231302F,
            0x2E2D2C2B2A292827, 0x262524232221201F,
            0x1E1D1C1B1A191817, 0x161514131211100F,
            0x0E0D0C0B0A090807, 0x060504030201007F);
    return _mm512_permutex2var_epi8(b, indexes, a);
}

static inline __m512i avx512_push_last_2bytes_of_a_to_b(__m512i a, __m512i b) {
    __m512i indexes = _mm512_set_epi64(0x3D3C3B3A39383736, 0x3534333231302F2E,
            0x2D2C2B2A29282726, 0x2524232221201F1E,
            0x1D1C1B1A19181716, 0x1514131211100F0E,
            0x0D0C0B0A09080706, 0x0504030201007F7E);
    return _mm512_permutex2var_epi8(b, indexes, a);
}

// all byte values must be no larger than 0xF4
static inline void avx512_checkSmallerThan0xF4(__m512i current_bytes,
        __mmask64 *has_error) {
    *has_error =
        _kor_mask64(*has_error, _mm512_cmpgt_epu8_mask(current_bytes,
                    _mm512_set1_epi8(0xF4)));
}

static inline __m512i avx512_continuationLengths(__m512i high_nibbles) {
    return _mm512_shuffle_epi8(
            _mm512_setr4_epi32(0x01010101, 0x01010101, 0x00000000,
                0x04030202), // see avx2 version for clarity
            high_nibbles);
}

static inline __m512i avx512_carryContinuations(__m512i initial_lengths,
        __m512i previous_carries) {

    __m512i right1 = _mm512_subs_epu8(
            avx512_push_last_byte_of_a_to_b(previous_carries, initial_lengths),
            _mm512_set1_epi8(1));
    __m512i sum = _mm512_add_epi8(initial_lengths, right1);

    __m512i right2 =
        _mm512_subs_epu8(avx512_push_last_2bytes_of_a_to_b(previous_carries, sum),
                _mm512_set1_epi8(2));
    return _mm512_add_epi8(sum, right2);
}

static inline void avx512_checkContinuations(__m512i initial_lengths,
        __m512i carries,
        __mmask64 *has_error) {
    // overlap || underlap
    // carry > length && length > 0 || !(carry > length) && !(length > 0)
    // (carries > length) == (lengths > 0)
    *has_error = _kor_mask64(
            *has_error,
            _kxnor_mask64(
                _mm512_cmpgt_epi8_mask(carries, initial_lengths),
                _mm512_cmpgt_epi8_mask(initial_lengths, _mm512_setzero_si512())));
}

// when 0xED is found, next byte must be no larger than 0x9F
// when 0xF4 is found, next byte must be no larger than 0x8F
// next byte must be continuation, ie sign bit is set, so signed < is ok
static inline void avx512_checkFirstContinuationMax(__m512i current_bytes,
        __m512i off1_current_bytes,
        __mmask64 *has_error) {
    __mmask64 maskED =
        _mm512_cmpeq_epi8_mask(off1_current_bytes, _mm512_set1_epi8((char)0xED));
    __mmask64 maskF4 =
        _mm512_cmpeq_epi8_mask(off1_current_bytes, _mm512_set1_epi8((char)0xF4));
    __mmask64 badfollowED = _kand_mask64(
            _mm512_cmpgt_epi8_mask(current_bytes, _mm512_set1_epi8((char)0x9F)), maskED);
    __mmask64 badfollowF4 = _kand_mask64(
            _mm512_cmpgt_epi8_mask(current_bytes, _mm512_set1_epi8((char)0x8F)), maskF4);

    *has_error = _kor_mask64(*has_error, _kor_mask64(badfollowED, badfollowF4));
}

// map off1_hibits => error condition
// hibits     off1    cur
// C       => < C2 && true
// E       => < E1 && < A0
// F       => < F1 && < 90
// else      false && false
static inline void avx512_checkOverlong(__m512i current_bytes,
        __m512i off1_current_bytes,
        __m512i hibits, __m512i previous_hibits,
        __mmask64 *has_error) {
    __m512i off1_hibits =
        avx512_push_last_byte_of_a_to_b(previous_hibits, hibits);
    __m512i initial_mins = _mm512_shuffle_epi8(
            _mm512_setr4_epi32(0x80808080, 0x80808080, 0x80808080,
                0xF1E180C2), // see avx2 version for clarity
            off1_hibits);

    __mmask64 initial_under =
        _mm512_cmpgt_epi8_mask(initial_mins, off1_current_bytes);

    __m512i second_mins = _mm512_shuffle_epi8(
            _mm512_setr4_epi32(0x80808080, 0x80808080, 0x80808080,
                0x90A07F7F), // see avx2 version for clarity
            off1_hibits);
    __mmask64 second_under = _mm512_cmpgt_epi8_mask(second_mins, current_bytes);
    *has_error =
        _kor_mask64(*has_error, _kand_mask64(initial_under, second_under));
}

struct avx512_processed_utf_bytes {
    __m512i rawbytes;
    __m512i high_nibbles;
    __m512i carried_continuations;
};

static inline void
avx512_count_nibbles(__m512i bytes, struct avx512_processed_utf_bytes *answer) {
    answer->rawbytes = bytes;
    answer->high_nibbles =
        _mm512_and_si512(_mm512_srli_epi16(bytes, 4), _mm512_set1_epi8((char)0x0F));
}

// check whether the current bytes are valid UTF-8
// at the end of the function, previous gets updated
static struct avx512_processed_utf_bytes
avx512_checkUTF8Bytes(__m512i current_bytes,
        struct avx512_processed_utf_bytes *previous,
        __mmask64 *has_error) {
    struct avx512_processed_utf_bytes pb;
    avx512_count_nibbles(current_bytes, &pb);

    avx512_checkSmallerThan0xF4(current_bytes, has_error);

    __m512i initial_lengths = avx512_continuationLengths(pb.high_nibbles);

    pb.carried_continuations = avx512_carryContinuations(
            initial_lengths, previous->carried_continuations);

    avx512_checkContinuations(initial_lengths, pb.carried_continuations,
            has_error);

    __m512i off1_current_bytes =
        avx512_push_last_byte_of_a_to_b(previous->rawbytes, pb.rawbytes);
    avx512_checkFirstContinuationMax(current_bytes, off1_current_bytes,
            has_error);

    avx512_checkOverlong(current_bytes, off1_current_bytes, pb.high_nibbles,
            previous->high_nibbles, has_error);
    return pb;
}

// check whether the current bytes are valid UTF-8
// at the end of the function, previous gets updated
static struct avx512_processed_utf_bytes
avx512_checkUTF8Bytes_asciipath(__m512i current_bytes,
        struct avx512_processed_utf_bytes *previous,
        __mmask64 *has_error) {
    if (!_mm512_cmpge_epu8_mask(current_bytes,
                _mm512_set1_epi8((char)0x80))) { // fast ascii path
        *has_error = _kor_mask64(
                *has_error,
                _mm512_cmpgt_epi8_mask(
                    previous->carried_continuations,
                    _mm512_setr_epi32(0x09090909, 0x09090909, 0x09090909, 0x09090909,
                        0x09090909, 0x09090909, 0x09090909, 0x09090909,
                        0x09090909, 0x09090909, 0x09090909, 0x09090909,
                        0x09090909, 0x09090909, 0x09090909, 0x01090909)));
        return *previous;
    }

    struct avx512_processed_utf_bytes pb;
    avx512_count_nibbles(current_bytes, &pb);

    avx512_checkSmallerThan0xF4(current_bytes, has_error);

    __m512i initial_lengths = avx512_continuationLengths(pb.high_nibbles);

    pb.carried_continuations = avx512_carryContinuations(
            initial_lengths, previous->carried_continuations);

    avx512_checkContinuations(initial_lengths, pb.carried_continuations,
            has_error);

    __m512i off1_current_bytes =
        avx512_push_last_byte_of_a_to_b(previous->rawbytes, pb.rawbytes);
    avx512_checkFirstContinuationMax(current_bytes, off1_current_bytes,
            has_error);

    avx512_checkOverlong(current_bytes, off1_current_bytes, pb.high_nibbles,
            previous->high_nibbles, has_error);
    return pb;
}

static bool validate_utf8_fast_avx512_asciipath(const char *src, size_t len) {
    size_t i = 0;
    __mmask64 has_error = 0;
    struct avx512_processed_utf_bytes previous = {
        .rawbytes = _mm512_setzero_si512(),
        .high_nibbles = _mm512_setzero_si512(),
        .carried_continuations = _mm512_setzero_si512()
    };
    if (len >= 64) {
        for (; i <= len - 64; i += 64) {
            __m512i current_bytes = _mm512_loadu_si512((const __m512i *)(src + i));
            previous =
                avx512_checkUTF8Bytes_asciipath(current_bytes, &previous, &has_error);
        }
    }

    // last part
    if (i < len) {
        char buffer[64];
        memset(buffer, 0, 64);
        memcpy(buffer, src + i, len - i);
        __m512i current_bytes = _mm512_loadu_si512((const __m512i *)(buffer));
        previous = avx512_checkUTF8Bytes(current_bytes, &previous, &has_error);
    } else {
        has_error = _kor_mask64(
                has_error,
                _mm512_cmpgt_epi8_mask(
                    previous.carried_continuations,
                    _mm512_setr_epi32(0x09090909, 0x09090909, 0x09090909, 0x09090909,
                        0x09090909, 0x09090909, 0x09090909, 0x09090909,
                        0x09090909, 0x09090909, 0x09090909, 0x09090909,
                        0x09090909, 0x09090909, 0x09090909, 0x01090909)));
    }

    return !has_error;
}

bool utf8_check(const char *src, size_t len) {
    size_t i = 0;
    __mmask64 has_error = 0;
    struct avx512_processed_utf_bytes previous = {
        .rawbytes = _mm512_setzero_si512(),
        .high_nibbles = _mm512_setzero_si512(),
        .carried_continuations = _mm512_setzero_si512()
    };
    if (len >= 64) {
        for (; i <= len - 64; i += 64) {
            __m512i current_bytes = _mm512_loadu_si512((const __m512i *)(src + i));
            previous = avx512_checkUTF8Bytes(current_bytes, &previous, &has_error);
        }
    }

    // last part
    if (i < len) {
        char buffer[64];
        memset(buffer, 0, 64);
        memcpy(buffer, src + i, len - i);
        __m512i current_bytes = _mm512_loadu_si512((const __m512i *)(buffer));
        previous = avx512_checkUTF8Bytes(current_bytes, &previous, &has_error);
    } else {
        has_error = _kor_mask64(
                has_error,
                _mm512_cmpgt_epi8_mask(
                    previous.carried_continuations,
                    _mm512_setr_epi32(0x09090909, 0x09090909, 0x09090909, 0x09090909,
                        0x09090909, 0x09090909, 0x09090909, 0x09090909,
                        0x09090909, 0x09090909, 0x09090909, 0x09090909,
                        0x09090909, 0x09090909, 0x09090909, 0x01090909)));
    }

    return !has_error;
}

#elif defined(__AVX2__) && defined(__AVX__)

/*****************************/
static inline __m256i push_last_byte_of_a_to_b(__m256i a, __m256i b) {
    return _mm256_alignr_epi8(b, _mm256_permute2x128_si256(a, b, 0x21), 15);
}

static inline __m256i push_last_2bytes_of_a_to_b(__m256i a, __m256i b) {
    return _mm256_alignr_epi8(b, _mm256_permute2x128_si256(a, b, 0x21), 14);
}

// all byte values must be no larger than 0xF4
static inline void avxcheckSmallerThan0xF4(__m256i current_bytes,
        __m256i *has_error) {
    // unsigned, saturates to 0 below max
    *has_error = _mm256_or_si256(
            *has_error, _mm256_subs_epu8(current_bytes, _mm256_set1_epi8((char)0xF4)));
}

static inline __m256i avxcontinuationLengths(__m256i high_nibbles) {
    return _mm256_shuffle_epi8(
            _mm256_setr_epi8(1, 1, 1, 1, 1, 1, 1, 1, // 0xxx (ASCII)
                0, 0, 0, 0,             // 10xx (continuation)
                2, 2,                   // 110x
                3,                      // 1110
                4, // 1111, next should be 0 (not checked here)
                1, 1, 1, 1, 1, 1, 1, 1, // 0xxx (ASCII)
                0, 0, 0, 0,             // 10xx (continuation)
                2, 2,                   // 110x
                3,                      // 1110
                4 // 1111, next should be 0 (not checked here)
                ),
            high_nibbles);
}

static inline __m256i avxcarryContinuations(__m256i initial_lengths,
        __m256i previous_carries) {

    __m256i right1 = _mm256_subs_epu8(
            push_last_byte_of_a_to_b(previous_carries, initial_lengths),
            _mm256_set1_epi8(1));
    __m256i sum = _mm256_add_epi8(initial_lengths, right1);

    __m256i right2 = _mm256_subs_epu8(
            push_last_2bytes_of_a_to_b(previous_carries, sum), _mm256_set1_epi8(2));
    return _mm256_add_epi8(sum, right2);
}

static inline void avxcheckContinuations(__m256i initial_lengths,
        __m256i carries, __m256i *has_error) {

    // overlap || underlap
    // carry > length && length > 0 || !(carry > length) && !(length > 0)
    // (carries > length) == (lengths > 0)
    __m256i overunder = _mm256_cmpeq_epi8(
            _mm256_cmpgt_epi8(carries, initial_lengths),
            _mm256_cmpgt_epi8(initial_lengths, _mm256_setzero_si256()));

    *has_error = _mm256_or_si256(*has_error, overunder);
}

// when 0xED is found, next byte must be no larger than 0x9F
// when 0xF4 is found, next byte must be no larger than 0x8F
// next byte must be continuation, ie sign bit is set, so signed < is ok
static inline void avxcheckFirstContinuationMax(__m256i current_bytes,
        __m256i off1_current_bytes,
        __m256i *has_error) {
    __m256i maskED =
        _mm256_cmpeq_epi8(off1_current_bytes, _mm256_set1_epi8((char)0xED));
    __m256i maskF4 =
        _mm256_cmpeq_epi8(off1_current_bytes, _mm256_set1_epi8((char)0xF4));

    __m256i badfollowED = _mm256_and_si256(
            _mm256_cmpgt_epi8(current_bytes, _mm256_set1_epi8((char)0x9F)), maskED);
    __m256i badfollowF4 = _mm256_and_si256(
            _mm256_cmpgt_epi8(current_bytes, _mm256_set1_epi8((char)0x8F)), maskF4);

    *has_error =
        _mm256_or_si256(*has_error, _mm256_or_si256(badfollowED, badfollowF4));
}

// map off1_hibits => error condition
// hibits     off1    cur
// C       => < C2 && true
// E       => < E1 && < A0
// F       => < F1 && < 90
// else      false && false
static inline void avxcheckOverlong(__m256i current_bytes,
        __m256i off1_current_bytes, __m256i hibits,
        __m256i previous_hibits,
        __m256i *has_error) {
    __m256i off1_hibits = push_last_byte_of_a_to_b(previous_hibits, hibits);
    __m256i initial_mins = _mm256_shuffle_epi8(
            _mm256_setr_epi8(-128, -128, -128, -128, -128, -128, -128, -128, -128,
                -128, -128, -128, // 10xx => false
                (char)0xC2, -128,       // 110x
                (char)0xE1,             // 1110
                (char)0xF1, -128, -128, -128, -128, -128, -128, -128, -128,
                -128, -128, -128, -128, // 10xx => false
                (char)0xC2, -128,             // 110x
                (char)0xE1,                   // 1110
                (char)0xF1),
            off1_hibits);

    __m256i initial_under = _mm256_cmpgt_epi8(initial_mins, off1_current_bytes);

    __m256i second_mins = _mm256_shuffle_epi8(
            _mm256_setr_epi8(-128, -128, -128, -128, -128, -128, -128, -128, -128,
                -128, -128, -128, // 10xx => false
                127, 127,         // 110x => true
                (char)0xA0,             // 1110
                (char)0x90, -128, -128, -128, -128, -128, -128, -128, -128,
                -128, -128, -128, -128, // 10xx => false
                127, 127,               // 110x => true
                (char)0xA0,                   // 1110
                (char)0x90),
            off1_hibits);
    __m256i second_under = _mm256_cmpgt_epi8(second_mins, current_bytes);
    *has_error = _mm256_or_si256(*has_error,
            _mm256_and_si256(initial_under, second_under));
}

struct avx_processed_utf_bytes {
    __m256i rawbytes;
    __m256i high_nibbles;
    __m256i carried_continuations;
};

static inline void avx_count_nibbles(__m256i bytes,
        struct avx_processed_utf_bytes *answer) {
    answer->rawbytes = bytes;
    answer->high_nibbles =
        _mm256_and_si256(_mm256_srli_epi16(bytes, 4), _mm256_set1_epi8((char)0x0F));
}

// check whether the current bytes are valid UTF-8
// at the end of the function, previous gets updated
static struct avx_processed_utf_bytes
avxcheckUTF8Bytes(__m256i current_bytes,
        struct avx_processed_utf_bytes *previous,
        __m256i *has_error) {
    struct avx_processed_utf_bytes pb;
    avx_count_nibbles(current_bytes, &pb);

    avxcheckSmallerThan0xF4(current_bytes, has_error);

    __m256i initial_lengths = avxcontinuationLengths(pb.high_nibbles);

    pb.carried_continuations =
        avxcarryContinuations(initial_lengths, previous->carried_continuations);

    avxcheckContinuations(initial_lengths, pb.carried_continuations, has_error);

    __m256i off1_current_bytes =
        push_last_byte_of_a_to_b(previous->rawbytes, pb.rawbytes);
    avxcheckFirstContinuationMax(current_bytes, off1_current_bytes, has_error);

    avxcheckOverlong(current_bytes, off1_current_bytes, pb.high_nibbles,
            previous->high_nibbles, has_error);
    return pb;
}

bool utf8_check(const char *src, size_t len) {
    size_t i = 0;
    __m256i has_error = _mm256_setzero_si256();
    struct avx_processed_utf_bytes previous = {
        .rawbytes = _mm256_setzero_si256(),
        .high_nibbles = _mm256_setzero_si256(),
        .carried_continuations = _mm256_setzero_si256()};
    if (len >= 32) {
        for (; i <= len - 32; i += 32) {
            __m256i current_bytes = _mm256_loadu_si256((const __m256i *)(src + i));
            previous = avxcheckUTF8Bytes(current_bytes, &previous, &has_error);
        }
    }

    // last part
    if (i < len) {
        char buffer[32];
        memset(buffer, 0, 32);
        memcpy(buffer, src + i, len - i);
        __m256i current_bytes = _mm256_loadu_si256((const __m256i *)(buffer));
        previous = avxcheckUTF8Bytes(current_bytes, &previous, &has_error);
    } else {
        has_error = _mm256_or_si256(
                _mm256_cmpgt_epi8(previous.carried_continuations,
                    _mm256_setr_epi8(9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
                        9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
                        9, 9, 9, 9, 9, 9, 9, 1)),
                has_error);
    }

    return _mm256_testz_si256(has_error, has_error);
}

#elif defined(__SSE2__)

/*
 * legal utf-8 byte sequence
 * http://www.unicode.org/versions/Unicode6.0.0/ch03.pdf - page 94
 *
 *  Code Points        1st       2s       3s       4s
 * U+0000..U+007F     00..7F
 * U+0080..U+07FF     C2..DF   80..BF
 * U+0800..U+0FFF     E0       A0..BF   80..BF
 * U+1000..U+CFFF     E1..EC   80..BF   80..BF
 * U+D000..U+D7FF     ED       80..9F   80..BF
 * U+E000..U+FFFF     EE..EF   80..BF   80..BF
 * U+10000..U+3FFFF   F0       90..BF   80..BF   80..BF
 * U+40000..U+FFFFF   F1..F3   80..BF   80..BF   80..BF
 * U+100000..U+10FFFF F4       80..8F   80..BF   80..BF
 *
 */

// all byte values must be no larger than 0xF4
static inline void checkSmallerThan0xF4(__m128i current_bytes,
        __m128i *has_error) {
    // unsigned, saturates to 0 below max
    *has_error = _mm_or_si128(*has_error,
            _mm_subs_epu8(current_bytes, _mm_set1_epi8((char)0xF4)));
}

static inline __m128i continuationLengths(__m128i high_nibbles) {
    return _mm_shuffle_epi8(
            _mm_setr_epi8(1, 1, 1, 1, 1, 1, 1, 1, // 0xxx (ASCII)
                0, 0, 0, 0,             // 10xx (continuation)
                2, 2,                   // 110x
                3,                      // 1110
                4), // 1111, next should be 0 (not checked here)
            high_nibbles);
}

static inline __m128i carryContinuations(__m128i initial_lengths,
        __m128i previous_carries) {

    __m128i right1 =
        _mm_subs_epu8(_mm_alignr_epi8(initial_lengths, previous_carries, 16 - 1),
                _mm_set1_epi8(1));
    __m128i sum = _mm_add_epi8(initial_lengths, right1);

    __m128i right2 = _mm_subs_epu8(_mm_alignr_epi8(sum, previous_carries, 16 - 2),
            _mm_set1_epi8(2));
    return _mm_add_epi8(sum, right2);
}

static inline void checkContinuations(__m128i initial_lengths, __m128i carries,
        __m128i *has_error) {

    // overlap || underlap
    // carry > length && length > 0 || !(carry > length) && !(length > 0)
    // (carries > length) == (lengths > 0)
    __m128i overunder =
        _mm_cmpeq_epi8(_mm_cmpgt_epi8(carries, initial_lengths),
                _mm_cmpgt_epi8(initial_lengths, _mm_setzero_si128()));

    *has_error = _mm_or_si128(*has_error, overunder);
}

// when 0xED is found, next byte must be no larger than 0x9F
// when 0xF4 is found, next byte must be no larger than 0x8F
// next byte must be continuation, ie sign bit is set, so signed < is ok
static inline void checkFirstContinuationMax(__m128i current_bytes,
        __m128i off1_current_bytes,
        __m128i *has_error) {
    __m128i maskED = _mm_cmpeq_epi8(off1_current_bytes, _mm_set1_epi8((char)0xED));
    __m128i maskF4 = _mm_cmpeq_epi8(off1_current_bytes, _mm_set1_epi8((char)0xF4));

    __m128i badfollowED =
        _mm_and_si128(_mm_cmpgt_epi8(current_bytes, _mm_set1_epi8((char)0x9F)), maskED);
    __m128i badfollowF4 =
        _mm_and_si128(_mm_cmpgt_epi8(current_bytes, _mm_set1_epi8((char)0x8F)), maskF4);

    *has_error = _mm_or_si128(*has_error, _mm_or_si128(badfollowED, badfollowF4));
}

// map off1_hibits => error condition
// hibits     off1    cur
// C       => < C2 && true
// E       => < E1 && < A0
// F       => < F1 && < 90
// else      false && false
static inline void checkOverlong(__m128i current_bytes,
        __m128i off1_current_bytes, __m128i hibits,
        __m128i previous_hibits, __m128i *has_error) {
    __m128i off1_hibits = _mm_alignr_epi8(hibits, previous_hibits, 16 - 1);
    __m128i initial_mins = _mm_shuffle_epi8(
            _mm_setr_epi8(-128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
                -128, -128, // 10xx => false
                (char)0xC2, -128, // 110x
                (char)0xE1,       // 1110
                (char)0xF1),
            off1_hibits);

    __m128i initial_under = _mm_cmpgt_epi8(initial_mins, off1_current_bytes);

    __m128i second_mins = _mm_shuffle_epi8(
            _mm_setr_epi8(-128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
                -128, -128, // 10xx => false
                127, 127,   // 110x => true
                (char)0xA0,       // 1110
                (char)0x90),
            off1_hibits);
    __m128i second_under = _mm_cmpgt_epi8(second_mins, current_bytes);
    *has_error =
        _mm_or_si128(*has_error, _mm_and_si128(initial_under, second_under));
}

struct processed_utf_bytes {
    __m128i rawbytes;
    __m128i high_nibbles;
    __m128i carried_continuations;
};

static inline void count_nibbles(__m128i bytes,
        struct processed_utf_bytes *answer) {
    answer->rawbytes = bytes;
    answer->high_nibbles =
        _mm_and_si128(_mm_srli_epi16(bytes, 4), _mm_set1_epi8((char)0x0F));
}

// check whether the current bytes are valid UTF-8
// at the end of the function, previous gets updated
static struct processed_utf_bytes
checkUTF8Bytes(__m128i current_bytes, struct processed_utf_bytes *previous,
        __m128i *has_error) {
    struct processed_utf_bytes pb;
    count_nibbles(current_bytes, &pb);

    checkSmallerThan0xF4(current_bytes, has_error);

    __m128i initial_lengths = continuationLengths(pb.high_nibbles);

    pb.carried_continuations =
        carryContinuations(initial_lengths, previous->carried_continuations);

    checkContinuations(initial_lengths, pb.carried_continuations, has_error);

    __m128i off1_current_bytes =
        _mm_alignr_epi8(pb.rawbytes, previous->rawbytes, 16 - 1);
    checkFirstContinuationMax(current_bytes, off1_current_bytes, has_error);

    checkOverlong(current_bytes, off1_current_bytes, pb.high_nibbles,
            previous->high_nibbles, has_error);
    return pb;
}

bool utf8_check(const char *src, size_t len) {
    size_t i = 0;
    __m128i has_error = _mm_setzero_si128();
    struct processed_utf_bytes previous = {.rawbytes = _mm_setzero_si128(),
        .high_nibbles = _mm_setzero_si128(),
        .carried_continuations =
            _mm_setzero_si128()};
    if (len >= 16) {
        for (; i <= len - 16; i += 16) {
            __m128i current_bytes = _mm_loadu_si128((const __m128i *)(src + i));
            previous = checkUTF8Bytes(current_bytes, &previous, &has_error);
        }
    }

    // last part
    if (i < len) {
        char buffer[16];
        memset(buffer, 0, 16);
        memcpy(buffer, src + i, len - i);
        __m128i current_bytes = _mm_loadu_si128((const __m128i *)(buffer));
        previous = checkUTF8Bytes(current_bytes, &previous, &has_error);
    } else {
        has_error =
            _mm_or_si128(_mm_cmpgt_epi8(previous.carried_continuations,
                        _mm_setr_epi8(9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
                            9, 9, 9, 9, 9, 1)),
                    has_error);
    }

    return _mm_testz_si128(has_error, has_error);
}
#else
// credit: @hoehrmann

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

static const uint8_t utf8d[] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 00..1f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 20..3f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 40..5f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 60..7f
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   9,   9,   9,   9,   9,   9,
    9,   9,   9,   9,   9,   9,   9,   9,   9,   9, // 80..9f
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7, // a0..bf
    8,   8,   2,   2,   2,   2,   2,   2,   2,   2,   2,
    2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
    2,   2,   2,   2,   2,   2,   2,   2,   2,   2, // c0..df
    0xa, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
    0x3, 0x3, 0x4, 0x3, 0x3, // e0..ef
    0xb, 0x6, 0x6, 0x6, 0x5, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,
    0x8, 0x8, 0x8, 0x8, 0x8 // f0..ff
};

static const uint8_t shifted_utf8d_transition[] = {
    0x0,  0x10, 0x20, 0x30, 0x50, 0x80, 0x70, 0x10, 0x10, 0x10, 0x40, 0x60,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0,  0x10, 0x10,
    0x10, 0x10, 0x10, 0x0,  0x10, 0x0,  0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x20, 0x10, 0x10, 0x10, 0x10, 0x10, 0x20, 0x10, 0x20, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x20,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x20, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x20, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x30, 0x10, 0x30, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x30,
    0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x30, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10
};

inline static uint32_t shiftless_updatestate(uint32_t *state, uint32_t byte) {
  uint32_t type = utf8d[byte];
  *state = shifted_utf8d_transition[*state + type];
  return *state;
}

/* shiftless_validate_dfa_utf8_branchless */
bool utf8_check(const char *src, size_t len) {
    uint32_t byteval;
    const unsigned char *cu = (const unsigned char *)src;
    uint32_t state = 0;

    for (size_t i = 0; i < len; i++) {
        byteval = (uint32_t)cu[i];
        shiftless_updatestate(&state, byteval);
    }

    byteval = (uint32_t)'\0';
    shiftless_updatestate(&state, byteval);

    return state != 16;
}

#endif
