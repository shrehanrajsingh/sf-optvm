#include "codegen.h"

static int
_obj_tobool (obj_t *o)
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

void
add_inst (vm_t *vm, instr_t i)
{
  if (vm->inst_len >= vm->inst_cap)
    {
      vm->inst_cap += 64;
      vm->insts = SFREALLOC (vm->insts, vm->inst_cap * sizeof (*vm->insts));
    }

  vm->insts[vm->inst_len++] = i;
}

hashtable_t *
push_ht (vm_t *vm)
{
  if (vm->htl >= vm->htc)
    {
      vm->htc += SF_VM_HT_CAP;
      vm->hts = SFREALLOC (vm->hts, vm->htc * sizeof (*vm->hts));
    }

  vm->hts[vm->htl] = sf_ht_new ();
  return vm->hts[vm->htl++];
}

void
pop_ht (vm_t *vm)
{
  if (!vm->htl)
    return;

  sf_ht_free (vm->hts[--vm->htl]);
}

int
const_eqeq (const_t c, const_t d)
{
  if (c.type != d.type)
    return 0;

  switch (c.type)
    {
    case CONST_INT:
      return c.v.c_int.v == d.v.c_int.v;
      break;

    case CONST_BOOL:
      return c.v.c_bool.v == d.v.c_bool.v;
      break;

    case CONST_FLOAT:
      return c.v.c_float.v == d.v.c_float.v;
      break;

    case CONST_NONE:
      return 1;
      break;

    case CONST_STRING:
      return !strcmp (c.v.c_str.v, d.v.c_str.v);
      break;

    default:
      break;
    }

  return 0;
}

#define PRESERVE(vm)                                                          \
  size_t _pres_g_slot = (vm)->meta.g_slot;                                    \
  size_t _pres_l_slot = (vm)->meta.l_slot;                                    \
  size_t _pres_n_slot = (vm)->meta.n_slot;                                    \
  size_t _pres_slot = (vm)->meta.slot;

#define RESTORE(vm)                                                           \
  vm->meta.g_slot = _pres_g_slot;                                             \
  vm->meta.l_slot = _pres_l_slot;                                             \
  vm->meta.n_slot = _pres_n_slot;                                             \
  vm->meta.slot = _pres_slot;

static vval_t *
get_var (vm_t *vm, const char *name, int *level_ptr)
{
  int gt = 0;
  vval_t *v = sf_ht_get (vm->hts[vm->htl - 1], name, &gt);
  int l = vm->htl - 1;
  int level = 0;

  while (!gt)
    {
      if (l < 1)
        break;

      v = sf_ht_get (vm->hts[--l], name, &gt);
      level++;
    }

  if (level_ptr != NULL)
    *level_ptr = level;

  return v;
}

static vval_t *
get_var_look_top (vm_t *vm, const char *name)
{
  int gt = 0;
  vval_t *v = sf_ht_get (vm->hts[vm->htl - 1], name, &gt);

  if (!gt)
    return NULL;

  return v;
}

static vval_t *
add_var (vm_t *vm, const char *name)
{
  vval_t *v = get_var_look_top (vm, name);

  if (v != NULL)
    {
      return v;
    }

  v = SFMALLOC (sizeof (*v));
  v->slot = vm->meta.slot;

  if (v->slot == SF_VM_SLOT_GLOBAL)
    {
      v->pos = vm->meta.g_slot++;
    }
  else if (v->slot == SF_VM_SLOT_LOCAL)
    {
      v->pos = vm->meta.l_slot++;
    }
  else if (v->slot == SF_VM_SLOT_NAME)
    {
      v->pos = vm->meta.n_slot++;
    }

  sf_ht_insert (vm->hts[vm->htl - 1], name, (void *)v);
  return v;
}

