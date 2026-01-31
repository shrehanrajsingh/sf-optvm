#include "arith.h"

SF_API void
sf_arithnode_print (arith_node_t a)
{
  if (a.type == ARITH_NODE_OPERATOR)
    {
      printf ("arith_op: %s\n", a.v.op);
    }
  else
    {
      printf ("arith_expr: ");
      sf_expr_print (*a.v.expr);
      printf ("\n");
    }
}

SF_API int
sf_arith_getprec (char c)
{
  const char prec[] = { '+', '-', '*', '/', '\0' };
  int prec_v[] = { 10, 10, 20, 20, 0 };

  for (int i = 0; prec[i]; i++)
    if (prec[i] == c)
      return prec_v[i];

  return -1;
}

/**
 * Converts an infix arithmetic expression to postfix (Reverse Polish
 * Notation) using the Shunting Yard algorithm.
 *
 * @param t Pointer to an array of arithmetic nodes representing the infix
 * expression. On return, this will point to the postfix expression.
 * @param s The number of nodes in the input expression.
 *
 * The function allocates a buffer twice the size of the input to hold both
 * the output queue and operator stack. It iterates through each node:
 * - Expression nodes (operands) are pushed directly to the output.
 * - Operator nodes are handled by first popping higher or equal precedence
 *   operators from the stack to the output, then pushing the current
 * operator.
 *
 * After processing all nodes, remaining operators are popped to the output.
 * The original array is freed and replaced with the postfix result.
 *
 * @note The caller is responsible for freeing the resulting array.
 * @note This implementation assumes left-to-right associativity for all
 * operators.
 */
SF_API void
sf_arith_makepostfix (arith_node_t **t, size_t s)
{
  arith_node_t *n = *t;

  arith_node_t *buf = SFMALLOC ((s << 1) * sizeof (*buf));

  arith_node_t *w = buf;
  arith_node_t *opst = buf + s;

  size_t wc = 0;
  size_t oc = 0;

  for (int i = 0; i < s; i++)
    {
      if (n[i].type == ARITH_NODE_E_O)
        {
          w[wc++] = n[i];
        }
      else
        {
          while (oc
                 && sf_arith_getprec (opst[oc - 1].v.op[0])
                        >= sf_arith_getprec (n[i].v.op[0]))
            w[wc++] = opst[--oc];

          opst[oc++] = n[i];
        }
    }

  while (oc)
    w[wc++] = opst[--oc];

  for (int i = 0; i < wc; i++)
    (*t)[i] = w[i];

  SFFREE (buf);
}

SF_API void
sf_arith_print (arith_node_t n)
{
  switch (n.type)
    {
    case ARITH_NODE_E_O:
      {
        printf ("ARITH_EXPR: ");
        sf_expr_print (*n.v.expr);
        putchar ('\n');
      }
      break;

    case ARITH_NODE_OPERATOR:
      {
        printf ("ARITH_OPERATOR: %s\n", n.v.op);
      }
      break;

    default:
      break;
    }
}

SF_API void
sf_arith_print_tree (arith_node_t *n, size_t ns)
{
  for (size_t i = 0; i < ns; i++)
    sf_arith_print (n[i]);
}

