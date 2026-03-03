#if !defined(NATIVES_H)
#define NATIVES_H

#include "bytecode.h"
#include "header.h"
#include "malloc.h"

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API void sf_natives_add_tovm (vm_t *);

  SF_API obj_t *sf_native_putln (obj_t *);
  SF_API obj_t *sf_native_put (obj_t *);
  SF_API obj_t *sf_native_write (obj_t **, size_t);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // NATIVES_H