SF_API void
sf_vm_gen_b_fromexpr (vm_t *vm, expr_t e)
{
  switch (e.type)
    {
    case EXPR_VAR:
      {
        int lev = 0;
        vval_t *v = get_var (vm, e.v.e_var.v, &lev);

        if (v == NULL)
          {
            printf ("undefined variable: %s\n", e.v.e_var.v);
            exit (1);
          }

        if (v->slot == SF_VM_SLOT_GLOBAL)
          {
            add_inst (vm, (instr_t){
                              .op = OP_LOAD,
                              .a = v->pos,
                              .b = 0,
                          });
          }
        else if (v->slot == SF_VM_SLOT_LOCAL)
          {
            add_inst (
                vm,
                (instr_t){
                    .op = OP_LOAD_FAST,
                    .a = v->pos,
                    .b = lev, /* how many frames up can we find the variable */
                });
          }
        else if (v->slot == SF_VM_SLOT_NAME)
          {
            add_inst (vm, (instr_t){ .op = OP_LOAD_NAME,
                                     .a = v->pos,
                                     .b = lev,
                                     .c = (char *)e.v.e_var.v });
          }
      }
      break;

    case EXPR_CONST:
      {
        size_t j;
        int found = 0;
        const_t d = e.v.e_const.v;

        for (int i = 0; i < vm->s_ml; i++)
          {
            if (const_eqeq (vm->map_consts[i], d))
              {
                found = 1;
                j = i;
                break;
              }
          }

        if (found)
          {
            add_inst (vm, (instr_t){
                              .op = OP_LOAD_CONST,
                              .a = j,
                              .b = 0,
                          });
          }
        else
          {
            if (vm->s_ml >= vm->s_mc)
              {
                vm->s_mc += 64;
                vm->map_consts = SFREALLOC (
                    vm->map_consts, vm->s_mc * sizeof (*vm->map_consts));
              }

            vm->map_consts[vm->s_ml++] = d;

            add_inst (vm, (instr_t){
                              .op = OP_LOAD_CONST,
                              .a = vm->s_ml - 1,
                              .b = 0,
                          });
          }
      }
      break;

    case EXPR_ADD_1:
      {
        sf_vm_gen_b_fromexpr (vm, *e.v.e_add_one.v);

        add_inst (vm, (instr_t){
                          .op = OP_ADD_1,
                          .a = 0,
                          .b = 0,
                      });
      }
      break;

    case EXPR_ARITHMETIC:
      {
        size_t tl = e.v.e_arith.tl;
        arith_node_t *tree = e.v.e_arith.tree;
        size_t x = 0, y = 0;

        for (size_t i = 0; i < tl; i++)
          {
            arith_node_t n = tree[i];

            if (n.type == ARITH_NODE_OPERATOR)
              {
                y = i;
                while (x < y)
                  {
                    sf_vm_gen_b_fromexpr (vm, *tree[x].v.expr);
                    x++;
                  }

                const char *op = n.v.op;

                if (*op == '+')
                  {
                    add_inst (vm, (instr_t){
                                      .op = OP_ADD,
                                      .a = 0,
                                      .b = 0,
                                  });
                  }
                else if (*op == '-')
                  {
                    add_inst (vm, (instr_t){
                                      .op = OP_SUB,
                                      .a = 0,
                                      .b = 0,
                                  });
                  }
                else if (*op == '*')
                  {
                    add_inst (vm, (instr_t){
                                      .op = OP_MUL,
                                      .a = 0,
                                      .b = 0,
                                  });
                  }

                x++;
              }
          }
      }
      break;

    case EXPR_FUNCALL:
      {
        size_t al = e.v.e_funcall.al;
        expr_t **args = e.v.e_funcall.args;
        expr_t *name = e.v.e_funcall.name;

        for (size_t i = 0; i < al; i++)
          sf_vm_gen_b_fromexpr (vm, *args[i]);

        sf_vm_gen_b_fromexpr (vm, *name);

        add_inst (vm, (instr_t){
                          .op = OP_CALL,
                          .a = al,
                          .b = 1, /* 1 means push the return value to stack */
                      });
      }
      break;

    case EXPR_CMP:
      {
        sf_vm_gen_b_fromexpr (vm, *e.v.e_cmp.left);
        sf_vm_gen_b_fromexpr (vm, *e.v.e_cmp.right);

        add_inst (vm, (instr_t){ .op = OP_CMP, .a = e.v.e_cmp.type, .b = 0 });
      }
      break;

    case EXPR_DOT_ACCESS:
      {
        sf_vm_gen_b_fromexpr (vm, *e.v.e_dota.left);

        add_inst (vm, (instr_t){ .op = OP_DOT_ACCESS,
                                 .a = 0,
                                 .b = 0,
                                 .c = e.v.e_dota.right });
      }
      break;

    case EXPR_ARRAY:
      {
        for (size_t i = 0; i < e.v.e_array.vl; i++)
          {
            sf_vm_gen_b_fromexpr (vm, *e.v.e_array.vals[i]);
          }

        add_inst (vm, (instr_t){
                          .op = OP_LOAD_ARRAY,
                          .a = e.v.e_array.vl,
                          .b = 0,
                      });
      }
      break;

    case EXPR_SQUARE_ACCESS:
      {
        sf_vm_gen_b_fromexpr (vm, *e.v.e_sqr_access.parent);
        sf_vm_gen_b_fromexpr (vm, *e.v.e_sqr_access.idx);

        add_inst (vm, (instr_t){
                          .op = OP_SQR_ACCESS,
                          .a = 0,
                          .b = 0,
                      });
      }
      break;

    default:
      break;
    }
}

