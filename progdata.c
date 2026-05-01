#include "progdata.h"

SF_API progdata_t
sf_progdata_new ()
{
  progdata_t t;
  t.lc = SF_PROGDATA_LC_CAP;
  t.ll = 0;
  t.lines = NULL;
  t.curr_line = 0;

  return t;
}

SF_API progdata_t
sf_progdata_new_withLines (char **lines, size_t ll)
{
  progdata_t t = sf_progdata_new ();
  t.ll = ll;
  t.lc = (ll / SF_PROGDATA_LC_CAP + 1) * SF_PROGDATA_LC_CAP;
  t.lines = lines;

  return t;
}

SF_API inline void
sf_progdata_update_currline (progdata_t *pg, size_t s)
{
  pg->curr_line = s;
}
