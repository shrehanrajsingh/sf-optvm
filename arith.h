#if !defined(ARITH_H)
#define ARITH_H

#include "expr.h"
#include "header.h"
#include "malloc.h"
#include "object.h"

enum ArithType
{
  ARITH_NODE_E_O, /* expr or object */
  ARITH_NODE_OPERATOR,
};

typedef struct arith_s
{
  int type;

  union
  {
    expr_t *expr;
    char *op;

  } v;

} arith_node_t;

typedef struct arith_objs
{
  int type;

  union
  {
    obj_t *obj;
    char *op;

  } v;

} arith_objnode_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API void sf_arithnode_print (arith_node_t);
  SF_API int sf_arith_getprec (char);
  SF_API void sf_arith_makepostfix (arith_node_t **,
                                    size_t); /* convert infix to postfix */
  SF_API expr_t sf_arith_eval_consttree (arith_node_t *, size_t);
  SF_API obj_t *sf_arith_eval_objtree (arith_objnode_t *, size_t);

  SF_API void sf_arith_print (arith_node_t);
  SF_API void sf_arith_print_tree (arith_node_t *, size_t);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // ARITH_H
