#if !defined(EXPR_H)
#define EXPR_H

#include "const.h"
#include "header.h"
#include "malloc.h"

enum ExprType
{
  EXPR_CONST,
  EXPR_VAR,
  EXPR_ADD_1,
  EXPR_ARITHMETIC,
  EXPR_FUNCALL,
  EXPR_CMP,
  EXPR_COUNT
};

enum CmpType
{
  CMP_EQEQ,
  CMP_NEQ,
  CMP_LE,
  CMP_GE,
  CMP_LEQ,
  CMP_GEQ,
};

struct arith_s;
typedef struct __expr_s
{
  int type;

  union
  {
    struct
    {
      const_t v;

    } e_const;

    struct
    {
      const char *v;

    } e_var;

    struct
    {
      struct __expr_s *v;

    } e_add_one;

    struct
    {
      struct arith_s *tree; /* postfix arith tree */
      size_t tl;

    } e_arith;

    struct
    {
      struct __expr_s *name;
      struct __expr_s **args;
      size_t al;

    } e_funcall;

    struct
    {
      int type;
      struct __expr_s *left;
      struct __expr_s *right;

    } e_cmp;

  } v;

} expr_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API void sf_expr_print (expr_t);
  SF_API int sf_expr_tobool (expr_t);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // EXPR_H
