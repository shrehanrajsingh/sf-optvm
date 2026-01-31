#if !defined(SFMALLOC_H)
#define SFMALLOC_H

#include "header.h"

#define SFMALLOC(X) __sf_malloc (X)
#define SFREALLOC(X, Y) __sf_realloc ((X), (Y))
#define SFFREE(X) __sf_free ((X))
#define SFSTRDUP(X) __sf_strdup ((X))

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API void *__sf_malloc (size_t);
  SF_API void *__sf_realloc (void *, size_t);
  SF_API void __sf_free (void *);

  SF_API char *__sf_strdup (const char *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // SFMALLOC_H
