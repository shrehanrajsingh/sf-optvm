#if !defined(SFSOCKET_H)
#define SFSOCKET_H

#include "../header.h"

struct __mod_s;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API struct __mod_s *sf_lib_socket_makemod ();

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // SFSOCKET_H
