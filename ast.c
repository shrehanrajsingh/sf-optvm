#include "ast.h"

size_t
get_tbsp (token_t *curr, token_t *start)
{
  token_t t;
  int gb = 0;

  while (curr - start > -1)
    {
      t = *curr--;
      // sf_token_print (t);

      if (t.type == TOK_OPERATOR)
        {
          const char *op = t.v.t_operator.value;

          if (strstr ("({[", op) != NULL)
            gb++;

          if (strstr (")}]", op) != NULL)
            gb--;
        }

      if (t.type == TOK_SPACE && !gb)
        {
          return t.v.space.v;
        }

      if (t.type == TOK_NEWLINE && !gb)
        {
          break;
        }
    }

  return 0;
}

token_t *
get_block (token_t *start, size_t tbsp)
{
  token_t *e = start;
  int gb = 0;

  while (e->type != TOK_EOF)
    {
      token_t t = *e++;

      if (t.type == TOK_OPERATOR)
        {
          const char *op = t.v.t_operator.value;

          if (strstr ("({[", op) != NULL)
            gb++;

          if (strstr (")}]", op) != NULL)
            gb--;
        }

      if (gb)
        continue;

      if (t.type == TOK_SPACE)
        {
          if (t.v.space.v <= tbsp)
            break;
        }
      else if (t.type == TOK_NEWLINE)
        {
          t = *e;

          while (t.type == TOK_NEWLINE)
            t = *++e;

          if (t.type != TOK_SPACE)
            {
              break;
            }
          else
            {
              if (t.v.space.v <= tbsp)
                break;
            }
        }
    }

  return --e;
}