SF_API void
sf_vm_gen_bytecode (vm_t *vm, StmtSM *smt)
{
  stmt_t *s;
  while (1)
    {
      s = smt->vals++;

      switch (s->type)
        {
        case STMT_EOF:
          goto end;
          break;

        case STMT_VARDECL:
          {
            sf_vm_gen_b_fromexpr (vm, *s->v.s_vardecl.val);

            expr_t *name = s->v.s_vardecl.name;

            switch (name->type)
              {
              case EXPR_VAR:
                {
                  vval_t *v = add_var (vm, (char *)name->v.e_var.v);

                  if (v->slot == SF_VM_SLOT_LOCAL)
                    add_inst (vm, (instr_t){
                                      .op = OP_STORE_FAST,
                                      .a = v->pos,
                                      .b = 0,
                                  });
                  else if (v->slot == SF_VM_SLOT_GLOBAL)
                    add_inst (vm, (instr_t){
                                      .op = OP_STORE,
                                      .a = v->pos,
                                      .b = 0,
                                  });
                  else if (v->slot == SF_VM_SLOT_NAME)
                    add_inst (vm, (instr_t){ .op = OP_STORE_NAME,
                                             .a = v->pos,
                                             .b = 0,
                                             .c = (char *)name->v.e_var.v });
                }
                break;
              case EXPR_DOT_ACCESS:
                {
                  sf_vm_gen_b_fromexpr (vm, *name->v.e_dota.left);

                  add_inst (vm,
                            (instr_t){ .op = OP_STORE_NAME,
                                       .a = 0,
                                       .b = 1,
                                       .c = (char *)name->v.e_dota.right });
                }
                break;

              case EXPR_SQUARE_ACCESS:
                {
                  sf_vm_gen_b_fromexpr (vm, *name->v.e_sqr_access.idx);
                  sf_vm_gen_b_fromexpr (vm, *name->v.e_sqr_access.parent);

                  add_inst (vm, (instr_t){
                                    .op = OP_STORE_SQR,
                                    .a = 0,
                                    .b = 0,
                                });
                }
                break;

              default:
                break;
              }
          }
          break;

        case STMT_FUNCALL:
          {
            size_t argc = s->v.s_funcall.argc;
            expr_t **args = s->v.s_funcall.args;
            expr_t *name = s->v.s_funcall.name;

            for (size_t i = 0; i < argc; i++)
              {
                // sf_expr_print (*args[i]);
                sf_vm_gen_b_fromexpr (vm, *args[i]);
              }

            sf_vm_gen_b_fromexpr (vm, *name);
            add_inst (vm, (instr_t){
                              .op = OP_CALL,
                              .a = argc,
                              .b = 0,
                          });
          }
          break;

        case STMT_IFBLOCK:
          {
            size_t bl = s->v.s_ifblock.bl;
            stmt_t *body = s->v.s_ifblock.body;
            expr_t *cond = s->v.s_ifblock.cond;
            size_t ebl = s->v.s_ifblock.else_bl;
            stmt_t *else_body = s->v.s_ifblock.else_body;

            sf_vm_gen_b_fromexpr (vm, *cond);

            add_inst (vm, (instr_t){
                              .op = OP_JUMP_IF_FALSE,
                              .a = 0,
                              .b = 0,
                          });

            size_t pl = vm->inst_len - 1;

            StmtSM smt;
            smt.vals = body;
            smt.vl = smt.vc = bl;

            sf_vm_gen_bytecode (vm, &smt);
            vm->inst_len--; // eat return

            add_inst (vm, (instr_t){
                              .op = OP_JUMP,
                              .a = 0,
                              .b = 0,
                          });

            size_t ql = vm->inst_len - 1;
            vm->insts[pl] = (instr_t){
              .op = OP_JUMP_IF_FALSE,
              .a = vm->inst_len,
              .b = 0,
            };

            if (ebl)
              {
                smt.vals = else_body;
                smt.vl = smt.vc = ebl;

                sf_vm_gen_bytecode (vm, &smt);
                vm->inst_len--; // eat return
              }

            vm->insts[ql] = (instr_t){
              .op = OP_JUMP,
              .a = vm->inst_len,
              .b = 0,
            };
          }
          break;

        case STMT_WHILE:
          {
            size_t bl = s->v.s_while.bl;
            stmt_t *body = s->v.s_while.body;
            expr_t *cond = s->v.s_while.cond;

            size_t vl = vm->inst_len;

            // write condition to check
            sf_vm_gen_b_fromexpr (vm, *cond);

            // jump to end of loop if condition is false
            add_inst (vm, (instr_t){
                              .op = OP_JUMP_IF_FALSE,
                              .a = 0,
                              .b = 0,
                          });

            size_t il = vm->inst_len - 1;
            /**
             * ! DO NOT TAKE REFERENCE OF LAST INSTRUCTION
             * ! IF vm->insts IS REALLOCED THEN REFERENCES BECOME
             * ! MEANINGLESS
             */

            StmtSM smt;
            smt.vals = body;
            smt.vc = smt.vl = bl;

            here;
            for (int i = 0; i < bl; i++)
              sf_stmt_print (body[i]);
            here;

            sf_vm_gen_bytecode (vm, &smt);

            vm->inst_len--; // eat return

            // D (printf ("%d\n", vl));
            add_inst (vm, (instr_t){
                              .op = OP_JUMP,
                              .a = vl,
                              .b = 0,
                          });

            // D (printf ("%d\n", vm->inst_len));
            vm->insts[il] = (instr_t){
              .op = OP_JUMP_IF_FALSE,
              .a = vm->inst_len,
              .b = 0,
            };
            // D (printf ("%d %d\n", p->a, p->op));
          }
          break;

        case STMT_FUNDECL:
          {
            size_t argc = s->v.s_fundecl.argc;
            expr_t **args = s->v.s_fundecl.args;
            size_t bl = s->v.s_fundecl.bl;
            stmt_t *body = s->v.s_fundecl.body;
            const char *name = s->v.s_fundecl.name;

            vval_t *nl = add_var (vm, name);

            PRESERVE (vm);

            // hashtable_t *ht = sf_ht_new ();
            // hashtable_t *ht_pres = vm->ht;
            // vm->ht = ht;

            vval_t *vlt[128];
            size_t vltc = 0;

            push_ht (vm);

            vm->meta.slot = SF_VM_SLOT_LOCAL;
            for (size_t i = 0; i < argc; i++)
              {
                expr_t *arg = args[i];

                if (arg->type == EXPR_VAR)
                  {
                    const char *n = arg->v.e_var.v;

                    vlt[vltc++] = add_var (vm, n);
                  }
              }

            StmtSM smt;
            smt.vals = body;
            smt.vl = smt.vc = bl;

            add_inst (vm, (instr_t){
                              .op = OP_JUMP,
                              .a = 0,
                              .b = 0,
                          });

            size_t pl = vm->inst_len - 1;
            size_t ql = vm->inst_len;

            for (size_t i = 0; i < vltc; i++)
              {
                add_inst (vm, (instr_t){
                                  .op = OP_STORE_FAST,
                                  .a = vlt[i]->pos,
                                  .b = 0,
                              });
              }

            sf_vm_gen_bytecode (vm, &smt);
            /* no eating return here, because we need return */

            pop_ht (vm);

            vm->insts[pl] = (instr_t){
              .op = OP_JUMP,
              .a = vm->inst_len,
              .b = 0,
            };

            add_inst (vm, (instr_t){
                              .op = OP_LOAD_FUNC_CODED,
                              .a = ql,
                              .b = argc,
                          });

            // vm->ht = ht_pres;
            // sf_ht_free (ht);

            RESTORE (vm);

            if (nl->slot == SF_VM_SLOT_LOCAL)
              add_inst (vm, (instr_t){
                                .op = OP_STORE_FAST,
                                .a = nl->pos,
                                .b = 0,
                            });
            else if (nl->slot == SF_VM_SLOT_GLOBAL)
              add_inst (vm, (instr_t){
                                .op = OP_STORE,
                                .a = nl->pos,
                                .b = 0,
                            });
            else if (nl->slot == SF_VM_SLOT_NAME)
              add_inst (vm, (instr_t){
                                .op = OP_STORE_NAME,
                                .a = nl->pos,
                                .b = 0,
                                .c = (char *)name,
                            });
          }
          break;

        case STMT_RETURN:
          {
            sf_vm_gen_b_fromexpr (vm, *s->v.s_return.v);

            add_inst (vm,
                      (instr_t){
                          .op = OP_RETURN,
                          .a = 1, /* 1 means the return is coded by the user */
                          .b = 0,
                      });
          }
          break;

        case STMT_CLASSDECL:
          {
            size_t bl = s->v.s_classdecl.bl;
            stmt_t *body = s->v.s_classdecl.body;
            const char *name = s->v.s_classdecl.name;

            add_inst (vm, (instr_t){
                              .op = OP_LOAD_BUILDCLASS,
                              .a = 0, /* the corresponding LOAD_BUILDEND */
                              .b = 0,
                              .c = (char *)name,
                          });

            size_t il = vm->inst_len - 1;

            StmtSM csm;
            csm.vals = body;
            csm.vl = csm.vc = bl;

            PRESERVE (vm);

            push_ht (vm);
            vm->meta.slot = SF_VM_SLOT_NAME;

            sf_vm_gen_bytecode (vm, &csm);
            vm->inst_len--;

            pop_ht (vm);

            RESTORE (vm);

            vval_t *nl = add_var (vm, name);

            add_inst (vm, (instr_t){
                              .op = OP_LOAD_BUILDCLASS_END,
                              .a = il, /* the corresponding LOAD_BUILDCLASS */
                              .b = 0,
                          });

            vm->insts[il] = (instr_t){
              .op = OP_LOAD_BUILDCLASS,
              .a = vm->inst_len - 1, /* the corresponding LOAD_BUILDEND */
              .b = 0,
              .c = (char *)name,
            };

            if (vm->meta.slot == SF_VM_SLOT_GLOBAL)
              add_inst (vm, (instr_t){
                                .op = OP_STORE,
                                .a = nl->pos,
                                .b = 0,
                            });
            else if (vm->meta.slot == SF_VM_SLOT_LOCAL)
              add_inst (vm, (instr_t){
                                .op = OP_STORE_FAST,
                                .a = nl->pos,
                                .b = 0,
                            });
            else if (vm->meta.slot == SF_VM_SLOT_NAME)
              add_inst (vm, (instr_t){
                                .op = OP_STORE_NAME,
                                .a = nl->pos,
                                .b = 0,
                                .c = (char *)name,
                            });
          }
          break;

        default:
          break;
        }
    }

end:;

  add_inst (vm, (instr_t){
                    .op = OP_RETURN,
                    .a = 0, /* 0 means the user hasnt written a return
                               anywhere, but the routine has to end somehow */
                    .b = 0,
                });
}