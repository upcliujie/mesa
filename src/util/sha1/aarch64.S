/*
 * Copyright © 2020 Emmanuel Gil Peyrot
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Based on RustCrypto/asm-hashes sha1/src/aarch64.S
 */

/* void SHA1TransformAArch64(uint32_t state[5], const uint8_t block[64]) */
.global	SHA1TransformAArch64
.type	SHA1TransformAArch64, %function
.arch	armv8-a+crypto
SHA1TransformAArch64:
	/*
	 * Storage usage:
	 *   Bytes  Location  Description
	 *       4  x0        state argument
	 *       4  x1        block argument
	 *      16  q0        W0
	 *      16  q1        W1
	 *      16  q2        W2
	 *      16  q3        W3
	 *      16  q4        k
	 *      16  q5        Original ABCD
	 *      16  q6        ABCD (with s3 being A)
	 *       4  s16       E
	 *       4  s17       e0
	 *       4  s18       e1
	 *      16  q19       wk
	 */

	// Load state in registers
	ldr	q5, [x0]
	ldr	s16, [x0, 16]
	mov	v6.16b, v5.16b

	// Load block in registers
	ldr	q0, [x1]
	ldr	q1, [x1, 16]
	ldr	q2, [x1, 32]
	ldr	q3, [x1, 48]

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	rev32	v0.16b, v0.16b
	rev32	v1.16b, v1.16b
	rev32	v2.16b, v2.16b
	rev32	v3.16b, v3.16b
#endif

	// k for the next five rounds
	adrp	x1, .K0
	ldr	q4, [x1, #:lo12:.K0]

	// 0
	sha1h	s18, s6
	add	v19.4s, v0.4s, v4.4s
	sha1c	q6, s16, v19.4s
	sha1su0	v0.4s, v1.4s, v2.4s

	// 1
	sha1h	s17, s6
	add	v19.4s, v1.4s, v4.4s
	sha1c	q6, s18, v19.4s
	sha1su1	v0.4s, v3.4s
	sha1su0	v1.4s, v2.4s, v3.4s

	// 2
	sha1h	s18, s6
	add	v19.4s, v2.4s, v4.4s
	sha1c	q6, s17, v19.4s
	sha1su1	v1.4s, v0.4s
	sha1su0	v2.4s, v3.4s, v0.4s

	// 3
	sha1h	s17, s6
	add	v19.4s, v3.4s, v4.4s
	sha1c	q6, s18, v19.4s
	sha1su1	v2.4s, v1.4s
	sha1su0	v3.4s, v0.4s, v1.4s

	// 4
	sha1h	s18, s6
	add	v19.4s, v0.4s, v4.4s
	sha1c	q6, s17, v19.4s
	sha1su1	v3.4s, v2.4s
	sha1su0	v0.4s, v1.4s, v2.4s

	// k for the next five rounds
	adrp	x1, .K1
	ldr	q4, [x1, #:lo12:.K1]

	// 5
	sha1h	s17, s6
	add	v19.4s, v1.4s, v4.4s
	sha1p	q6, s18, v19.4s
	sha1su1	v0.4s, v3.4s
	sha1su0	v1.4s, v2.4s, v3.4s

	// 6
	sha1h	s18, s6
	add	v19.4s, v2.4s, v4.4s
	sha1p	q6, s17, v19.4s
	sha1su1	v1.4s, v0.4s
	sha1su0	v2.4s, v3.4s, v0.4s

	// 7
	sha1h	s17, s6
	add	v19.4s, v3.4s, v4.4s
	sha1p	q6, s18, v19.4s
	sha1su1	v2.4s, v1.4s
	sha1su0	v3.4s, v0.4s, v1.4s

	// 8
	sha1h	s18, s6
	add	v19.4s, v0.4s, v4.4s
	sha1p	q6, s17, v19.4s
	sha1su1	v3.4s, v2.4s
	sha1su0	v0.4s, v1.4s, v2.4s

	// 9
	sha1h	s17, s6
	add	v19.4s, v1.4s, v4.4s
	sha1p	q6, s18, v19.4s
	sha1su1	v0.4s, v3.4s
	sha1su0	v1.4s, v2.4s, v3.4s

	// k for the next five rounds
	adrp	x1, .K2
	ldr	q4, [x1, #:lo12:.K2]

	// 10
	sha1h	s18, s6
	add	v19.4s, v2.4s, v4.4s
	sha1m	q6, s17, v19.4s
	sha1su1	v1.4s, v0.4s
	sha1su0	v2.4s, v3.4s, v0.4s

	// 11
	sha1h	s17, s6
	add	v19.4s, v3.4s, v4.4s
	sha1m	q6, s18, v19.4s
	sha1su1	v2.4s, v1.4s
	sha1su0	v3.4s, v0.4s, v1.4s

	// 12
	sha1h	s18, s6
	add	v19.4s, v0.4s, v4.4s
	sha1m	q6, s17, v19.4s
	sha1su1	v3.4s, v2.4s
	sha1su0	v0.4s, v1.4s, v2.4s

	// 13
	sha1h	s17, s6
	add	v19.4s, v1.4s, v4.4s
	sha1m	q6, s18, v19.4s
	sha1su1	v0.4s, v3.4s
	sha1su0	v1.4s, v2.4s, v3.4s

	// 14
	sha1h	s18, s6
	add	v19.4s, v2.4s, v4.4s
	sha1m	q6, s17, v19.4s
	sha1su1	v1.4s, v0.4s
	sha1su0	v2.4s, v3.4s, v0.4s

	// k for the next five rounds
	adrp	x1, .K3
	ldr	q4, [x1, #:lo12:.K3]

	// 15
	sha1h	s17, s6
	add	v19.4s, v3.4s, v4.4s
	sha1p	q6, s18, v19.4s
	sha1su1	v2.4s, v1.4s
	sha1su0	v3.4s, v0.4s, v1.4s

	// 16
	sha1h	s18, s6
	add	v19.4s, v0.4s, v4.4s
	sha1p	q6, s17, v19.4s
	sha1su1	v3.4s, v2.4s

	// 17
	sha1h	s17, s6
	add	v19.4s, v1.4s, v4.4s
	sha1p	q6, s18, v19.4s

	// 18
	sha1h	s18, s6
	add	v19.4s, v2.4s, v4.4s
	sha1p	q6, s17, v19.4s

	// 19
	sha1h	s17, s6
	add	v19.4s, v3.4s, v4.4s
	sha1p	q6, s18, v19.4s

	// Update state
	add	v6.4s, v6.4s, v5.4s
	str	q6, [x0]
	add	v16.2s, v16.2s, v17.2s
	str	s16, [x0, 16]

	ret
.size	SHA1TransformAArch64, .-SHA1TransformAArch64
.align	4
.K0:
	.word	0x5A827999
	.word	0x5A827999
	.word	0x5A827999
	.word	0x5A827999
.K1:
	.word	0x6ED9EBA1
	.word	0x6ED9EBA1
	.word	0x6ED9EBA1
	.word	0x6ED9EBA1
.K2:
	.word	0x8F1BBCDC
	.word	0x8F1BBCDC
	.word	0x8F1BBCDC
	.word	0x8F1BBCDC
.K3:
	.word	0xCA62C1D6
	.word	0xCA62C1D6
	.word	0xCA62C1D6
	.word	0xCA62C1D6
