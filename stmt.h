#if !defined(STMT_H)
#define STMT_H

#include "expr.h"
#include "header.h"
#include "malloc.h"

enum StmtType
{
  STMT_VARDECL,
  STMT_FUNCALL,
  STMT_IFBLOCK,
  STMT_WHILE,
  STMT_FUNDECL,
  STMT_EOF
};

typedef struct __stmt_s
{
  int type;

  union
  {
    struct
    {
      expr_t *name;
      expr_t *val;

    } s_vardecl;

    struct
    {
      expr_t *name;
      expr_t **args;
      size_t argc;

    } s_funcall;

    struct
    {
      expr_t *cond;
      struct __stmt_s *body;
      size_t bl;

      struct __stmt_s *else_body;
      size_t else_bl;

    } s_ifblock;

    struct
    {
      expr_t *cond;
      struct __stmt_s *body;
      size_t bl;

    } s_while;

    struct
    {
      const char *name;
      expr_t **args;
      size_t argc;

      struct __stmt_s *body;
      size_t bl;

    } s_fundecl;

  } v;

} stmt_t;

typedef struct
{
  stmt_t *vals;
  size_t vl;
  size_t vc;

} StmtSM;

#define STMT_SM_VALS_CAP (512)

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API void sf_stmt_print (stmt_t);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // STMT_H
