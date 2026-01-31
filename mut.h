#if !defined(MUT_H)
#define MUT_H

#include "header.h"

typedef struct
{
#if defined(_WIN32)
  HANDLE mut;
#else
  pthread_mutex_t mut;
#endif // _WIN32

} sfmutex_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API sfmutex_t sf_mutex_new ();
  SF_API void sf_mutex_lock (sfmutex_t *);
  SF_API void sf_mutex_unlock (sfmutex_t *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // MUT_H
