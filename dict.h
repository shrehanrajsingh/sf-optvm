#if !defined(DICT_H)
#define DICT_H

#include "header.h"
#include "malloc.h"

struct _vm_s;

struct object_s;
typedef struct __dict_s
{
  struct object_s **keys;
  struct object_s **vals;
  size_t len;

} dict_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API dict_t *sf_dict_new ();
  SF_API dict_t *sf_dict_withsize (size_t);
  SF_API void sf_dict_free (dict_t *, struct _vm_s *);

  SF_API void sf_dict_add (dict_t *, struct object_s *, struct object_s *,
                           struct _vm_s *);

  SF_API struct object_s *sf_dict_get (dict_t *, struct object_s *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // DICT_H
