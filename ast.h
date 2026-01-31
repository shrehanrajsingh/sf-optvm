#if !defined(AST_H)
#define AST_H

#include "arith.h"
#include "expr.h"
#include "header.h"
#include "malloc.h"
#include "stmt.h"
#include "token.h"

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API expr_t *sf_expr_gen (token_t *, token_t *);
  SF_API StmtSM *sf_ast_gen (TokenSM *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // AST_H
