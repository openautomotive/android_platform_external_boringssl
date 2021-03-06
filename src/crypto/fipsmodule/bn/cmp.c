/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

#include <openssl/bn.h>

#include <openssl/mem.h>
#include <openssl/type_check.h>

#include "internal.h"
#include "../../internal.h"


int BN_ucmp(const BIGNUM *a, const BIGNUM *b) {
  int a_width = bn_minimal_width(a);
  int b_width = bn_minimal_width(b);
  int i = a_width - b_width;
  if (i != 0) {
    return i;
  }

  const BN_ULONG *ap = a->d;
  const BN_ULONG *bp = b->d;
  for (i = a_width - 1; i >= 0; i--) {
    BN_ULONG t1 = ap[i];
    BN_ULONG t2 = bp[i];
    if (t1 != t2) {
      return (t1 > t2) ? 1 : -1;
    }
  }

  return 0;
}

int BN_cmp(const BIGNUM *a, const BIGNUM *b) {
  int i;
  int gt, lt;
  BN_ULONG t1, t2;

  if ((a == NULL) || (b == NULL)) {
    if (a != NULL) {
      return -1;
    } else if (b != NULL) {
      return 1;
    } else {
      return 0;
    }
  }

  if (a->neg != b->neg) {
    if (a->neg) {
      return -1;
    }
    return 1;
  }
  if (a->neg == 0) {
    gt = 1;
    lt = -1;
  } else {
    gt = -1;
    lt = 1;
  }

  int a_width = bn_minimal_width(a);
  int b_width = bn_minimal_width(b);
  if (a_width > b_width) {
    return gt;
  }
  if (a_width < b_width) {
    return lt;
  }

  for (i = a_width - 1; i >= 0; i--) {
    t1 = a->d[i];
    t2 = b->d[i];
    if (t1 > t2) {
      return gt;
    } if (t1 < t2) {
      return lt;
    }
  }

  return 0;
}

int bn_cmp_words(const BN_ULONG *a, const BN_ULONG *b, int n) {
  int i;
  BN_ULONG aa, bb;

  aa = a[n - 1];
  bb = b[n - 1];
  if (aa != bb) {
    return (aa > bb) ? 1 : -1;
  }

  for (i = n - 2; i >= 0; i--) {
    aa = a[i];
    bb = b[i];
    if (aa != bb) {
      return (aa > bb) ? 1 : -1;
    }
  }
  return 0;
}

int bn_cmp_part_words(const BN_ULONG *a, const BN_ULONG *b, int cl, int dl) {
  int n, i;
  n = cl - 1;

  if (dl < 0) {
    for (i = dl; i < 0; i++) {
      if (b[n - i] != 0) {
        return -1;  // a < b
      }
    }
  }
  if (dl > 0) {
    for (i = dl; i > 0; i--) {
      if (a[n + i] != 0) {
        return 1;  // a > b
      }
    }
  }

  return bn_cmp_words(a, b, cl);
}

static int bn_less_than_words_impl(const BN_ULONG *a, size_t a_len,
                                   const BN_ULONG *b, size_t b_len) {
  OPENSSL_COMPILE_ASSERT(sizeof(BN_ULONG) <= sizeof(crypto_word_t),
                         crypto_word_t_too_small);
  int ret = 0;
  // Process the common words in little-endian order.
  size_t min = a_len < b_len ? a_len : b_len;
  for (size_t i = 0; i < min; i++) {
    crypto_word_t eq = constant_time_eq_w(a[i], b[i]);
    crypto_word_t lt = constant_time_lt_w(a[i], b[i]);
    ret = constant_time_select_int(eq, ret, constant_time_select_int(lt, 1, 0));
  }

  // If |a| or |b| has non-zero words beyond |min|, they take precedence.
  if (a_len < b_len) {
    crypto_word_t mask = 0;
    for (size_t i = a_len; i < b_len; i++) {
      mask |= b[i];
    }
    ret = constant_time_select_int(constant_time_is_zero_w(mask), ret, 1);
  } else if (b_len < a_len) {
    crypto_word_t mask = 0;
    for (size_t i = b_len; i < a_len; i++) {
      mask |= a[i];
    }
    ret = constant_time_select_int(constant_time_is_zero_w(mask), ret, 0);
  }

  return ret;
}

int bn_less_than_words(const BN_ULONG *a, const BN_ULONG *b, size_t len) {
  return bn_less_than_words_impl(a, len, b, len);
}

int BN_abs_is_word(const BIGNUM *bn, BN_ULONG w) {
  switch (bn_minimal_width(bn)) {
    case 1:
      return bn->d[0] == w;
    case 0:
      return w == 0;
    default:
      return 0;
  }
}

int BN_cmp_word(const BIGNUM *a, BN_ULONG b) {
  BIGNUM b_bn;
  BN_init(&b_bn);

  b_bn.d = &b;
  b_bn.top = b > 0;
  b_bn.dmax = 1;
  b_bn.flags = BN_FLG_STATIC_DATA;
  return BN_cmp(a, &b_bn);
}

int BN_is_zero(const BIGNUM *bn) {
  return bn_minimal_width(bn) == 0;
}

int BN_is_one(const BIGNUM *bn) {
  return bn->neg == 0 && BN_abs_is_word(bn, 1);
}

int BN_is_word(const BIGNUM *bn, BN_ULONG w) {
  return BN_abs_is_word(bn, w) && (w == 0 || bn->neg == 0);
}

int BN_is_odd(const BIGNUM *bn) {
  return bn->top > 0 && (bn->d[0] & 1) == 1;
}

int BN_is_pow2(const BIGNUM *bn) {
  int width = bn_minimal_width(bn);
  if (width == 0 || bn->neg) {
    return 0;
  }

  for (int i = 0; i < width - 1; i++) {
    if (bn->d[i] != 0) {
      return 0;
    }
  }

  return 0 == (bn->d[width-1] & (bn->d[width-1] - 1));
}

int BN_equal_consttime(const BIGNUM *a, const BIGNUM *b) {
  BN_ULONG mask = 0;
  // If |a| or |b| has more words than the other, all those words must be zero.
  for (int i = a->top; i < b->top; i++) {
    mask |= b->d[i];
  }
  for (int i = b->top; i < a->top; i++) {
    mask |= a->d[i];
  }
  // Common words must match.
  int min = a->top < b->top ? a->top : b->top;
  for (int i = 0; i < min; i++) {
    mask |= (a->d[i] ^ b->d[i]);
  }
  // The sign bit must match.
  mask |= (a->neg ^ b->neg);
  return mask == 0;
}

int BN_less_than_consttime(const BIGNUM *a, const BIGNUM *b) {
  // We do not attempt to process the sign bit in constant time. Negative
  // |BIGNUM|s should never occur in crypto, only calculators.
  if (a->neg && !b->neg) {
    return 1;
  }
  if (b->neg && !a->neg) {
    return 0;
  }
  if (a->neg && b->neg) {
    const BIGNUM *tmp = a;
    a = b;
    b = tmp;
  }
  return bn_less_than_words_impl(a->d, a->top, b->d, b->top);
}