SF_API expr_t
sf_arith_eval_consttree (arith_node_t *n, size_t ns)
{
  expr_t **st = SFMALLOC (ns * sizeof (*st));
  expr_t res;
  res.type = EXPR_CONST;
  res.v.e_const.v = (const_t){ .type = CONST_INT, .v.c_int.v = 2104 };

  size_t sc = 0;

  for (int i = 0; i < ns; i++)
    {
      // sf_arithnode_print (n[i]);
      if (n[i].type == ARITH_NODE_E_O)
        {
          st[sc++] = n[i].v.expr;
        }
      else
        {
          const char *op = n[i].v.op;

          if (*op == '+')
            {
              // printf ("%d\n", sc);
              expr_t *right = st[--sc];
              expr_t *left = st[--sc];

              assert (right->type == left->type && left->type == EXPR_CONST);

              const_t *rc = &right->v.e_const.v;
              const_t *lc = &left->v.e_const.v;

              switch (rc->type)
                {
                case CONST_INT:
                  {
                    assert (lc->type == CONST_FLOAT || lc->type == CONST_INT);

                    if (lc->type == CONST_INT)
                      {
                        res.type = EXPR_CONST;
                        res.v.e_const.v
                            = (const_t){ .type = CONST_INT,
                                         .v.c_int.v
                                         = lc->v.c_int.v + rc->v.c_int.v };
                      }
                    else if (lc->type == CONST_FLOAT)
                      {
                        res.type = EXPR_CONST;
                        res.v.e_const.v
                            = (const_t){ .type = CONST_FLOAT,
                                         .v.c_float.v
                                         = lc->v.c_float.v + rc->v.c_int.v };
                      }
                  }
                  break;

                case CONST_FLOAT:
                  {
                    assert (lc->type == CONST_FLOAT || lc->type == CONST_INT);

                    if (lc->type == CONST_INT)
                      {
                        res.type = EXPR_CONST;
                        res.v.e_const.v
                            = (const_t){ .type = CONST_FLOAT,
                                         .v.c_float.v
                                         = lc->v.c_int.v + rc->v.c_float.v };
                      }
                    else if (lc->type == CONST_FLOAT)
                      {
                        res.type = EXPR_CONST;
                        res.v.e_const.v
                            = (const_t){ .type = CONST_FLOAT,
                                         .v.c_float.v
                                         = lc->v.c_float.v + rc->v.c_float.v };
                      }
                  }
                  break;

                default:
                  break;
                }
            }
          else if (*op == '-')
            {
              expr_t *right = st[--sc];
              expr_t *left = st[--sc];

              assert (right->type == left->type && left->type == EXPR_CONST);

              const_t *rc = &right->v.e_const.v;
              const_t *lc = &left->v.e_const.v;

              switch (rc->type)
                {
                case CONST_INT:
                  {
                    assert (lc->type == CONST_FLOAT || lc->type == CONST_INT);

                    if (lc->type == CONST_INT)
                      {
                        res.type = EXPR_CONST;
                        res.v.e_const.v
                            = (const_t){ .type = CONST_INT,
                                         .v.c_int.v
                                         = lc->v.c_int.v - rc->v.c_int.v };
                      }
                    else if (lc->type == CONST_FLOAT)
                      {
                        res.type = EXPR_CONST;
                        res.v.e_const.v
                            = (const_t){ .type = CONST_FLOAT,
                                         .v.c_float.v
                                         = lc->v.c_float.v - rc->v.c_int.v };
                      }
                  }
                  break;

                case CONST_FLOAT:
                  {
                    assert (lc->type == CONST_FLOAT || lc->type == CONST_INT);

                    if (lc->type == CONST_INT)
                      {
                        res.type = EXPR_CONST;
                        res.v.e_const.v
                            = (const_t){ .type = CONST_FLOAT,
                                         .v.c_float.v
                                         = lc->v.c_int.v - rc->v.c_float.v };
                      }
                    else if (lc->type == CONST_FLOAT)
                      {
                        res.type = EXPR_CONST;
                        res.v.e_const.v
                            = (const_t){ .type = CONST_FLOAT,
                                         .v.c_float.v
                                         = lc->v.c_float.v - rc->v.c_float.v };
                      }
                  }
                  break;

                default:
                  break;
                }
            }
        }
    }

  SFFREE (st);
  return res;
}

