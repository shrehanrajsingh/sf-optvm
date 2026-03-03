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
            printf ("\nARG (%lu): ", i);
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

    case EXPR_ARRAY:
      {
        printf ("EXPR_ARRAY (%lu):\n", e.v.e_array.vl);

        for (size_t i = 0; i < e.v.e_array.vl; i++)
          {
            sf_expr_print (*e.v.e_array.vals[i]);
            putchar ('\n');
          }
      }
      break;

    case EXPR_SQUARE_ACCESS:
      {
        printf ("EXPR_SQUARE_ACCESS:\nparent: ");
        sf_expr_print (*e.v.e_sqr_access.parent);
        printf ("\nidx: ");
        sf_expr_print (*e.v.e_sqr_access.idx);
      }
      break;

    case EXPR_TO_STEP:
      {
        printf ("EXPR_TO_STEP:\nlval: ");
        sf_expr_print (*e.v.e_to_step.lval);
        printf ("\nrval: ");
        sf_expr_print (*e.v.e_to_step.rval);

        if (e.v.e_to_step.step != NULL)
          {
            printf ("\nstep: ");
            sf_expr_print (*e.v.e_to_step.step);
          }
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

SF_API void
sf_expr_free (expr_t *e)
{
  if (e == NULL)
    return;

  switch (e->type)
    {
    case EXPR_ADD_1:
      sf_expr_free (e->v.e_add_one.v);
      SFFREE (e->v.e_add_one.v);
      break;

    case EXPR_ARITHMETIC:
      {
        arith_node_t *nodes = e->v.e_arith.tree;

        for (size_t i = 0; i < e->v.e_arith.tl; i++)
          {
            if (nodes[i].type == ARITH_NODE_E_O)
              {
                sf_expr_free (nodes[i].v.expr);
                SFFREE (nodes[i].v.expr);
              }
          }

        SFFREE (nodes);
      }
      break;

    case EXPR_ARRAY:
      {
        for (size_t i = 0; i < e->v.e_array.vl; i++)
          {
            sf_expr_free (e->v.e_array.vals[i]);
            SFFREE (e->v.e_array.vals[i]);
          }

        SFFREE (e->v.e_array.vals);
      }
      break;

    case EXPR_CMP:
      {
        sf_expr_free (e->v.e_cmp.left);
        sf_expr_free (e->v.e_cmp.right);

        SFFREE (e->v.e_cmp.left);
        SFFREE (e->v.e_cmp.right);
      }
      break;

    case EXPR_CONST:
      sf_const_free (&e->v.e_const.v);
      break;

    case EXPR_DOT_ACCESS:
      sf_expr_free (e->v.e_dota.left);
      SFFREE (e->v.e_dota.left);
      SFFREE (e->v.e_dota.right);
      break;

    case EXPR_FUNCALL:
      {
        sf_expr_free (e->v.e_funcall.name);
        SFFREE (e->v.e_funcall.name);

        for (size_t i = 0; i < e->v.e_funcall.al; i++)
          {
            sf_expr_free (e->v.e_funcall.args[i]);
            SFFREE (e->v.e_funcall.args[i]);
          }

        SFFREE (e->v.e_funcall.args);
      }
      break;

    case EXPR_SQUARE_ACCESS:
      {
        sf_expr_free (e->v.e_sqr_access.idx);
        sf_expr_free (e->v.e_sqr_access.parent);

        SFFREE (e->v.e_sqr_access.idx);
        SFFREE (e->v.e_sqr_access.parent);
      }
      break;

    case EXPR_TO_STEP:
      {
        sf_expr_free (e->v.e_to_step.lval);
        sf_expr_free (e->v.e_to_step.rval);
        sf_expr_free (e->v.e_to_step.step);

        SFFREE (e->v.e_to_step.lval);
        SFFREE (e->v.e_to_step.rval);
        SFFREE (e->v.e_to_step.step);
      }
      break;

    case EXPR_VAR:
      // here;
      // D (printf ("%s\n", e->v.e_var.v));
      SFFREE ((void *)e->v.e_var.v);
      break;

    default:
      break;
    }
}