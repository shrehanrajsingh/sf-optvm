#include "iter.h"
#include "object.h"

SF_API iter_t
sf_iter_new (struct object_s *o)
{
  iter_t i;
  i.o = o;
  i.meta.next_idx = 0;

  return i;
}

SF_API struct object_s *
sf_iter_next (iter_t *i)
{
  switch (i->o->type)
    {
    case OBJ_ARRAY:
      {
        array_t *a = i->o->v.o_array.v;

        if (a->len <= i->meta.next_idx)
          {
            return NULL;
          }

        return a->vals[i->meta.next_idx++];
      }
      break;

    default:
      break;
    }

  return NULL;
}