SF_API StmtSM *
sf_ast_gen (TokenSM *smt)
{
  StmtSM *res = SFMALLOC (sizeof (*res));
  res->vc = STMT_SM_VALS_CAP;
  res->vl = 0;
  res->vals = SFMALLOC (res->vc * sizeof (*res->vals));

  token_t *smtv = smt->vals;
  token_t tok = *smtv++;

  while (tok.type != TOK_EOF)
    {
      if (res->vl >= res->vc)
        {
          res->vc += STMT_SM_VALS_CAP;
          res->vals = SFREALLOC (res->vals, res->vc * sizeof (*res->vals));
        }

      switch (tok.type)
        {
        case TOK_OPERATOR:
          {
            const char *op = tok.v.t_operator.value;

            if (*op == '=' && !op[1])
              {
                token_t *smt_back = smtv;
                token_t *smt_front = smtv;

                int gb = 0;
                token_t t;

                while (!gb && smt_back > smt->vals)
                  {
                    t = *--smt_back;

                    switch (t.type)
                      {
                      case TOK_OPERATOR:
                        {
                          const char *op = t.v.t_operator.value;

                          if (strstr ("({[", op) != NULL)
                            gb++;

                          if (strstr (")}]", op) != NULL)
                            gb--;
                        }
                        break;

                      case TOK_NEWLINE:
                        {
                          if (!gb)
                            goto l2;
                        }
                        break;

                      default:
                        break;
                      }
                  }

              l2:;
                gb = 0;

                while (smt_front->type != TOK_EOF)
                  {
                    t = *smt_front++;

                    switch (t.type)
                      {
                      case TOK_OPERATOR:
                        {
                          const char *op = t.v.t_operator.value;

                          if (strstr ("({[", op) != NULL)
                            gb++;

                          if (strstr (")}]", op) != NULL)
                            gb--;
                        }
                        break;

                      case TOK_NEWLINE:
                        {
                          if (!gb)
                            goto l0;
                        }
                        break;

                      default:
                        break;
                      }
                  }

              l0:;

                stmt_t st;
                st.type = STMT_VARDECL;
                st.v.s_vardecl.name = sf_expr_gen (smt_back, smtv - 1);
                st.v.s_vardecl.val = sf_expr_gen (smtv, smt_front - 1);

                res->vals[res->vl++] = st;
                smtv = smt_front - 1;
              }

            else if (*op == '(')
              {
                token_t *smt_front = smtv;
                token_t *smt_back = --smtv;

                int gb = 0;
                token_t t;

                while (!gb && smt_back > smt->vals)
                  {
                    t = *--smt_back;

                    switch (t.type)
                      {
                      case TOK_OPERATOR:
                        {
                          const char *op = t.v.t_operator.value;

                          if (strstr ("({[", op) != NULL)
                            gb++;

                          if (strstr (")}]", op) != NULL)
                            gb--;
                        }
                        break;

                      case TOK_NEWLINE:
                        {
                          if (!gb)
                            goto l3;
                        }
                        break;

                      default:
                        break;
                      }
                  }

              l3:;
                gb = 0;

                expr_t **args = NULL;
                size_t argc = 0;

                token_t *left = smt_front;

                while (!gb && smt_front->type != TOK_EOF)
                  {
                    t = *smt_front++;

                    switch (t.type)
                      {
                      case TOK_OPERATOR:
                        {
                          //   sf_token_print (t);
                          const char *op = t.v.t_operator.value;

                          if (*op == ')' && !gb)
                            {
                              if (args == NULL)
                                {
                                  if (smt_front - 1 != left)
                                    args = SFMALLOC (sizeof (*args));
                                }
                              else
                                {
                                  args = SFREALLOC (
                                      args, (argc + 1) * sizeof (*args));
                                }

                              if (smt_front - 1 != left)
                                {
                                  args[argc++]
                                      = sf_expr_gen (left, smt_front - 1);
                                  left = smt_front;
                                }

                              goto l1;
                            }

                          if (*op == ',' && !gb)
                            {
                              if (args == NULL)
                                {
                                  args = SFMALLOC (sizeof (*args));
                                }
                              else
                                {
                                  args = SFREALLOC (
                                      args, (argc + 1) * sizeof (*args));
                                }

                              args[argc++] = sf_expr_gen (left, smt_front - 1);
                              left = smt_front;
                            }

                          if (strstr ("({[", op) != NULL)
                            gb++;

                          if (strstr (")}]", op) != NULL)
                            gb--;
                        }
                        break;

                      default:
                        break;
                      }
                  }

              l1:;
                // printf ("%d\n", argc);

                // sf_token_print (t);
                assert (t.type == TOK_OPERATOR
                        && t.v.t_operator.value[0] == ')');

                stmt_t st;
                st.type = STMT_FUNCALL;
                st.v.s_funcall.name = sf_expr_gen (smt_back, smtv);
                st.v.s_funcall.args = args;
                st.v.s_funcall.argc = argc;

                res->vals[res->vl++] = st;
                smtv = smt_front - 1;
              }
          }
          break;

        case TOK_KEYWORD:
          {
            const char *kw = tok.v.t_keyword.value;

            if (!strcmp (kw, "if"))
              {
                size_t tb = get_tbsp (smtv, smt->vals);
                // sf_token_print (*smtv);
                // D (printf ("tabspace: %lu\n", tb));

                token_t *l1 = smtv;
                token_t *l2 = smtv;

                int gb = 0;
                while (l2->type != TOK_EOF)
                  {
                    token_t t = *l2++;

                    if (t.type == TOK_OPERATOR)
                      {
                        const char *op = t.v.t_operator.value;

                        if (strstr ("({[", op) != NULL)
                          gb++;

                        if (strstr (")}]", op) != NULL)
                          gb--;
                      }

                    if (t.type == TOK_NEWLINE && !gb)
                      break;
                  }

                expr_t *cond = sf_expr_gen (l1, --l2);
                token_t *block_end = get_block (++l2, tb);

                TokenSM tsmt;
                tsmt.vals = l2;
                tsmt.vl = block_end - l2;

                // for (int i = 0; i < tsmt.vl; i++)
                //   sf_token_print (tsmt.vals[i]);

                // printf ("--------------\n");

                block_end++;
                token_t bep = *block_end;
                block_end->type = TOK_EOF;

                StmtSM *body_smt = sf_ast_gen (&tsmt);
                *block_end = bep;

                while (block_end->type != TOK_EOF)
                  {
                    if (block_end->type == TOK_NEWLINE
                        || block_end->type == TOK_SPACE)
                      ;
                    else
                      break;

                    block_end++;
                  }

                stmt_t st;
                st.type = STMT_IFBLOCK;
                st.v.s_ifblock.body = body_smt->vals;
                st.v.s_ifblock.bl = body_smt->vl;
                st.v.s_ifblock.cond = cond;
                st.v.s_ifblock.else_bl = 0;
                st.v.s_ifblock.else_body = NULL;
                smtv = block_end;

                if (block_end->type == TOK_KEYWORD
                    && !strcmp (block_end->v.t_identifier.value, "else"))
                  {
                    token_t *ebe = get_block (++block_end, tb);
                    // sf_token_print (*ebe);

                    TokenSM tsmt;
                    tsmt.vals = block_end;
                    tsmt.vl = ebe - block_end;

                    // for (int i = 0; i < tsmt.vl; i++)
                    //   sf_token_print (tsmt.vals[i]);

                    ebe++;
                    token_t bep = *ebe;
                    ebe->type = TOK_EOF;

                    StmtSM *eb_smt = sf_ast_gen (&tsmt);
                    *ebe = bep;

                    st.v.s_ifblock.else_body = eb_smt->vals;
                    st.v.s_ifblock.else_bl = eb_smt->vl;
                    smtv = ebe;
                  }

                if (cond->type == EXPR_CONST)
                  {
                    if (sf_expr_tobool (*cond))
                      {
                        stmt_t *ifb = st.v.s_ifblock.body;
                        size_t ifbl = st.v.s_ifblock.bl;

                        for (int i = 0; i < ifbl; i++)
                          {
                            if (ifb[i].type == STMT_EOF)
                              break;

                            if (res->vl >= res->vc)
                              {
                                res->vc += STMT_SM_VALS_CAP;
                                res->vals = SFREALLOC (
                                    res->vals, res->vc * sizeof (*res->vals));
                              }

                            res->vals[res->vl++] = ifb[i];
                          }
                      }
                    else
                      {
                        stmt_t *elb = st.v.s_ifblock.else_body;
                        size_t elbl = st.v.s_ifblock.else_bl;

                        for (int i = 0; i < elbl; i++)
                          {
                            if (elb[i].type == STMT_EOF)
                              break;

                            if (res->vl >= res->vc)
                              {
                                res->vc += STMT_SM_VALS_CAP;
                                res->vals = SFREALLOC (
                                    res->vals, res->vc * sizeof (*res->vals));
                              }

                            res->vals[res->vl++] = elb[i];
                          }
                      }

                    SFFREE (cond);
                  }
                else
                  res->vals[res->vl++] = st;
                // for (int i = 0; i < body_smt->vl; i++)
                //   sf_stmt_print (body_smt->vals[i]);
              }
            else if (!strcmp (kw, "while"))
              {
                size_t tb = get_tbsp (smtv, smt->vals);
                // sf_token_print (*smtv);
                // D (printf ("tabspace: %lu\n", tb));

                token_t *l1 = smtv;
                token_t *l2 = smtv;

                int gb = 0;
                while (l2->type != TOK_EOF)
                  {
                    token_t t = *l2++;

                    if (t.type == TOK_OPERATOR)
                      {
                        const char *op = t.v.t_operator.value;

                        if (strstr ("({[", op) != NULL)
                          gb++;

                        if (strstr (")}]", op) != NULL)
                          gb--;
                      }

                    if (t.type == TOK_NEWLINE && !gb)
                      break;
                  }

                expr_t *cond = sf_expr_gen (l1, --l2);
                token_t *block_end = get_block (++l2, tb);

                TokenSM tsmt;
                tsmt.vals = l2;
                tsmt.vl = block_end - l2;

                // for (int i = 0; i < tsmt.vl; i++)
                //   sf_token_print (tsmt.vals[i]);

                // printf ("--------------\n");

                block_end++;
                token_t bep = *block_end;
                block_end->type = TOK_EOF;

                StmtSM *body_smt = sf_ast_gen (&tsmt);
                *block_end = bep;

                while (block_end->type != TOK_EOF)
                  {
                    if (block_end->type == TOK_NEWLINE
                        || block_end->type == TOK_SPACE)
                      ;
                    else
                      break;

                    block_end++;
                  }

                stmt_t st;
                st.type = STMT_WHILE;
                st.v.s_while.cond = cond;
                st.v.s_while.body = body_smt->vals;
                st.v.s_while.bl = body_smt->vl;

                smtv = block_end;
                res->vals[res->vl++] = st;
              }
          }
          break;

        default:
          break;
        }

      tok = *smtv++;
    }

  stmt_t st;
  st.type = STMT_EOF;

  if (res->vl >= res->vc)
    {
      res->vc += STMT_SM_VALS_CAP;
      res->vals = SFREALLOC (res->vals, res->vc * sizeof (*res->vals));
    }

  res->vals[res->vl++] = st;

  return res;
}

