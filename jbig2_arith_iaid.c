/* Annex A.3 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "config_types.h"
#elif _WIN32
#include "config_win32.h"
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <stddef.h>
#include <string.h> /* memset() */

#ifdef VERBOSE
#include <stdio.h> /* for debug printing only */
#endif

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_arith_iaid.h"

struct _Jbig2ArithIaidCtx {
  int SBSYMCODELEN;
  Jbig2ArithCx *IAIDx;
};

Jbig2ArithIaidCtx *
jbig2_arith_iaid_ctx_new(Jbig2Ctx *ctx, int SBSYMCODELEN)
{
  Jbig2ArithIaidCtx *result = jbig2_new(ctx, Jbig2ArithIaidCtx, 1);
  int ctx_size = 1 << SBSYMCODELEN;

  result->SBSYMCODELEN = SBSYMCODELEN;
  result->IAIDx = jbig2_alloc(ctx->allocator, ctx_size);
  memset(result->IAIDx, 0, ctx_size);

  return result;
}

/* A.3 */
/* Return value: -1 on error, 0 on normal value */
int
jbig2_arith_iaid_decode(Jbig2ArithIaidCtx *ctx, Jbig2ArithState *as,
		       int32_t *p_result)
{
  Jbig2ArithCx *IAIDx = ctx->IAIDx;
  int SBSYMCODELEN = ctx->SBSYMCODELEN;
  int PREV = 1;
  int D;
  int i;

  /* A.3 (2) */
  for (i = 0; i < SBSYMCODELEN; i++)
    {
      D = jbig2_arith_decode(as, &IAIDx[PREV]);
#ifdef VERBOSE
      fprintf(stderr, "IAID%x: D = %d\n", PREV, D);
#endif
      PREV = (PREV << 1) | D;
    }
  /* A.3 (3) */
  PREV -= 1 << SBSYMCODELEN;
#ifdef VERBOSE
  fprintf(stderr, "IAID result: %d\n", PREV);
#endif
  *p_result = PREV;
  return 0;
}

void
jbig2_arith_iaid_ctx_free(Jbig2Ctx *ctx, Jbig2ArithIaidCtx *iax)
{
  jbig2_free(ctx->allocator, iax->IAIDx);
  jbig2_free(ctx->allocator, iax);
}
