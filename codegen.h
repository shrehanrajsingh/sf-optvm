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

#if !defined(PRESERVE)
#define PRESERVE(vm)                                                          \
  size_t _pres_g_slot = (vm)->meta.g_slot;                                    \
  size_t _pres_l_slot = (vm)->meta.l_slot;                                    \
  size_t _pres_n_slot = (vm)->meta.n_slot;                                    \
  size_t _pres_slot = (vm)->meta.slot;

#endif // PRESERVE

#if !defined(RESTORE)
#define RESTORE(vm)                                                           \
  vm->meta.g_slot = _pres_g_slot;                                             \
  vm->meta.l_slot = _pres_l_slot;                                             \
  vm->meta.n_slot = _pres_n_slot;                                             \
  vm->meta.slot = _pres_slot;

#endif // RESTORE

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
