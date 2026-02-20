#include "cl.h"

SF_API class_t *
sf_class_new ()
{
  class_t *c = SFMALLOC (sizeof (*c));
  c->name = NULL;
  c->slots = NULL;
  c->vals = NULL;
  c->svl = 0;

  return c;
}