#if !defined(CL_H)
#define CL_H

#include "header.h"
#include "malloc.h"

struct object_s;
typedef struct __class_s
{
  char *name;

  char **slots;
  struct object_s **vals;
  size_t svl;
  size_t svc;

} class_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API class_t *sf_class_new ();

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // CL_H
