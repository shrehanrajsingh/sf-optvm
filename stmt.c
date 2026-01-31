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

    case STMT_EOF:
      {
        printf ("STMT_EOF\n");
      }
      break;

    default:
      break;
    }
}