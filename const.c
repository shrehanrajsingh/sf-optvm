#include "const.h"

SF_API const_t
sf_const_int_new (int v)
{
  const_t t;
  t.type = CONST_INT;
  t.v.c_int.v = v;

  return t;
}

SF_API const_t
sf_const_float_new (float v)
{
  const_t t;
  t.type = CONST_FLOAT;
  t.v.c_float.v = v;

  return t;
}

SF_API const_t
sf_const_str_new (const char *v)
{
  const_t t;
  t.type = CONST_STRING;
  t.v.c_str.v = SFSTRDUP (v);

  return t;
}

SF_API const_t
sf_const_bool_new (int v)
{
  const_t t;
  t.type = CONST_BOOL;
  t.v.c_bool.v = v;

  return t;
}

SF_API void
sf_const_print (const_t c)
{
  switch (c.type)
    {
    case CONST_INT:
      printf ("%d", c.v.c_int.v);
      break;
    case CONST_FLOAT:
      printf ("%g", c.v.c_float.v);
      break;
    case CONST_STRING:
      if (c.v.c_str.v)
        printf ("\"%s\"", c.v.c_str.v);
      else
        printf ("NULL");
      break;
    case CONST_BOOL:
      printf ("%s", c.v.c_bool.v ? "true" : "false");
      break;
    default:
      printf ("<unknown-const:%d>", c.type);
      break;
    }
}