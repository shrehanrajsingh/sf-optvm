#if !defined(FUN_H)
#define FUN_H

#include "expr.h"
#include "header.h"

struct object_s;
struct __module_s;

enum FunType
{
  FUN_NATIVE,
  FUN_CODED
};

enum NFType
{
  NF_ARG_1,
  NF_ARG_2,
  NF_ARG_3,
  NF_ARG_ANY,
};

typedef struct
{
  char *name;
  int type;

  char **args;
  size_t argl;
  size_t argc;

  union
  {
    struct
    {
      int nf_type;
      int scc; /* sys code call */

      union
      {
        struct object_s *(*f_onearg) (struct object_s *);
        struct object_s *(*f_twoarg) (struct object_s *, struct object_s *);
        struct object_s *(*f_threearg) (struct object_s *, struct object_s *,
                                        struct object_s *);

        struct object_s *(*f_anyarg) (struct object_s **, size_t);
      } v;

    } native;

    struct
    {
      size_t lp;

    } coded;

  } v;

} fun_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API fun_t *sf_fun_new (int);
  SF_API void sf_fun_free (fun_t *);
  SF_API void sf_fun_addarg (fun_t *, char *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // FUN_H
