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
  size_t _pres_slot = (vm)->meta.slot;

#define RESTORE(vm)                                                           \
  vm->meta.g_slot = _pres_g_slot;                                             \
  vm->meta.l_slot = _pres_l_slot;                                             \
  vm->meta.slot = _pres_slot;

static vval_t *
get_var (vm_t *vm, const char *name)
{
  int gt = 0;
  vval_t *v = sf_ht_get (vm->ht, name, &gt);

  if (!gt)
    return NULL;

  return v;
}

static vval_t *
add_var (vm_t *vm, const char *name)
{
  vval_t *v = get_var (vm, name);

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

  sf_ht_insert (vm->ht, name, (void *)v);
  return v;
}

SF_API void
sf_vm_gen_b_fromexpr (vm_t *vm, expr_t e)
{
  switch (e.type)
    {
    case EXPR_VAR:
      {
        vval_t *v = get_var (vm, e.v.e_var.v);

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
            add_inst (vm, (instr_t){
                              .op = OP_LOAD_FAST,
                              .a = v->pos,
                              .b = 0,
                          });
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

                  add_inst (vm, (instr_t){
                                    .op = OP_STORE,
                                    .a = v->pos,
                                    .b = v->slot,
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
              sf_vm_gen_b_fromexpr (vm, *args[i]);

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

            instr_t *p = &vm->insts[vm->inst_len - 1];

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

            instr_t *q = &vm->insts[vm->inst_len - 1];

            smt.vals = else_body;
            smt.vl = smt.vc = ebl;

            p->a = vm->inst_len;

            sf_vm_gen_bytecode (vm, &smt);
            vm->inst_len--; // eat return

            q->a = vm->inst_len;
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

            instr_t *p = &vm->insts[vm->inst_len - 1];

            StmtSM smt;
            smt.vals = body;
            smt.vc = smt.vl = bl;

            sf_vm_gen_bytecode (vm, &smt);
            vm->inst_len--; // eat return

            add_inst (vm, (instr_t){
                              .op = OP_JUMP,
                              .a = vl,
                              .b = 0,
                          });

            p->a = vm->inst_len;
          }
          break;

        default:
          break;
        }
    }

end:;

  add_inst (vm, (instr_t){
                    .op = OP_RETURN,
                    .a = 0,
                    .b = 0,
                });
}