#include "mod.h"
#include "object.h"

SF_API mod_t *
sf_mod_new ()
{
  mod_t *m = SFMALLOC (sizeof (*m));
  m->svc = 0;
  m->svl = 0;
  m->name = NULL;
  m->slots = NULL;
  m->name = NULL;

  return m;
}

SF_API void
sf_mod_free (mod_t *mod)
{
  for (size_t i = 0; i < mod->svc; i++)
    if (mod->slots[i] != NULL)
      SFFREE (mod->slots[i]);

  if (mod->slots != NULL)
    SFFREE (mod->slots);

  if (mod->vals != NULL)
    SFFREE (mod->vals);

  SFFREE (mod);
}

SF_API modstore_t *
sf_modstore_new ()
{
  modstore_t *md = SFMALLOC (sizeof (*md));
  md->keys = NULL;
  md->vals = NULL;
  md->kl = 0;
  md->kc = 0;

  return md;
}

SF_API void
sf_modstore_free (modstore_t *md)
{
  if (md->keys != NULL)
    SFFREE (md->keys);

  if (md->vals != NULL)
    SFFREE (md->vals);

  SFFREE (md);
}

SF_API int
sf_modstore_haskey (modstore_t *md, int k)
{
  for (size_t i = 0; i < md->kl; i++)
    {
      if (k == md->keys[i])
        return 1;
    }

  return 0;
}

SF_API obj_t *
sf_modstore_get (modstore_t *md, int k)
{
  for (size_t i = 0; i < md->kl; i++)
    {
      if (k == md->keys[i])
        return md->vals[i];
    }

  return NULL;
}

SF_API void
sf_modstore_add (modstore_t *md, int k, struct object_s *v)
{
  if (md->kl >= md->kc)
    {
      md->kc += 8;
      md->keys = SFREALLOC (md->keys, md->kc * sizeof (*md->keys));
      md->vals = SFREALLOC (md->vals, md->kc * sizeof (*md->vals));
    }

  md->keys[md->kl] = k;
  md->vals[md->kl++] = v;
}