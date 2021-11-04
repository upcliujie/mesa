/*
 * Copyright © 2021 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <gtest/gtest.h>

static testing::AssertionResult
bytes_equal_pred(const char *a_expr,
                 const char *b_expr,
                 const char *c_expr,
                 const uint8_t *a,
                 const uint8_t *b,
                 size_t num_bytes)
{
   if (memcmp(a, b, num_bytes)) {
      std::stringstream result;

      unsigned mismatches = 0;
      for (size_t i = 0; i < num_bytes; i++) {
         if (a[i] != b[i])
            mismatches++;
      }

      result << "Expected " << num_bytes << " bytes to be equal but found "
             << mismatches << " bytes that differ:\n\n";

      result << std::right << std::hex << std::setfill('0') << std::uppercase;

      result << "    " << a_expr << " bytes are:\n";
      for (size_t i = 0; i < num_bytes; i++) {
         if (i % 16 == 0)
            result << "\n[" << std::setw(3) << i << "]";
         unsigned v = a[i];
         result << " "
                << (a[i] == b[i] ? ' ' : '*')
                << std::setw(2) << v;
      }
      result << "\n\n";

      result << "    " << b_expr << " bytes are:\n";
      for (size_t i = 0; i < num_bytes; i++) {
         if (i % 16 == 0)
            result << "\n[" << std::setw(3) << i << "]";
         unsigned v = b[i];
         result << " "
                << (a[i] == b[i] ? ' ' : '*')
                << std::setw(2) << v;
      }
      result << "\n";

      return testing::AssertionFailure() << result.str();
   } else {
      return testing::AssertionSuccess();
   }
}

#define EXPECT_BYTES_EQUAL(a, b, num_bytes) \
   EXPECT_PRED_FORMAT3(bytes_equal_pred, a, b, num_bytes)

#define ASSERT_BYTES_EQUAL(a, b, num_bytes) \
   ASSERT_PRED_FORMAT3(bytes_equal_pred, a, b, num_bytes)
