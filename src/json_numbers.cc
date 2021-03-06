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

#include "json_numbers.h"

#include "json_charclass.h"

#define DTOA_OPTIMIZE_SIZE 1
#include "dtoa.h"

#define STRTOD_OPTIMIZE_SIZE 1
#include "strtod.h"

#include <cassert>
#include <climits>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>

#ifndef JSON_ASSERT
#define JSON_ASSERT(X) assert(X)
#endif

using namespace json;
using namespace json::numbers;

//==================================================================================================
// NumberToString
//==================================================================================================

static char* I64ToString(char* next, char* last, int64_t i)
{
    // Reuse PrintDecimalDigits from the dtoa implementation.
    // This routine assumes that n has at most 17 decimal digits.
    // We only use this here if n has at most 16 decimal digits.
    JSON_ASSERT(i > -10000000000000000);
    JSON_ASSERT(i <  10000000000000000);

    if (i < 0)
    {
        next[0] = '-';
        next++;
        i = -i;
    }

    int const len = base_conv::dtoa_impl::PrintDecimalDigits(next, last, static_cast<uint64_t>(i));
    return next + len;
}

char* json::numbers::NumberToString(char* next, char* last, double value, bool emit_trailing_dot_zero)
{
#if 1
    constexpr double kMinInteger = -9007199254740992.0; // -2^53
    constexpr double kMaxInteger =  9007199254740992.0; //  2^53

    // Print integers in the range [-2^53, +2^53] as integers (without a trailing ".0").
    // These integers are exactly representable as 'double's.
    // However, always print -0 as "-0.0" to increase compatibility with other libraries
    //
    // NB:
    // These tests for work correctly for NaN's and Infinity's (i.e. the DoubleToString branch is taken).
#if 1
    if (value == 0)
    {
        if (std::signbit(value))
        {
            std::memcpy(next, "-0.0", 4);
            return next + 4;
        }
        else
        {
            next[0] = '0';
            return next + 1;
        }
    }
    else if (kMinInteger <= value && value <= kMaxInteger)
    {
        const int64_t i = static_cast<int64_t>(value);
        if (static_cast<double>(i) == value)
        {
            return I64ToString(next, last, i);
        }
    }
#else
    if (emit_trailing_dot_zero && (kMinInteger <= value && value <= kMaxInteger))
    {
        emit_trailing_dot_zero = false;
    }
#endif
#endif

    return base_conv::Dtoa(next, last, value, emit_trailing_dot_zero, "NaN", "Infinity");
}

//==================================================================================================
// StringToNumber
//==================================================================================================

