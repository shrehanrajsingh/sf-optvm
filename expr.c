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
    case EXPR_FUNCALL:
      {
        printf ("EXPR_FUNCALL (%lu)\nname: ", e.v.e_funcall.al);
        sf_expr_print (*e.v.e_funcall.name);

        for (size_t i = 0; i < e.v.e_funcall.al; i++)
          {
            printf ("ARG (%lu): ", i);
            sf_expr_print (*e.v.e_funcall.args[i]);
          }
      }
      break;

    case EXPR_CMP:
      {
        printf ("EXPR_CMP: ");
        switch (e.v.e_cmp.type)
          {
          case CMP_EQEQ:
            printf ("EQEQ");
            break;
          case CMP_NEQ:
            printf ("NEQ");
            break;
          case CMP_LE:
            printf ("LE");
            break;
          case CMP_GE:
            printf ("GE");
            break;
          case CMP_LEQ:
            printf ("LEQ");
            break;
          case CMP_GEQ:
            printf ("GEQ");
            break;

          default:
            break;
          }

        printf ("\nleft: ");
        sf_expr_print (*e.v.e_cmp.left);
        printf ("\nright: ");
        sf_expr_print (*e.v.e_cmp.right);
      }
      break;

    case EXPR_DOT_ACCESS:
      {
        printf ("EXPR_DOT_ACCESS:\nleft: ");
        sf_expr_print (*e.v.e_dota.left);
        printf ("\nright: %s\n", e.v.e_dota.right);
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