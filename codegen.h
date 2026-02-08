#if !defined(CODEGEN_H)
#define CODEGEN_H

#include "arith.h"
#include "bytecode.h"
#include "const.h"
#include "expr.h"
#include "header.h"
#include "ht.h"
#include "object.h"
#include "stmt.h"

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API void sf_vm_gen_b_fromexpr (vm_t *, expr_t);
  SF_API void sf_vm_gen_bytecode (vm_t *, StmtSM *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // CODEGEN_H
