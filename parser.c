#include "parser.h"

void _exec_vardecl (const char *, expr_t *, mod_t *);
void _exec_funcall (expr_t *, expr_t **, size_t, mod_t *);
void _exec_ifblock (expr_t *, stmt_t *, size_t, stmt_t *, size_t, mod_t *);

static int _obj_tobool (obj_t *, mod_t *);

void _eval_const (const_t *, obj_t *);

SF_API void
sf_parser_exec_one (mod_t *mod)
{
  stmt_t s = *mod->body->vals++;

  switch (s.type)
    {
    case STMT_VARDECL:
      {
        expr_t *name = s.v.s_vardecl.name;

        switch (name->type)
          {
          case EXPR_VAR:
            {
              _exec_vardecl (s.v.s_vardecl.name->v.e_var.v, s.v.s_vardecl.val,
                             mod);
            }
            break;

          default:
            break;
          }
      }
      break;

    case STMT_FUNCALL:
      {
        _exec_funcall (s.v.s_funcall.name, s.v.s_funcall.args,
                       s.v.s_funcall.argc, mod);
      }
      break;

    case STMT_IFBLOCK:
      {
        _exec_ifblock (s.v.s_ifblock.cond, s.v.s_ifblock.body,
                       s.v.s_ifblock.bl, s.v.s_ifblock.else_body,
                       s.v.s_ifblock.else_bl, mod);
      }
      break;

    default:
      break;
    }
}

SF_API void
sf_parser_exec (mod_t *mod)
{
  while (mod->body->vals->type != STMT_EOF)
    sf_parser_exec_one (mod);
}

