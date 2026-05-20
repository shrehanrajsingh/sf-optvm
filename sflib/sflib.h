#if !defined(SFLIB_H)
#define SFLIB_H

#include "../header.h"
#include "../malloc.h"

#include "json.h"
#include "sfsocket.h"
#include "sfthread.h"

struct _vm_s;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API void sf_lib_addlibs (struct _vm_s *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // SFLIB_H
