#include "mod.h"

SF_API mod_t *
sf_mod_new (int type, StmtSM *smt)
{
  mod_t *m = SFMALLOC (sizeof (*m));
  m->type = type;
  m->body = smt;
  m->ht = sf_ht_new ();
  m->parent = NULL;

  return m;
}

SF_API void
sf_mod_free (mod_t *mod)
{
  sf_ht_free (mod->ht);
  SFFREE (mod);
}

SF_API void
sf_mod_addvar (mod_t *mod, const char *s, obj_t *v)
{
  int got = 0;
  obj_t *old = sf_mod_getvar (mod, s, &got);

  if (got)
    {
      DR (old);
      sf_ht_delete (mod->ht, s);
    }

  sf_ht_insert (mod->ht, s, v);
  IR (v);
}

SF_API obj_t *
sf_mod_getvar (mod_t *mod, const char *n, int *gr)
{
  // D (printf ("%s\n", n));
  int got = 0;
  void *r = sf_ht_get (mod->ht, n, &got);

  if (gr != NULL)
    *gr = got;

  if (got)
    return (obj_t *)r;

  return NULL;
}