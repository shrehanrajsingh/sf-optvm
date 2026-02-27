#if !defined(ARRAY_H)
#define ARRAY_H

#include "header.h"
#include "malloc.h"

struct object_s;
typedef struct __array_s
{
  struct object_s **vals;
  size_t len;

} array_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API array_t *sf_array_new ();
  SF_API array_t *sf_array_withsize (size_t);
  SF_API void sf_array_free (array_t *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // ARRAY_H
