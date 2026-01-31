#if !defined(PARSER_H)
#define PARSER_H

#include "arith.h"
#include "header.h"
#include "malloc.h"
#include "mod.h"
#include "object.h"

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API void sf_parser_exec_one (mod_t *);
  SF_API void sf_parser_exec (mod_t *);
  SF_API obj_t *sf_parser_eval_expr (expr_t *, mod_t *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // PARSER_H
