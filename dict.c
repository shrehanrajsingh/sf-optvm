#include "dict.h"
#include "bytecode.h"
#include "object.h"

SF_API dict_t *
sf_dict_new ()
{
  dict_t *d = SFMALLOC (sizeof (dict_t));
  d->keys = NULL;
  d->vals = NULL;
  d->len = 0;

  return d;
}

SF_API dict_t *
sf_dict_withsize (size_t s)
{
  dict_t *d = sf_dict_new ();
  d->len = s;
  d->keys = SFMALLOC (d->len * sizeof (*d->keys));
  d->vals = SFMALLOC (d->len * sizeof (*d->vals));

  return d;
}

SF_API void
sf_dict_free (dict_t *d, vm_t *vm)
{
  for (size_t i = 0; i < d->len; i++)
    {
      DR (d->keys[i], vm);
      DR (d->vals[i], vm);
    }

  if (d->keys != NULL)
    SFFREE (d->keys);

  if (d->vals != NULL)
    SFFREE (d->vals);
}

SF_API void
sf_dict_add (dict_t *d, obj_t *k, obj_t *v, vm_t *vm)
{
  size_t saw_idx = 0;
  int saw = 0;

  for (size_t i = 0; i < d->len; i++)
    {
      if (sf_obj_eqeq (d->keys[i], k))
        {
          saw = 1;
          saw_idx = i;
          break;
        }
    }

  /* no IR (k) and IR (v) */

  if (saw)
    {
      DR (d->vals[saw_idx], vm);
      d->keys[saw_idx] = k;
      d->vals[saw_idx] = v;
      return;
    }

  d->len++;
  d->keys = SFREALLOC (d->keys, d->len * sizeof (*d->keys));
  d->vals = SFREALLOC (d->vals, d->len * sizeof (*d->vals));

  d->keys[d->len - 1] = k;
  d->vals[d->len - 1] = v;
}

SF_API struct object_s *
sf_dict_get (dict_t *d, obj_t *k)
{
  for (size_t i = 0; i < d->len; i++)
    {
      if (sf_obj_eqeq (d->keys[i], k))
        return d->vals[i];
    }

  return NULL;
}