#include "std.h"

SF_API std_t
sf_std_new ()
{
  std_t s;
  s.ll = 0;
  s.lc = SF_STACK_CAP;
  s.lis = SFMALLOC (s.lc * sizeof (*s.lis));
  s.lof = SFMALLOC (s.lc * sizeof (*s.lof));

  return s;
}

SF_API void
sf_std_free (std_t *s)
{
  SFFREE (s->lis);
}

SF_API void
sf_std_push (std_t *s, size_t l, size_t o)
{
  if (s->ll >= s->lc)
    {
      s->lc += SF_STACK_CAP;
      s->lis = SFREALLOC (s->lis, s->lc * sizeof (*s->lis));
      s->lof = SFREALLOC (s->lof, s->lc * sizeof (*s->lof));
    }

  s->lis[s->ll] = l;
  s->lof[s->ll++] = o;
}

SF_API struct __l_o_pair_str
sf_std_pop (std_t *s)
{
  if (s->ll)
    {
      s->ll--;
      return (struct __l_o_pair_str){
        .l = s->lis[s->ll],
        .o = s->lof[s->ll],
      };
    }

  return (struct __l_o_pair_str){ .l = 0, .o = 0 };
}