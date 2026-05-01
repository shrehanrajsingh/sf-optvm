#if !defined(PROGDATA_H)
#define PROGDATA_H

#include "header.h"

/* lines are zero-indexed */
struct __typed_str
{
  char **lines;
  size_t lc;
  size_t ll;

  size_t curr_line;
};

typedef struct __typed_str progdata_t;

#define SF_PROGDATA_LC_CAP (64)

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API progdata_t sf_progdata_new ();
  SF_API progdata_t sf_progdata_new_withLines (char **, size_t);
  SF_API inline void sf_progdata_update_currline (progdata_t *, size_t);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // PROGDATA_H
