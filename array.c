#include "array.h"

SF_API array_t *
sf_array_new ()
{
  array_t *a = SFMALLOC (sizeof (*a));
  a->vals = NULL;
  a->len = 0;

  return a;
}

SF_API array_t *
sf_array_withsize (size_t s)
{
  array_t *a = SFMALLOC (sizeof (*a));
  a->vals = SFMALLOC (s * sizeof (*a->vals));
  a->len = s;

  return a;
}

SF_API void
sf_array_free (array_t *a)
{
  if (a->vals != NULL)
    SFFREE (a->vals);
  SFFREE (a);
}