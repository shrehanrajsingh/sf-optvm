#include "malloc.h"

SF_API void *
__sf_malloc (size_t size)
{
  void *p = malloc (size);

  if (p == NULL)
    {
      D (perror ("cannot allocate buffer"));
      return NULL;
    }

  return p;
}

SF_API void *
__sf_realloc (void *old, size_t ns)
{
  void *p = realloc (old, ns);

  if (p == NULL)
    {
      D (perror ("cannot allocate buffer"));
      return NULL;
    }

  return p;
}

SF_API void
__sf_free (void *ptr)
{
  if (ptr != NULL)
    free (ptr);
}

SF_API char *
__sf_strdup (const char *s)
{
  size_t sl = strlen (s);
  char *res = __sf_malloc (sl + 1);

  if (res == NULL)
    {
      D (perror ("cannot allocate memory for string"));
      return NULL;
    }

  memcpy (res, s, sl);
  res[sl] = '\0';

  return res;
}