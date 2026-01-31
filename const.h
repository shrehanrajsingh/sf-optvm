#if !defined(CONST_H)
#define CONST_H

#include "header.h"
#include "malloc.h"

enum ConstType
{
  CONST_INT,
  CONST_FLOAT,
  CONST_STRING,
  CONST_NONE,
  CONST_BOOL
};

typedef struct sfconst_s
{
  int type;

  union
  {
    struct
    {
      int v;
    } c_int;

    struct
    {
      float v;
    } c_float;

    struct
    {
      char *v;
    } c_str;

    struct
    {
      int v;
    } c_bool;

  } v;

} const_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API const_t sf_const_int_new (int);
  SF_API const_t sf_const_float_new (float);
  SF_API const_t sf_const_str_new (const char *);
  SF_API const_t sf_const_bool_new (int);
  SF_API void sf_const_print (const_t);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // CONST_H