SF_API obj_t *
sf_parser_eval_expr (expr_t *e, mod_t *mod)
{
  obj_t *res = NULL;

  switch (e->type)
    {
    case EXPR_CONST:
      {
        res = sf_objstore_req_forconst (&e->v.e_const.v);
        // res = sf_objstore_req ();

        if (res == NULL)
          {
            res = sf_objstore_req ();
            _eval_const (&e->v.e_const.v, res);
          }
      }
      break;

    case EXPR_VAR:
      {
        const char *n = e->v.e_var.v;

        int got = 0;
        res = sf_mod_getvar (mod, n, &got);

        if (!got)
          {
            D (printf ("variable not found: %s\n", n));
            exit (-1);
          }
      }
      break;

    case EXPR_ADD_1:
      {
        expr_t *eao = e->v.e_add_one.v;
        obj_t *o_eao = sf_parser_eval_expr (eao, mod);
        int done = 0;

        switch (o_eao->type)
          {
          case OBJ_CONST:
            {
              switch (o_eao->v.o_const.v.type)
                {
                case CONST_INT:
                  {
                    const_t c
                        = { .type = CONST_INT,
                            .v.c_int.v = o_eao->v.o_const.v.v.c_int.v + 1 };

                    obj_t *r = sf_objstore_req_forconst (&c);

                    if (r != NULL)
                      res = r;
                    else
                      {
                        res = sf_objstore_req ();
                        res->type = OBJ_CONST;
                        res->v.o_const.v = c;
                      }
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

        if (!done)
          {
            assert (0 && "TODO\n");
          }
      }
      break;

    case EXPR_ARITHMETIC:
      {
        arith_objnode_t ao[64];

        for (int i = 0; i < e->v.e_arith.tl; i++)
          {
            if (e->v.e_arith.tree[i].type == ARITH_NODE_OPERATOR)
              {
                ao[i] = (arith_objnode_t){ .type = ARITH_NODE_OPERATOR,
                                           .v.op = e->v.e_arith.tree[i].v.op };
              }
            else
              {
                ao[i] = (arith_objnode_t){
                  .type = ARITH_NODE_E_O,
                  .v = sf_parser_eval_expr (e->v.e_arith.tree[i].v.expr, mod)
                };
              }
          }

        res = sf_arith_eval_objtree (ao, e->v.e_arith.tl);

        for (int i = 0; i < e->v.e_arith.tl; i++)
          if (ao[i].type == ARITH_NODE_E_O)
            DR (ao[i].v.obj);
      }
      break;

    default:
      D (printf ("invalid expr: %d\n", e->type));
      exit (-1);
      break;
    }

  assert (res != NULL);
  IR (res);
  return res;
}

void
_exec_vardecl (const char *name, expr_t *val, mod_t *mod)
{
  obj_t *o_val = sf_parser_eval_expr (val, mod);
  sf_mod_addvar (mod, name, o_val);
  DR (o_val);
}

void
_exec_funcall (expr_t *name, expr_t **args, size_t argc, mod_t *mod)
{
#if defined(__STDC_VERSION__)
#if __STDC_VERSION__ >= 202311L
  static thread_local mod_t *__m = NULL;
#else
  static mod_t *__m = NULL;
#endif
#else
  static mod_t *__m = NULL;
#endif

  obj_t *o_name = sf_parser_eval_expr (name, mod);

  if (__m == NULL)
    __m = sf_mod_new (MOD_FILE, NULL);

  switch (o_name->type)
    {
    case OBJ_FUNC:
      {
        fun_t *f = o_name->v.o_fun.v;

        switch (f->type)
          {
          case FUN_NATIVE:
            {
              if (f->v.native.nf_type != NF_ARG_ANY)
                assert (argc == f->argl);

              int call_done = 0;

              if (f->v.native.scc != 0)
                {
                  switch (f->v.native.scc)
                    {
                    case (int)'W':
                      {
                        if (f->argl == 1)
                          {
                            if (args[0]->type == EXPR_CONST)
                              {
                                const_t *c = &args[0]->v.e_const.v;

                                switch (c->type)
                                  {
                                  case CONST_INT:
                                    {
                                      printf ("%d\n", c->v.c_int.v);
                                    }
                                    break;

                                  case CONST_FLOAT:
                                    {
                                      printf ("%f\n", c->v.c_float.v);
                                    }
                                    break;

                                  case CONST_STRING:
                                    {
                                      puts (c->v.c_str.v);
                                    }
                                    break;

                                  case CONST_BOOL:
                                    {
                                      puts (c->v.c_bool.v ? "true" : "false");
                                    }
                                    break;

                                  case CONST_NONE:
                                    {
                                      puts ("none");
                                    }
                                    break;

                                  default:
                                    break;
                                  }

                                call_done = 1;
                              }
                          }
                      }
                      break;

                    case (int)'w':
                      {
                        if (f->argl == 1)
                          {
                            if (args[0]->type == EXPR_CONST)
                              {
                                const_t *c = &args[0]->v.e_const.v;

                                switch (c->type)
                                  {
                                  case CONST_INT:
                                    {
                                      printf ("%d", c->v.c_int.v);
                                    }
                                    break;

                                  case CONST_FLOAT:
                                    {
                                      printf ("%f", c->v.c_float.v);
                                    }
                                    break;

                                  case CONST_STRING:
                                    {
                                      fputs (c->v.c_str.v, stdout);
                                    }
                                    break;

                                  case CONST_BOOL:
                                    {
                                      fputs (c->v.c_bool.v ? "true" : "false",
                                             stdout);
                                    }
                                    break;

                                  case CONST_NONE:
                                    {
                                      fputs ("none", stdout);
                                    }
                                    break;

                                  default:
                                    break;
                                  }

                                call_done = 1;
                              }
                          }
                      }
                      break;

                    default:
                      break;
                    }
                }

              if (!call_done)
                {
                  obj_t *fr = NULL;

                  switch (f->v.native.nf_type)
                    {
                    case NF_ARG_1:
                      {
                        obj_t *io = sf_parser_eval_expr (args[0], mod);
                        f->v.native.v.f_onearg (io);
                        DR (io);
                      }
                      break;

                    case NF_ARG_2:
                      {
                        obj_t *io_1 = sf_parser_eval_expr (args[0], mod);
                        obj_t *io_2 = sf_parser_eval_expr (args[1], mod);

                        f->v.native.v.f_twoarg (io_1, io_2);

                        DR (io_1);
                        DR (io_2);
                      }
                      break;

                    case NF_ARG_3:
                      {
                        obj_t *io_1 = sf_parser_eval_expr (args[0], mod);
                        obj_t *io_2 = sf_parser_eval_expr (args[1], mod);
                        obj_t *io_3 = sf_parser_eval_expr (args[2], mod);

                        f->v.native.v.f_threearg (io_1, io_2, io_3);

                        DR (io_1);
                        DR (io_2);
                        DR (io_3);
                      }
                      break;

                    case NF_ARG_ANY:
                      {
                        obj_t **ioargs = SFMALLOC (argc * sizeof (*ioargs));

                        for (int i = 0; i < argc; i++)
                          ioargs[i] = sf_parser_eval_expr (args[i], mod);

                        f->v.native.v.f_anyarg (ioargs, argc);

                        for (int i = 0; i < argc; i++)
                          DR (ioargs[i]);
                        SFFREE (ioargs);
                      }
                      break;

                    default:
                      break;
                    }

                  if (fr != NULL)
                    DR (fr);
                }
            }
            break;

          default:
            break;
          }
      }
      break;

    default:
      D (printf ("invalid callable: %d\n", o_name->type));
      exit (-1);
      break;
    }

  DR (o_name);
}

void
_exec_ifblock (expr_t *cond, stmt_t *body, size_t bl, stmt_t *else_body,
               size_t else_bl, mod_t *mod)
{
  stmt_t *mb_bpres = mod->body->vals;
  size_t mb_blpres = mod->body->vl;
  size_t mb_bcpres = mod->body->vc;

  obj_t *o_cond = sf_parser_eval_expr (cond, mod);

  if (_obj_tobool (o_cond, mod))
    {
      mod->body->vals = body;
      mod->body->vl = bl;
    }
  else
    {
      mod->body->vals = else_body;
      mod->body->vl = else_bl;
    }

  sf_parser_exec (mod);
  DR (o_cond);

  mod->body->vals = mb_bpres;
  mod->body->vl = mb_blpres;
  mod->body->vc = mb_bcpres;
}

void
_eval_const (const_t *c, obj_t *r)
{
  r->type = OBJ_CONST;
  r->v.o_const.v = *c;
}

static int
_obj_tobool (obj_t *o, mod_t *mod)
{
  int r = 0;

  switch (o->type)
    {
    case OBJ_CONST:
      {
        switch (o->v.o_const.v.type)
          {
          case CONST_INT:
            r = o->v.o_const.v.v.c_int.v != 0;
            break;

          case CONST_FLOAT:
            r = o->v.o_const.v.v.c_float.v != 0.0;
            break;

          case CONST_NONE:
            r = 0;
            break;

          case CONST_BOOL:
            r = o->v.o_const.v.v.c_bool.v;
            break;

          case CONST_STRING:
            r = o->v.o_const.v.v.c_str.v[0] != '\0';
            break;

          default:
            break;
          }
      }
      break;

    case OBJ_FUNC:
      r = 1;
      break;

    default:
      break;
    }

  return r;
}