#if !defined(STMT_H)
#define STMT_H

#include "expr.h"
#include "header.h"
#include "malloc.h"

enum StmtType
{
  STMT_VARDECL = 0,
  STMT_FUNCALL = 1,
  STMT_IFBLOCK = 2,
  STMT_WHILE = 3,
  STMT_FUNDECL = 4,
  STMT_RETURN = 5,
  STMT_CLASSDECL = 6,
  STMT_FOR = 7,
  STMT_IMPORT = 8,
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

    struct
    {
      expr_t *v;

    } s_return;

    struct
    {
      const char *name;
      struct __stmt_s *body;
      size_t bl;

    } s_classdecl;

    struct
    {
      expr_t **vars;
      size_t vl;

      struct __stmt_s *body;
      size_t bl;

      expr_t *cond;

    } s_for;

    struct
    {
      const char *path;
      const char *alias;

    } s_import;

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
  SF_API void sf_stmt_free (stmt_t *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // STMT_H
