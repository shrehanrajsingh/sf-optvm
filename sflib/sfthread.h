#if !defined(SFTHREAD_H)
#define SFTHREAD_H

#include "../header.h"

struct __mod_s;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API struct __mod_s *sf_lib_thread_makemod ();

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // SFTHREAD_H
