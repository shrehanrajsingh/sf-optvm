#if !defined(MOD_H)
#define MOD_H

#include "header.h"
#include "malloc.h"

struct _frame_s;
typedef struct __mod_s
{
  char *name;

  char **slots;
  struct object_s **vals;
  size_t svl;
  size_t svc;
  struct _frame_s *fr;

} mod_t;

typedef struct __modstore_s
{
  int *keys;
  struct object_s **vals;
  size_t kl;
  size_t kc;

} modstore_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API mod_t *sf_mod_new ();
  SF_API void sf_mod_free (mod_t *);

  SF_API modstore_t *sf_modstore_new ();
  SF_API void sf_modstore_free (modstore_t *);
  SF_API int sf_modstore_haskey (modstore_t *, int);
  SF_API struct object_s *sf_modstore_get (modstore_t *, int);
  SF_API void sf_modstore_add (modstore_t *, int, struct object_s *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // MOD_H
