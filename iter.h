#if !defined(ITER_H)
#define ITER_H

#include "header.h"
#include "malloc.h"

struct object_s;
typedef struct __iter_s
{
  struct object_s *o;

  struct
  {
    size_t next_idx; /* for arrays */

  } meta;

} iter_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API iter_t sf_iter_new (struct object_s *);
  SF_API struct object_s *
  sf_iter_next (iter_t *); /* return NULL to stop iteration */

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // ITER_H
