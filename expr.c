#include "expr.h"
#include "arith.h"

SF_API void
sf_expr_print (expr_t e)
{
  switch (e.type)
    {
    case EXPR_CONST:
      switch (e.v.e_const.v.type)
        {
        case CONST_INT:
          printf ("INT: %d", e.v.e_const.v.v.c_int.v);
          break;
        case CONST_FLOAT:
          printf ("FLOAT: %f", e.v.e_const.v.v.c_float.v);
          break;
        case CONST_STRING:
          printf ("STRING: \"%s\"",
                  e.v.e_const.v.v.c_str.v ? e.v.e_const.v.v.c_str.v : "");
          break;
        case CONST_BOOL:
          printf ("BOOL: %s", e.v.e_const.v.v.c_bool.v ? "true" : "false");
          break;
        case CONST_NONE:
          printf ("none");
          break;
        default:
          printf ("<const?>");
          break;
        }
      break;
    case EXPR_VAR:
      {
        printf ("VAR: %s", e.v.e_var.v ? e.v.e_var.v : "<null>");
      }
      break;
    case EXPR_ADD_1:
      {
        printf ("ADD_1:\n");
        sf_expr_print (*e.v.e_add_one.v);
      }
      break;
    case EXPR_ARITHMETIC:
      {
        printf ("EXPR_ARITHMETIC (%lu)\n", e.v.e_arith.tl);
        sf_arith_print_tree (e.v.e_arith.tree, e.v.e_arith.tl);
      }
      break;
    default:
      {
        printf ("<expr?> %d\n", e.type);
      }
      break;
    }
}

SF_API int
sf_expr_tobool (expr_t e)
{
  int r = 0;

  switch (e.type)
    {
    case EXPR_CONST:
      {
        switch (e.v.e_const.v.type)
          {
          case CONST_INT:
            r = e.v.e_const.v.v.c_int.v != 0;
            break;

          case CONST_FLOAT:
            r = e.v.e_const.v.v.c_float.v != 0.0;
            break;

          case CONST_NONE:
            r = 0;
            break;

          case CONST_BOOL:
            r = e.v.e_const.v.v.c_bool.v;
            break;

          case CONST_STRING:
            r = e.v.e_const.v.v.c_str.v[0] != '\0';
            break;

          default:
            break;
          }
      }
      break;

    default:
      break;
    }

  return r;
}