static bool StringToDouble(double& result, char const* next, char const* last, Options const& options)
{
#if 0
    if (next == last)
    {
        result = 0.0;
        return true;
    }

    return base_conv::Strtod(result, next, last) == base_conv::StrtodStatus::success;
#else
    using namespace json::charclass;

    // Inputs larger than kMaxInt (currently) can not be handled.
    // To avoid overflow in integer arithmetic.
    static constexpr int const kMaxInt = INT_MAX / 4;

    if (next == last)
    {
        result = 0.0; // [Recover.]
        return true;
    }

    if (last - next >= kMaxInt)
    {
        result = std::numeric_limits<double>::quiet_NaN();
        return false;
    }

    constexpr int kMaxSignificantDigits = base_conv::kMaxSignificantDigits;

    char digits[kMaxSignificantDigits];
    int  num_digits = 0;
    int  exponent = 0;
    bool nonzero_tail = false;

    bool is_neg = false;
    if (*next == '-')
    {
        is_neg = true;

        ++next;
        if (next == last)
        {
            result = is_neg ? -0.0 : +0.0; // Recover.
            return false;
        }
    }
#if 0
    else if (options.allow_leading_plus && *next == '+')
    {
        ++next;
        if (next == last)
        {
            result = is_neg ? -0.0 : +0.0; // Recover.
            return false;
        }
    }
#endif

    if (*next == '0')
    {
        ++next;
        if (next == last)
        {
            result = is_neg ? -0.0 : +0.0;
            return true;
        }
    }
    else if (IsDigit(*next))
    {
        // Copy significant digits of the integer part (if any) to the buffer.
        for (;;)
        {
            if (num_digits < kMaxSignificantDigits)
            {
                digits[num_digits++] = *next;
            }
            else
            {
                ++exponent;
                nonzero_tail = nonzero_tail || *next != '0';
            }
            ++next;
            if (next == last)
            {
                goto L_parsing_done;
            }
            if (!IsDigit(*next))
            {
                break;
            }
        }
    }
#if 0
    else if (options.allow_leading_dot && *next == '.')
    {
    }
#endif
    else
    {
        if (options.allow_nan_inf && last - next >= 3 && std::memcmp(next, "NaN", 3) == 0)
        {
            result = std::numeric_limits<double>::quiet_NaN();
            return true;
        }

        if (options.allow_nan_inf && last - next >= 8 && std::memcmp(next, "Infinity", 8) == 0)
        {
            result = is_neg ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
            return true;
        }

        return false;
    }

    if (*next == '.')
    {
        ++next;
        if (next == last)
        {
            // XXX: Recover? Continue with exponent?
            return false;
        }

        if (num_digits == 0)
        {
            // Integer part consists of 0 (or is absent).
            // Significant digits start after leading zeros (if any).
            while (*next == '0')
            {
                ++next;
                if (next == last)
                {
                    result = is_neg ? -0.0 : +0.0;
                    return true;
                }

                // Move this 0 into the exponent.
                --exponent;
            }
        }

        // There is a fractional part.
        // We don't emit a '.', but adjust the exponent instead.
        while (IsDigit(*next))
        {
            if (num_digits < kMaxSignificantDigits)
            {
                digits[num_digits++] = *next;
                --exponent;
            }
            else
            {
                nonzero_tail = nonzero_tail || *next != '0';
            }
            ++next;
            if (next == last)
            {
                goto L_parsing_done;
            }
        }
    }

    // Parse exponential part.
    if (*next == 'e' || *next == 'E')
    {
        ++next;
        if (next == last)
        {
            // XXX:
            // Recover? Parse as if exponent = 0?
            return false;
        }

        bool const exp_is_neg = (*next == '-');

        if (*next == '+' || exp_is_neg)
        {
            ++next;
            if (next == last)
            {
                // XXX:
                // Recover? Parse as if exponent = 0?
                return false;
            }
        }

        if (!IsDigit(*next))
        {
            // XXX:
            // Recover? Parse as if exponent = 0?
            return false;
        }

        int num = 0;
        for (;;)
        {
            int const digit = HexDigitValue(*next);

//          if (num > kMaxInt / 10 || digit > kMaxInt - 10 * num)
            if (num > kMaxInt / 10 - 9)
            {
                // Overflow.
                // Skip the rest of the exponent (ignored).
                for (++next; next != last && IsDigit(*next); ++next)
                {
                }
                num = kMaxInt;
                break;
            }

            num = num * 10 + digit;
            ++next;
            if (next == last)
            {
                break;
            }
            if (!IsDigit(*next))
            {
                break; // trailing junk
            }
        }

        exponent += exp_is_neg ? -num : num;
    }

L_parsing_done:
    if (next != last) // trailing garbage
    {
        return false;
    }

    double const value = base_conv::DecimalToDouble(digits, num_digits, exponent, nonzero_tail);
    JSON_ASSERT(!std::signbit(value));

    result = is_neg ? -value : value;
    return true;
#endif
}

inline double ReadDouble_unguarded(char const* digits, int num_digits)
{
    using namespace json::charclass;

    int64_t result = 0;

    for (int i = 0; i != num_digits; ++i)
    {
        JSON_ASSERT(IsDigit(digits[i]));
        result = 10 * result + HexDigitValue(digits[i]);
    }

    return static_cast<double>(result);
}

double json::numbers::StringToNumber(char const* first, char const* last, NumberClass nc)
{
    JSON_ASSERT(first != last);
    JSON_ASSERT(nc != NumberClass::invalid);

    if (first == last || last - first > INT_MAX) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (nc == NumberClass::nan) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (nc == NumberClass::pos_infinity) {
        return +std::numeric_limits<double>::infinity();
    }
    if (nc == NumberClass::neg_infinity) {
        return -std::numeric_limits<double>::infinity();
    }

    // Use a faster method for integers which will fit into a uint64_t.
    // Let static_cast do the conversion.
    if (nc == NumberClass::integer)
    {
        char const* first_digit = first;

        bool const is_neg = *first == '-';
        if (is_neg || *first == '+')
        {
            ++first_digit;
        }

        auto const num_digits = last - first_digit;
        if (num_digits <= 20)
        {
            auto const max_digits = num_digits <= 19 ? num_digits : 19;

            uint64_t const u = base_conv::strtod_impl::ReadInt<uint64_t>(first_digit, first_digit + max_digits);
            if (max_digits == num_digits)
            {
                return is_neg ? -static_cast<double>(u) : static_cast<double>(u);
            }

            // num_digits == 20.
            // Check if accumulating one digit more will still fit into a uint64_t.

            unsigned const next_digit = static_cast<unsigned>(first_digit[19]) - '0';
            if (u <= UINT64_MAX / 10 && next_digit <= UINT64_MAX - 10 * u)
            {
                uint64_t const u1 = 10 * u + next_digit;
                return is_neg ? -static_cast<double>(u1) : static_cast<double>(u1);
            }
        }
    }

    double result;
    if (StringToDouble(result, first, last, Options{}))
    {
        return result;
    }

    JSON_ASSERT(false && "unreachable");
    return std::numeric_limits<double>::quiet_NaN();
}

bool json::numbers::StringToNumber(double& result, char const* first, char const* last, Options const& options)
{
    if (StringToDouble(result, first, last, options))
    {
        return true;
    }

    result = std::numeric_limits<double>::quiet_NaN();
    return false;
}
