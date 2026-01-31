#if !defined(HEADER_H)
#define HEADER_H

#include <assert.h>
#include <ctype.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif // _WIN32

#define SF_API
#define D(X)                                                                  \
  {                                                                           \
    printf ("%s:%d[%s]   ", __FILE__, __LINE__, __FUNCTION__);                \
    X;                                                                        \
    putchar ('\n');                                                           \
  }

#define here printf ("%s: %d\n", __FILE__, __LINE__);

#endif // HEADER_H
