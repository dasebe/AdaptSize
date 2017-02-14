#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "vcl.h"
#include "vrt.h"
#include "cache/cache.h"

#include "vcc_if.h"

int
init_function(const struct vrt_ctx *ctx, struct vmod_priv *priv,
	      enum vcl_event_e e)
{
  if (e != VCL_EVENT_LOAD)
    return (0);

  /* init what you need */
  return (0);
}

VCL_BOOL
vmod_thlru(VRT_CTX, VCL_STRING p)
{
  char *e;
  long r;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);

  if (p == NULL)
    return (1);

  while(isspace(*p))
    p++;

  e = NULL;
  r = strtol(p, &e, 0);
  if (e == NULL || *e != '\0')
    return (1);
  return (r <= cache_param->opt_threshold);
}

VCL_BOOL
vmod_explru(VRT_CTX, VCL_STRING p)
{
  char *e;
  long r;

  CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);

  if (p == NULL)
    return (1);

  while(isspace(*p))
    p++;

  e = NULL;
  r = strtol(p, &e, 0);
  if (e == NULL || *e != '\0')
    return (1);

  const double urand = drand48();
  const double admissionprob = exp(-r/ cache_param->opt_threshold);
  return (admissionprob >= urand);
}