SF_API expr_t *
sf_expr_gen (token_t *start, token_t *end)
{
  expr_t e;

  // token_t *stt = start;
  // while (stt != end)
  //   {
  //     sf_token_print (*stt);
  //     stt++;
  //   }

  // printf ("------\n");

  token_t t;
  while (start != end)
    {
      t = *start++;
      // sf_token_print (t);

      switch (t.type)
        {
        case TOK_IDENTIFIER:
          {
            const char *id = t.v.t_identifier.value;

            e.type = EXPR_VAR;
            e.v.e_var.v = SFSTRDUP (id);
          }
          break;

        case TOK_INTEGER:
          {
            e.type = EXPR_CONST;
            e.v.e_const.v.type = CONST_INT;
            e.v.e_const.v.v.c_int.v = t.v.t_integer.value;
          }
          break;

        case TOK_FLOAT:
          {
            e.type = EXPR_CONST;
            e.v.e_const.v.type = CONST_FLOAT;
            e.v.e_const.v.v.c_float.v = t.v.t_float.value;
          }
          break;

        case TOK_STRING:
          {
            e.type = EXPR_CONST;
            e.v.e_const.v.type = CONST_STRING;
            e.v.e_const.v.v.c_str.v = SFSTRDUP (t.v.t_string.value);
          }
          break;

        case TOK_BOOL:
          {
            e.type = EXPR_CONST;
            e.v.e_const.v.type = CONST_BOOL;
            e.v.e_const.v.v.c_bool.v = t.v.t_bool.value;
          }
          break;

        case TOK_NONE:
          {
            e.type = EXPR_CONST;
            e.v.e_const.v.type = CONST_NONE;
          }
          break;

        case TOK_OPERATOR:
          {
            const char *op = t.v.t_operator.value;

            if (strstr ("+-*/%", (char *)(char[]){ *op, '\0' })
                && op[1] == '\0')
              {
                size_t nc = 8;
                size_t nl = 0;

                arith_node_t *nodes = SFMALLOC (nc * sizeof (*nodes));

                expr_t *em = SFMALLOC (sizeof (*em));
                *em = e;
                nodes[nl++]
                    = (arith_node_t){ .type = ARITH_NODE_E_O, .v.expr = em };

                nodes[nl++] = (arith_node_t){ .type = ARITH_NODE_OPERATOR,
                                              .v.op = (char *)op };

                int gb = 0;
                int all_consts = nodes[0].v.expr->type == EXPR_CONST;

                token_t *l1 = start;
                t = *start;

                while (start != end)
                  {
                    // sf_token_print (t);
                    if (t.type == TOK_OPERATOR)
                      {
                        const char *op = t.v.t_operator.value;

                        if (strstr ("({[", op) != NULL)
                          gb++;

                        if (strstr (")}]", op) != NULL)
                          gb--;

                        if (strstr ("+-*/%", (char *)(char[]){ *op, '\0' })
                            && op[1] == '\0' && !gb)
                          {
                            if (nl >= nc)
                              {
                                nc += 8;
                                nodes
                                    = SFREALLOC (nodes, nc * sizeof (*nodes));
                              }

                            nodes[nl++]
                                = (arith_node_t){ .type = ARITH_NODE_E_O,
                                                  .v.expr
                                                  = sf_expr_gen (l1, start) };

                            if (nodes[nl - 1].v.expr->type != EXPR_CONST)
                              all_consts = 0;

                            l1 = start + 1;

                            nodes[nl++]
                                = (arith_node_t){ .type = ARITH_NODE_OPERATOR,
                                                  .v.op = (char *)op };
                          }
                      }

                    t = *++start;
                  }

                // here;

                if (nl >= nc)
                  {
                    nc += 8;
                    nodes = SFREALLOC (nodes, nc * sizeof (*nodes));
                  }

                nodes[nl++]
                    = (arith_node_t){ .type = ARITH_NODE_E_O,
                                      .v.expr = sf_expr_gen (l1, start) };

                if (nodes[nl - 1].v.expr->type != EXPR_CONST)
                  all_consts = 0;

                // for (int i = 0; i < nl; i++)
                //   sf_arithnode_print (nodes[i]);

                /* add 1 */
                if (!all_consts && nl == 3
                    && nodes[1].type == ARITH_NODE_OPERATOR
                    && !strcmp (nodes[1].v.op, "+")
                    && nodes[2].type == ARITH_NODE_E_O
                    && nodes[2].v.expr->type == EXPR_CONST
                    && nodes[2].v.expr->v.e_const.v.type == CONST_INT
                    && nodes[2].v.expr->v.e_const.v.v.c_int.v == 1)
                  {
                    e.type = EXPR_ADD_1;
                    e.v.e_add_one.v = nodes[0].v.expr;

                    SFFREE (nodes);
                  }
                else if (!all_consts && nl == 3
                         && nodes[1].type == ARITH_NODE_OPERATOR
                         && !strcmp (nodes[1].v.op, "+")
                         && nodes[0].type == ARITH_NODE_E_O
                         && nodes[0].v.expr->type == EXPR_CONST
                         && nodes[0].v.expr->v.e_const.v.type == CONST_INT
                         && nodes[0].v.expr->v.e_const.v.v.c_int.v == 1)
                  {
                    e.type = EXPR_ADD_1;
                    e.v.e_add_one.v = nodes[2].v.expr;

                    SFFREE (nodes);
                  }
                else
                  {
                    /* make expression tree */
                    sf_arith_makepostfix (&nodes, nl);

                    if (all_consts)
                      {

                        expr_t _e = sf_arith_eval_consttree (nodes, nl);

                        for (int i = 0; i < nl; i++)
                          if (nodes[i].type == ARITH_NODE_E_O)
                            SFFREE (nodes[i].v.expr);

                        SFFREE (nodes);
                        // sf_expr_print (_e);

                        e = _e;
                      }
                    else
                      {
                        assert (nl < 64 && "too big arithmetic expression");

                        e.type = EXPR_ARITHMETIC;
                        e.v.e_arith.tree = nodes;
                        e.v.e_arith.tl = nl;
                        // sf_arith_print_tree (nodes, nl);
                      }
                  }

                start = end;
                goto end;
              }
          }
          break;

        default:
          break;
        }
    }

end:
  expr_t *ee = SFMALLOC (sizeof (*ee));
  *ee = e;
  return ee;
}