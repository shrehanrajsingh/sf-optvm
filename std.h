#if !defined(STD_H)
#define STD_H

#include "header.h"
#include "malloc.h"

#define SF_STACK_CAP 64

struct __std_str
{
  size_t *lis; /* lines */
  size_t *lof; /* offsets */
  size_t lc;
  size_t ll;
};

struct __l_o_pair_str
{
  size_t l;
  size_t o;
};

typedef struct __std_str std_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API std_t sf_std_new ();
  SF_API void sf_std_free (std_t *);

  SF_API void sf_std_push (std_t *, size_t, size_t);
  SF_API struct __l_o_pair_str sf_std_pop (std_t *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // STD_H
