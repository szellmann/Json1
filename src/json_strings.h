// Copyright 2018 Alexander Bolz
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "json_options.h"
#include "json_unicode.h"

#include <cassert>
#include <cstdint>
#include <iterator>

namespace json {
namespace strings {

//==================================================================================================
//
//==================================================================================================

//
//  string
//      ""
//      " chars "
//  chars
//      char
//      char chars
//  char
//      <any Unicode character except " or \ or control-character>
//      '\"'
//      '\\'
//      '\/'
//      '\b'
//      '\f'
//      '\n'
//      '\r'
//      '\t'
//      '\u' four-hex-digits
//

//enum class UnescapeStringStatus {
//    success,
//    unescaped_control_character,
//    incomplete_ucn_sequence,
//    incomplete_utf8_sequence,
//    invalid_unicode,
//};

template <typename It>
struct UnescapeStringResult
{
    bool ok;
    It next;
};

template <typename It, typename Fn>
UnescapeStringResult<It> UnescapeString(
    It             next,
    It             last,
    //char           quote_char,
    Options const& options,
    Fn             yield)
{
    while (next != last)
    {
        //if (*next == quote_char)
        //{
        //    ++next;
        //    break;
        //}

        auto const uc = static_cast<unsigned char>(*next);

        if (uc < 0x20) // unescaped control character
        {
            if (!options.allow_unescaped_control_characters)
                return {false, next};

            yield(*next);
            ++next;
        }
        else if (uc < 0x80) // ASCII printable or DEL
        {
            if (*next != '\\')
            {
                yield(*next);
                ++next;
                continue;
            }

            auto const f = next; // The start of the UCN sequence

            ++next; // skip '\'
            if (next == last)
                return {false, next};

            switch (*next)
            {
            case '"':
                yield('"');
                ++next;
                break;
            case '\\':
                yield('\\');
                ++next;
                break;
            case '/':
                yield('/');
                ++next;
                break;
            case 'b':
                yield('\b');
                ++next;
                break;
            case 'f':
                yield('\f');
                ++next;
                break;
            case 'n':
                yield('\n');
                ++next;
                break;
            case 'r':
                yield('\r');
                ++next;
                break;
            case 't':
                yield('\t');
                ++next;
                break;
            case 'u':
                {
                    if (++next == last)
                        return {false, f};

                    uint32_t U = 0;
                    auto const end = json::unicode::DecodeTrimmedUCNSequence(next, last, U);
                    //assert(end != next);
                    next = end;

                    if (U != json::unicode::kInvalidCodepoint)
                    {
                        json::unicode::EncodeUTF8(U, [&](uint8_t code_unit) { yield(static_cast<char>(code_unit)); });
                    }
                    else
                    {
                        if (!options.allow_invalid_unicode)
                            return {false, f};

                        yield('\xEF');
                        yield('\xBF');
                        yield('\xBD');
                    }
                }
                break;
            default:
                return {false, next}; // invalid escaped character
            }
        }
        else // (possibly) the start of a UTF-8 sequence.
        {
            auto f = next; // The start of the UTF-8 sequence

            uint32_t U = 0;
            next = json::unicode::DecodeUTF8Sequence(next, last, U);
            //assert(next != f);

            if (U != json::unicode::kInvalidCodepoint)
            {
                //if constexpr (std::is_same<std::input_iterator_tag, typename std::iterator_traits<It>::iterator_category>::value)
                //{
                //    json::unicode::EncodeUTF8(U, [&](uint8_t code_unit) { yield(static_cast<char>(code_unit)); });
                //}
                //else
                //{
                    // The range [f, next) already contains a valid UTF-8 encoding of U.
                    for ( ; f != next; ++f) {
                        yield(*f);
                    }
                //}
            }
            else
            {
                if (!options.allow_invalid_unicode)
                    return {false, f};

                yield('\xEF');
                yield('\xBF');
                yield('\xBD');
            }
        }
    }

    return {true, next};
}

template <typename It>
struct EscapeStringResult
{
    bool ok;
    It next;
};

template <typename It, typename Fn>
EscapeStringResult<It> EscapeString(
    It             next,
    It             last,
    Options const& options,
    Fn             yield)
{
    static constexpr char const kHexDigits[] = "0123456789ABCDEF";

    char ch_prev = '\0';
    char ch = '\0';

    while (next != last)
    {
        ch_prev = ch;
        ch = *next;

        auto const uc = static_cast<unsigned char>(ch);

        if (uc < 0x20) // (ASCII) control character
        {
            yield('\\');
            switch (ch)
            {
            case '\b':
                yield('b');
                break;
            case '\f':
                yield('f');
                break;
            case '\n':
                yield('n');
                break;
            case '\r':
                yield('r');
                break;
            case '\t':
                yield('t');
                break;
            default:
                yield('u');
                yield('0');
                yield('0');
                yield(kHexDigits[uc >> 4]);
                yield(kHexDigits[uc & 0xF]);
                break;
            }
            ++next;
        }
        else if (uc < 0x80) // ASCII printable or DEL
        {
            switch (ch)
            {
            case '"':   // U+0022
            case '\\':  // U+005C
                yield('\\');
                break;
            case '/':   // U+002F
                if (options.escape_slash && ch_prev == '<') {
                    yield('\\');
                }
                break;
            }
            yield(ch);
            ++next;
        }
        else // (possibly) the start of a UTF-8 sequence.
        {
            auto f = next; // The start of the UTF-8 sequence

            uint32_t U = 0;
            next = json::unicode::DecodeUTF8Sequence(next, last, U);
            //assert(next != f);

            if (U != json::unicode::kInvalidCodepoint)
            {
                //
                // Always escape U+2028 (LINE SEPARATOR) and U+2029 (PARAGRAPH
                // SEPARATOR). No string in JavaScript can contain a literal
                // U+2028 or a U+2029.
                //
                // (See http://timelessrepo.com/json-isnt-a-javascript-subset)
                //
                if (U == 0x2028)
                {
                    yield('\\');
                    yield('u');
                    yield('2');
                    yield('0');
                    yield('2');
                    yield('8');
                }
                else if (U == 0x2029)
                {
                    yield('\\');
                    yield('u');
                    yield('2');
                    yield('0');
                    yield('2');
                    yield('9');
                }
                else
                {
                    //if constexpr (std::is_same<std::input_iterator_tag, typename std::iterator_traits<It>::iterator_category>::value)
                    //{
                    //    json::unicode::EncodeUTF8(U, [&](uint8_t code_unit) { yield(static_cast<char>(code_unit)); });
                    //}
                    //else
                    //{
                        // The UTF-8 sequence is valid. No need to re-encode.
                        for ( ; f != next; ++f) {
                            yield(*f);
                        }
                    //}
                }
            }
            else
            {
                if (!options.allow_invalid_unicode)
                    return {false, next};

                yield('\\');
                yield('u');
                yield('F');
                yield('F');
                yield('F');
                yield('D');
            }
        }
    }

    return {true, next};
}

} // namespace strings
} // namespace json