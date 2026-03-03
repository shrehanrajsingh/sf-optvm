#include "stmt.h"

SF_API void
sf_stmt_print (stmt_t s)
{
  switch (s.type)
    {
    case STMT_VARDECL:
      {
        printf ("VARDECL:\nname: ");
        sf_expr_print (*s.v.s_vardecl.name);
        printf ("\nvalue: ");
        sf_expr_print (*s.v.s_vardecl.val);
        printf ("\n");
      }
      break;

    case STMT_FUNCALL:
      {
        printf ("FUNCALL:\nname: ");
        sf_expr_print (*s.v.s_funcall.name);
        printf ("\nargs (%lu)\n", s.v.s_funcall.argc);

        for (size_t i = 0; i < s.v.s_funcall.argc; ++i)
          {
            sf_expr_print (*s.v.s_funcall.args[i]);
            if (i < s.v.s_funcall.argc - 1)
              printf ("\n");
          }
        printf ("\n");
      }

      break;

    case STMT_IFBLOCK:
      {
        printf ("IFBLOCK:\ncond: ");
        sf_expr_print (*s.v.s_ifblock.cond);
        printf ("\nbody (%lu):\n", s.v.s_ifblock.bl);

        for (size_t i = 0; i < s.v.s_ifblock.bl; i++)
          sf_stmt_print (s.v.s_ifblock.body[i]);

        printf ("else_body (%lu)\n", s.v.s_ifblock.else_bl);

        for (size_t i = 0; i < s.v.s_ifblock.else_bl; i++)
          sf_stmt_print (s.v.s_ifblock.else_body[i]);
      }
      break;

    case STMT_WHILE:
      {
        printf ("WHILE:\ncond: ");
        sf_expr_print (*s.v.s_while.cond);

        printf ("\nbody (%lu):\n", s.v.s_while.bl);

        for (size_t i = 0; i < s.v.s_while.bl; i++)
          sf_stmt_print (s.v.s_while.body[i]);
      }
      break;

    case STMT_FUNDECL:
      {
        printf ("FUN_DECL: name '%s'\n", s.v.s_fundecl.name);

        printf ("args: (%lu)\n", s.v.s_fundecl.argc);
        for (int i = 0; i < s.v.s_fundecl.argc; i++)
          {
            sf_expr_print (*s.v.s_fundecl.args[i]);
            putchar ('\n');
          }

        printf ("body: (%lu)\n", s.v.s_fundecl.bl);
        for (int i = 0; i < s.v.s_fundecl.bl; i++)
          sf_stmt_print (s.v.s_fundecl.body[i]);
      }
      break;

    case STMT_RETURN:
      {
        printf ("STMT_RETURN: expr = ");
        sf_expr_print (*s.v.s_return.v);
        putchar ('\n');
      }
      break;

    case STMT_CLASSDECL:
      {
        printf ("STMT_CLASSDECL: name '%s'\n", s.v.s_classdecl.name);

        printf ("body: (%lu)\n", s.v.s_classdecl.bl);
        for (int i = 0; i < s.v.s_classdecl.bl; i++)
          {
            printf ("(%d) ", i + 1);
            sf_stmt_print (s.v.s_classdecl.body[i]);
          }
      }
      break;

    case STMT_FOR:
      {
        printf ("STMT_FOR: vars (%lu)", s.v.s_for.vl);

        for (int i = 0; i < s.v.s_for.vl; i++)
          {
            printf ("\n(%d) ", i);
            sf_expr_print (*s.v.s_for.vars[i]);
          }

        printf ("\ncond: ");
        sf_expr_print (*s.v.s_for.cond);

        printf ("\nbody (%lu)\n", s.v.s_for.bl);

        for (int i = 0; i < s.v.s_for.bl; i++)
          sf_stmt_print (s.v.s_for.body[i]);
      }
      break;

    case STMT_IMPORT:
      {
        printf ("STMT_IMPORT: path ('%s'); alias ('%s')\n", s.v.s_import.path,
                s.v.s_import.alias);
      }
      break;

    case STMT_EOF:
      {
        printf ("STMT_EOF\n");
      }
      break;

    default:
      break;
    }
}