SF_API obj_t *
sf_arith_eval_objtree (arith_objnode_t *n, size_t ns)
{
  // obj_t **st = SFMALLOC (ns * sizeof (*st));
  obj_t *st[64];
  obj_t res;
  res.type = OBJ_CONST;
  res.v.o_const.v = (const_t){ .type = CONST_INT, .v.c_int.v = 2104 };

  obj_t **st_l = st;

  for (int i = 0; i < ns; i++)
    {
      // sf_arithnode_print (n[i]);
      if (n[i].type == ARITH_NODE_E_O)
        {
          *st_l++ = n[i].v.obj;
        }
      else
        {
          const char *op = n[i].v.op;

          if (*op == '+')
            {
              // printf ("%d\n", sc);
              obj_t *right = *--st_l;
              obj_t *left = *--st_l;

              // assert (right->type == left->type && left->type == OBJ_CONST);

              const_t *rc = &right->v.o_const.v;
              const_t *lc = &left->v.o_const.v;

              switch (rc->type)
                {
                case CONST_INT:
                  {
                    // assert (lc->type == CONST_FLOAT || lc->type ==
                    // CONST_INT);

                    switch (lc->type)
                      {
                      case CONST_INT:
                        {
                          res.type = OBJ_CONST;
                          res.v.o_const.v
                              = (const_t){ .type = CONST_INT,
                                           .v.c_int.v
                                           = lc->v.c_int.v + rc->v.c_int.v };
                        }
                        break;

                      case CONST_FLOAT:
                        {
                          res.type = OBJ_CONST;
                          res.v.o_const.v
                              = (const_t){ .type = CONST_FLOAT,
                                           .v.c_float.v
                                           = lc->v.c_float.v + rc->v.c_int.v };
                        }
                        break;

                      default:
                        break;
                      }
                  }
                  break;

                case CONST_FLOAT:
                  {
                    // assert (lc->type == CONST_FLOAT || lc->type ==
                    // CONST_INT);

                    switch (lc->type)
                      {
                      case CONST_INT:
                        {
                          res.type = OBJ_CONST;
                          res.v.o_const.v
                              = (const_t){ .type = CONST_FLOAT,
                                           .v.c_float.v
                                           = lc->v.c_int.v + rc->v.c_float.v };
                        }
                        break;

                      case CONST_FLOAT:
                        {
                          res.type = OBJ_CONST;
                          res.v.o_const.v
                              = (const_t){ .type = CONST_FLOAT,
                                           .v.c_float.v = lc->v.c_float.v
                                                          + rc->v.c_float.v };
                        }
                        break;

                      default:
                        break;
                      }
                  }
                  break;

                default:
                  break;
                }
            }
          else if (*op == '-')
            {
              obj_t *right = *--st_l;
              obj_t *left = *--st_l;

              // assert (right->type == left->type && left->type == OBJ_CONST);

              const_t *rc = &right->v.o_const.v;
              const_t *lc = &left->v.o_const.v;

              switch (rc->type)
                {
                case CONST_INT:
                  {
                    // assert (lc->type == CONST_FLOAT || lc->type ==
                    // CONST_INT);

                    switch (lc->type)
                      {
                      case CONST_INT:
                        {
                          res.type = OBJ_CONST;
                          res.v.o_const.v
                              = (const_t){ .type = CONST_INT,
                                           .v.c_int.v
                                           = lc->v.c_int.v - rc->v.c_int.v };
                        }
                        break;

                      case CONST_FLOAT:
                        {
                          res.type = OBJ_CONST;
                          res.v.o_const.v
                              = (const_t){ .type = CONST_FLOAT,
                                           .v.c_float.v
                                           = lc->v.c_float.v - rc->v.c_int.v };
                        }
                        break;

                      default:
                        break;
                      }
                  }
                  break;

                case CONST_FLOAT:
                  {
                    // assert (lc->type == CONST_FLOAT || lc->type ==
                    // CONST_INT);

                    switch (lc->type)
                      {
                      case CONST_INT:
                        {
                          res.type = OBJ_CONST;
                          res.v.o_const.v
                              = (const_t){ .type = CONST_FLOAT,
                                           .v.c_float.v
                                           = lc->v.c_int.v - rc->v.c_float.v };
                        }
                        break;

                      case CONST_FLOAT:
                        {
                          res.type = OBJ_CONST;
                          res.v.o_const.v
                              = (const_t){ .type = CONST_FLOAT,
                                           .v.c_float.v = lc->v.c_float.v
                                                          - rc->v.c_float.v };
                        }
                        break;

                      default:
                        break;
                      }
                  }
                  break;

                default:
                  break;
                }
            }
        }
    }

  if (res.type == OBJ_CONST)
    {
      obj_t *o = sf_objstore_req_forconst (&res.v.o_const.v);

      if (o != NULL)
        return o;
    }

  obj_t *rr = sf_objstore_req ();
  rr->type = res.type;
  rr->v = res.v;

  return rr;
}