#if !defined(CL_H)
#define CL_H

#include "header.h"
#include "malloc.h"

struct object_s;

/**
 * Overview of how classes work in Sunflower
 * A class created is a class_t in memory
 * It has a name, variable slots, values corresponding
 * to those names, and (in future) might support inheritance
 * When a new class object is created, the newly created object
 * is a cobj_t that stores a reference to class and has a
 * separate dictionary for storing names
 */
typedef struct __class_s
{
  char *name;

  char **slots;
  struct object_s **vals;
  size_t svl;
  size_t svc;

} class_t;

typedef struct __cobj_s
{
  class_t *p;

  char **slots;
  struct object_s **vals;
  size_t svl;
  size_t svc;

} cobj_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API class_t *sf_class_new ();
  SF_API cobj_t *sf_cobj_new (class_t *);
  SF_API void sf_cobj_free (cobj_t *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // CL_H