SF_API void
sf_stmt_free (stmt_t *st)
{
  switch (st->type)
    {
    case STMT_CLASSDECL:
      {
        /* freed via token_free */
        // SFFREE ((void *)st->v.s_classdecl.name);
        for (size_t i = 0; i < st->v.s_classdecl.bl; i++)
          sf_stmt_free (&st->v.s_classdecl.body[i]);
        SFFREE (st->v.s_classdecl.body);
      }
      break;

    case STMT_EOF:
      break;

    case STMT_FOR:
      {
        sf_expr_free (st->v.s_for.cond);

        for (size_t i = 0; i < st->v.s_for.bl; i++)
          sf_stmt_free (&st->v.s_for.body[i]);

        SFFREE (st->v.s_for.body);

        for (size_t i = 0; i < st->v.s_for.bl; i++)
          {
            sf_expr_free (st->v.s_for.vars[i]);
            SFFREE (st->v.s_for.vars[i]);
          }

        SFFREE (st->v.s_for.vars);
      }
      break;

    case STMT_FUNCALL:
      {
        sf_expr_free (st->v.s_funcall.name);
        SFFREE (st->v.s_funcall.name);

        for (size_t i = 0; i < st->v.s_funcall.argc; i++)
          {
            sf_expr_free (st->v.s_funcall.args[i]);
            SFFREE (st->v.s_funcall.args[i]);
          }

        SFFREE (st->v.s_funcall.args);
      }
      break;

    case STMT_FUNDECL:
      {
        for (size_t i = 0; i < st->v.s_fundecl.argc; i++)
          {
            sf_expr_free (st->v.s_fundecl.args[i]);
            SFFREE (st->v.s_fundecl.args[i]);
          }

        SFFREE (st->v.s_fundecl.args);

        for (size_t i = 0; i < st->v.s_fundecl.bl; i++)
          sf_stmt_free (&st->v.s_fundecl.body[i]);

        SFFREE (st->v.s_fundecl.body);
      }
      break;

    case STMT_IFBLOCK:
      {
        sf_expr_free (st->v.s_ifblock.cond);
        SFFREE (st->v.s_ifblock.cond);

        for (size_t i = 0; i < st->v.s_ifblock.bl; i++)
          sf_stmt_free (&st->v.s_ifblock.body[i]);

        SFFREE (st->v.s_ifblock.body);

        for (size_t i = 0; i < st->v.s_ifblock.else_bl; i++)
          sf_stmt_free (&st->v.s_ifblock.else_body[i]);

        SFFREE (st->v.s_ifblock.else_body);
      }
      break;

    case STMT_IMPORT:
      {
        /* both are freed via token_free */
        // SFFREE ((void *)st->v.s_import.path);
        // SFFREE ((void *)st->v.s_import.alias);
      }
      break;

    case STMT_RETURN:
      {
        sf_expr_free (st->v.s_return.v);
        SFFREE (st->v.s_return.v);
      }
      break;

    case STMT_VARDECL:
      {
        sf_expr_free (st->v.s_vardecl.name);
        sf_expr_free (st->v.s_vardecl.val);

        SFFREE (st->v.s_vardecl.name);
        SFFREE (st->v.s_vardecl.val);
      }
      break;

    case STMT_WHILE:
      {
        sf_expr_free (st->v.s_while.cond);
        SFFREE (st->v.s_while.cond);

        for (size_t i = 0; i < st->v.s_while.bl; i++)
          sf_stmt_free (&st->v.s_while.body[i]);

        SFFREE (st->v.s_while.body);
      }
      break;

    default:
      break;
    }
}