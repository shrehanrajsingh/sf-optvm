#include "mut.h"

SF_API sfmutex_t
sf_mutex_new ()
{
  sfmutex_t m;
  return m;
}

SF_API void
sf_mutex_lock (sfmutex_t *m)
{
#if defined(_WIN32)
#else
  pthread_mutex_lock (&m->mut);
#endif // _WIN32
}

SF_API void
sf_mutex_unlock (sfmutex_t *m)
{
#if defined(_WIN32)
#else
  pthread_mutex_unlock (&m->mut);
#endif // _WIN32
}