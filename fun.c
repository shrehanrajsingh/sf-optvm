#include "fun.h"

SF_API fun_t *
sf_fun_new (int type)
{
  fun_t *f = SFMALLOC (sizeof (*f));
  f->type = type;
  f->argl = 0;
  f->argc = 4;
  f->args = SFMALLOC (f->argc * sizeof (*f->args));

  if (f->type == FUN_NATIVE)
    {
      f->v.native.scc = 0;
    }

  return f;
}

SF_API void
sf_fun_addarg (fun_t *f, char *n)
{
  if (f->argl >= f->argc)
    {
      f->argc += 4;
      f->args = SFREALLOC (f->args, f->argc * sizeof (*f->args));
    }

  f->args[f->argl++] = SFSTRDUP (n);
}

SF_API void
sf_fun_free (fun_t *f)
{
  for (int i = 0; i < f->argc; i++)
    SFFREE (f->args[i]);

  SFFREE (f->args);
  SFFREE (f);
}