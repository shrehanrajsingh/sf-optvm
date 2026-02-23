#include "cl.h"

SF_API class_t *
sf_class_new ()
{
  class_t *c = SFMALLOC (sizeof (*c));
  c->name = NULL;
  c->slots = NULL;
  c->vals = NULL;
  c->svl = 0;
  c->svc = 0;

  return c;
}

SF_API cobj_t *
sf_cobj_new (class_t *p)
{
  cobj_t *c = SFMALLOC (sizeof (*c));
  c->p = p;
  c->slots = NULL;
  c->vals = NULL;
  c->svc = 0;
  c->svl = 0;

  return c;
}

SF_API void
sf_cobj_free (cobj_t *c)
{
  if (c->slots != NULL)
    SFFREE (c->slots);

  if (c->vals != NULL)
    SFFREE (c->vals);

  SFFREE (c);
}