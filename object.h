#if !defined(OBJECT_H)
#define OBJECT_H

#include "cl.h"
#include "const.h"
#include "fun.h"
#include "header.h"
#include "malloc.h"
#include "mut.h"

enum ObjectType
{
  OBJ_CONST,
  OBJ_FUNC,
  OBJ_CLASS,
  OBJ_COBJ,
  OBJ_HFF, /* half function */
};

typedef struct object_s
{
  int type;

  union
  {
    struct
    {
      const_t v;

    } o_const;

    struct
    {
      fun_t *v;

    } o_fun;

    struct
    {
      class_t *v;

    } o_class;

    struct
    {
      cobj_t *v;

    } o_cobj;

    struct
    {
      struct object_s *f;
      struct object_s **args;
      size_t al;

    } o_hff;

  } v;

  struct
  {
    atomic_int ref_count;
    int active;
    int64_t index;

  } meta;

} obj_t;

#define IR(X)                                                                 \
  {                                                                           \
    sf_obj_rc_inc ((X));                                                      \
  }

#define DR(X)                                                                 \
  {                                                                           \
    sf_obj_rc_dec ((X));                                                      \
  }

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API void sf_objstore_init ();
  SF_API obj_t *sf_objstore_req ();
  SF_API obj_t *sf_objstore_req_forconst (const_t *);

  SF_API obj_t sf_objnew (int);
  SF_API void sf_obj_rc_inc (obj_t *);
  SF_API void sf_obj_rc_dec (obj_t *);
  SF_API void sf_obj_free (obj_t *);
  SF_API void sf_obj_print (obj_t);
  SF_API obj_t **sf_get_objstore ();

  SF_API int sf_obj_isfalse (obj_t);

  SF_API int sf_obj_eqeq (obj_t *, obj_t *);
  SF_API int sf_obj_neq (obj_t *, obj_t *);
  SF_API int sf_obj_le (obj_t *, obj_t *);
  SF_API int sf_obj_ge (obj_t *, obj_t *);
  SF_API int sf_obj_leq (obj_t *, obj_t *);
  SF_API int sf_obj_geq (obj_t *, obj_t *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // OBJECT_H
