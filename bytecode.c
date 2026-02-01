#include "bytecode.h"

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

size_t
add_const (vm_t *vm, const_t c)
{
  for (size_t i = 0; i < vm->s_ml; i++)
    {
      if (const_eqeq (c, vm->map_consts[i]))
        return i;
    }

  size_t p;

  if (vm->s_ml >= vm->s_mc)
    {
      vm->s_mc += 64;
      vm->map_consts
          = SFREALLOC (vm->map_consts, vm->s_mc * sizeof (*vm->map_consts));
    }

  p = vm->s_ml++;
  vm->map_consts[p] = c;

  return p;
}

size_t
add_var_global (vm_t *vm, const char *var)
{
  if (vm->gnext_slot >= vm->globals_cap)
    {
      vm->globals_cap += SF_VM_GLOBALS_CAP;
      vm->globals
          = SFREALLOC (vm->globals, vm->globals_cap * sizeof (*vm->globals));

      for (int i = 0; i < vm->globals_cap; i++)
        vm->globals[i] = NULL;
    }

  size_t gn = vm->gnext_slot;
  vval_t *ggn = SFMALLOC (sizeof (*ggn));
  ggn->pos = gn;
  ggn->slot = SF_VM_SLOT_GLOBAL;

  sf_ht_insert (vm->ht, var, (void *)ggn);
  vm->gnext_slot++;

  return gn;
}

size_t
add_var_local (vm_t *vm, const char *var)
{
  frame_t *fr = &vm->frames[vm->fp - 1];

  if (fr->locals_count >= fr->locals_cap)
    {
      fr->locals_cap += SF_VM_GLOBALS_CAP;
      fr->locals
          = SFREALLOC (fr->locals, fr->locals_cap * sizeof (*fr->locals));

      for (int i = 0; i < fr->locals_cap; i++)
        fr->locals[i] = NULL;
    }

  size_t gn = fr->locals_count;
  vval_t *ggn = SFMALLOC (sizeof (*ggn));
  ggn->pos = gn;
  ggn->slot = SF_VM_SLOT_LOCAL;

  sf_ht_insert (vm->ht, var, (void *)ggn);
  fr->locals_count++;

  return gn;
}

SF_API vm_t
sf_vm_new ()
{
  vm_t v;

  v.globals_cap = SF_VM_GLOBALS_CAP;
  v.globals = SFMALLOC (v.globals_cap * sizeof (*v.globals));
  v.gnext_slot = 0;
  v.ht = sf_ht_new ();
  v.inst_cap = 64;
  v.inst_len = 0;
  v.insts = SFMALLOC (v.inst_cap * sizeof (*v.insts));
  v.ip = 0;
  v.s_mc = 64;
  v.s_ml = 0;
  v.map_consts = SFMALLOC (v.s_mc * sizeof (*v.map_consts));
  v.sp = 0;
  v.stack_cap = SF_VM_STACK_CAP;
  v.stack = SFMALLOC (v.stack_cap * sizeof (*v.stack));
  v.fp = 0;
  v.frame_cap = SF_VM_FRAME_CAP;
  v.frames = SFMALLOC (v.frame_cap * sizeof (*v.frames));

  for (int i = 0; i < v.globals_cap; i++)
    v.globals[i] = NULL;

  return v;
}

SF_API void
sf_vm_gen_b_fromexpr (vm_t *vm, expr_t e)
{
  int is_global = vm->fp == 1;
  int slot = is_global ? SF_VM_SLOT_GLOBAL : SF_VM_SLOT_LOCAL;
  //   D (printf ("%d\n", vm->fp));

  switch (e.type)
    {
    case EXPR_VAR:
      {
        int gr = 0;
        vval_t *v = sf_ht_get (vm->ht, e.v.e_var.v, &gr);
        size_t vs;

        if (!gr)
          {
            printf ("undeclared variable: %s\n", e.v.e_var.v);
            exit (1);
          }
        else
          vs = v->pos;

        add_inst (vm, (instr_t){
                          .op = OP_LOAD_VAR,
                          .a = (int)vs,
                          .b = v->slot,
                      });
      }
      break;

    case EXPR_ADD_1:
      {
        sf_vm_gen_b_fromexpr (vm, *e.v.e_add_one.v);

        add_inst (vm, (instr_t){
                          .op = OP_ADD_1,
                      });
      }
      break;

    case EXPR_CONST:
      {
        size_t ci = add_const (vm, e.v.e_const.v);

        add_inst (vm, (instr_t){
                          .op = OP_LOAD_CONST,
                          .a = (int)ci,
                          .b = 0,
                      });
      }
      break;

    case EXPR_ARITHMETIC:
      {
        struct arith_s *tree = e.v.e_arith.tree;
        size_t tl = e.v.e_arith.tl;

        expr_t *args[64];
        int ac = 0;

        for (int i = 0; i < tl; i++)
          {
            if (tree[i].type == ARITH_NODE_OPERATOR)
              {
                const char *op = tree[i].v.op;

                if (*op == '+')
                  {
                    for (int j = 0; j < ac; j++)
                      sf_vm_gen_b_fromexpr (vm, *args[j]);
                    ac = 0;

                    add_inst (vm, (instr_t){
                                      .op = OP_ADD,
                                      .a = 0,
                                      .b = 0,
                                  });
                  }
                else if (*op == '-')
                  {
                    for (int j = 0; j < ac; j++)
                      sf_vm_gen_b_fromexpr (vm, *args[j]);
                    ac = 0;

                    add_inst (vm, (instr_t){
                                      .op = OP_SUB,
                                      .a = 0,
                                      .b = 0,
                                  });
                  }
                else if (*op == '*')
                  {
                    for (int j = 0; j < ac; j++)
                      sf_vm_gen_b_fromexpr (vm, *args[j]);
                    ac = 0;

                    add_inst (vm, (instr_t){
                                      .op = OP_MUL,
                                      .a = 0,
                                      .b = 0,
                                  });
                  }
                else if (*op == '/')
                  {
                    for (int j = 0; j < ac; j++)
                      sf_vm_gen_b_fromexpr (vm, *args[j]);
                    ac = 0;

                    add_inst (vm, (instr_t){
                                      .op = OP_DIV,
                                      .a = 0,
                                      .b = 0,
                                  });
                  }
              }
            else
              args[ac++] = tree[i].v.expr;
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
  stmt_t st = *smt->vals++;
  int is_global = vm->fp < 2;
  int slot = is_global ? SF_VM_SLOT_GLOBAL : SF_VM_SLOT_LOCAL;

  while (st.type != STMT_EOF)
    {
      switch (st.type)
        {
        case STMT_VARDECL:
          {
            sf_vm_gen_b_fromexpr (vm, *st.v.s_vardecl.val);

            size_t vs;
            int sl = slot;
            if (st.v.s_vardecl.name->type == EXPR_VAR)
              {
                int gr = 0;
                const char *vname = st.v.s_vardecl.name->v.e_var.v;
                vval_t *v = sf_ht_get (vm->ht, vname, &gr);

                if (!gr)
                  {
                    if (is_global)
                      vs = add_var_global (vm, vname);
                    else
                      vs = add_var_local (vm, vname);
                  }
                else
                  {
                    vs = v->pos;
                    sl = v->slot;
                  }
              }
            else
              assert (0 && "TODO");

            add_inst (vm, (instr_t){
                              .op = OP_STORE,
                              .a = vs,
                              .b = sl,
                          });
          }
          break;

        case STMT_IFBLOCK:
          {
            size_t bl = st.v.s_ifblock.bl;
            stmt_t *body = st.v.s_ifblock.body;
            expr_t *cond = st.v.s_ifblock.cond;
            size_t else_bl = st.v.s_ifblock.else_bl;
            stmt_t *else_body = st.v.s_ifblock.else_body;

            sf_vm_gen_b_fromexpr (vm, *cond);

            add_inst (vm, (instr_t){
                              .op = OP_JUMP_IF_FALSE,
                              .a = 0,
                              .b = 0,
                          });

            instr_t *ist = &vm->insts[vm->inst_len - 1];

            sf_vm_gen_bytecode (vm, (StmtSM *)(StmtSM[]){ (StmtSM){
                                        .vals = body,
                                        .vl = bl,
                                        .vc = bl,
                                    } });

            vm->inst_len--; /* eat return */

            if (else_bl)
              {
                ist->a = vm->inst_len + 1;
                add_inst (vm, (instr_t){
                                  .op = OP_JUMP,
                                  .a = 0,
                                  .b = 0,
                              });

                instr_t *i2 = &vm->insts[vm->inst_len - 1];

                sf_vm_gen_bytecode (vm, (StmtSM *)(StmtSM[]){ (StmtSM){
                                            .vals = else_body,
                                            .vl = else_bl,
                                            .vc = else_bl,
                                        } });

                vm->inst_len--;
                i2->a = vm->inst_len;
              }
          }
          break;

        case STMT_WHILE:
          {
            expr_t *cond = st.v.s_while.cond;
            stmt_t *body = st.v.s_while.body;
            size_t bl = st.v.s_while.bl;

            size_t ct = vm->inst_len;

            sf_vm_gen_b_fromexpr (vm, *cond);

            add_inst (vm, (instr_t){
                              .op = OP_JUMP_IF_FALSE,
                              .a = 0,
                              .b = 0,
                          });

            instr_t *tj = &vm->insts[vm->inst_len - 1];

            sf_vm_gen_bytecode (vm, (StmtSM *)(StmtSM[]){ (StmtSM){
                                        .vals = body,
                                        .vl = bl,
                                        .vc = bl,
                                    } });

            vm->inst_len--;

            add_inst (vm, (instr_t){
                              .op = OP_JUMP,
                              .a = ct,
                              .b = 0,
                          });

            tj->a = vm->inst_len;
          }
          break;

        case STMT_FUNCALL:
          {
            size_t argc = st.v.s_funcall.argc;
            expr_t **args = st.v.s_funcall.args;
            expr_t *name = st.v.s_funcall.name;

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

        case STMT_FUNDECL:
          {
            add_inst (vm, (instr_t){
                              .op = OP_JUMP,
                              .a = 0,
                              .b = 0,
                          });

            size_t vll = vm->inst_len;
            instr_t *lj1 = &vm->insts[vm->inst_len - 1];

            stmt_t *body = st.v.s_fundecl.body;
            size_t bl = st.v.s_fundecl.bl;

            expr_t **args = st.v.s_fundecl.args;
            size_t al = st.v.s_fundecl.argc;

            const char *name = st.v.s_fundecl.name;

            fun_t *f = sf_fun_new (FUN_CODED);
            f->name = (char *)name;

            for (int i = 0; i < al; i++)
              {
                switch (args[i]->type)
                  {
                  case EXPR_VAR:
                    sf_fun_addarg (f, (char *)args[i]->v.e_var.v);
                    break;

                  default:
                    break;
                  }
              }

            size_t pos;
            obj_t *tar;

            pos = add_var_global (vm, name);
            tar = vm->globals[pos];

            if (tar == NULL)
              {
                vm->globals[pos] = sf_objstore_req ();
                tar = vm->globals[pos];
              }

            tar->type = OBJ_FUNC;
            tar->v.o_fun.v = f;

            for (int i = 0; i < f->argl; i++)
              {
                add_inst (vm, (instr_t){
                                  .op = OP_STORE,
                                  .a = add_var_local (vm, f->args[i]),
                                  .b = SF_VM_SLOT_LOCAL,
                              });
              }

            frame_t fr = sf_frame_new ();
            sf_vm_addframe (vm, fr);

            sf_vm_gen_bytecode (vm, (StmtSM *)(StmtSM[]){ (StmtSM){
                                        .vals = body,
                                        .vl = bl,
                                        .vc = bl,
                                    } });

            add_inst (vm, (instr_t){
                              .op = OP_STORE,
                              .a = pos,
                              .b = slot,
                          });

            sf_vm_framefree (vm->frames--);
            vm->fp--;

            lj1->a = vm->inst_len;
            f->v.coded.lp = vll;
          }
          break;

        default:
          break;
        }

      st = *smt->vals++;
    }

  add_inst (vm, (instr_t){
                    .op = OP_RETURN,
                    .a = 0,
                    .b = 0,
                });
}

SF_API void
sf_vm_print_inst (instr_t i)
{
  switch (i.op)
    {
    case OP_LOAD_CONST:
      fputs ("OP_LOAD_CONST:", stdout);
      break;
    case OP_LOAD_FAST:
      fputs ("OP_LOAD_FAST:", stdout);
      break;
    case OP_LOAD_VAR:
      fputs ("OP_LOAD_VAR:", stdout);
      break;
    case OP_STORE:
      fputs ("OP_STORE:", stdout);
      break;
    case OP_RETURN:
      fputs ("OP_RETURN:", stdout);
      break;
    case OP_CALL:
      fputs ("OP_CALL:", stdout);
      break;
    case OP_ADD_1:
      fputs ("OP_ADD_1:", stdout);
      break;
    case OP_ADD:
      fputs ("OP_ADD:", stdout);
      break;
    case OP_SUB:
      fputs ("OP_SUB:", stdout);
      break;
    case OP_MUL:
      fputs ("OP_MUL:", stdout);
      break;
    case OP_DIV:
      fputs ("OP_DIV:", stdout);
      break;
    case OP_JUMP:
      fputs ("OP_JUMP:", stdout);
      break;
    case OP_JUMP_IF_FALSE:
      fputs ("OP_JUMP_IF_FALSE:", stdout);
      break;
    // case OP_STACK_POP:
    //   fputs ("OP_STACK_POP:", stdout);
    //   break;
    // case OP_STACK_PUSH:
    //   fputs ("OP_STACK_PUSH:", stdout);
    //   break;
    default:
      fputs ("UNKNOWN:", stdout);
      break;
    }

  printf (" %d %d\n", i.a, i.b);
}

SF_API void
sf_vm_print_b (vm_t *vm)
{
  for (int i = 0; i < vm->inst_len; i++)
    {
      printf ("(%d) ", i);
      sf_vm_print_inst (vm->insts[i]);
    }
}

static inline void
push (vm_t *vm, obj_t *obj)
{
  if (vm->sp >= vm->stack_cap)
    {
      vm->stack_cap += SF_VM_STACK_CAP;
      vm->stack = SFREALLOC (vm->stack, vm->stack_cap * sizeof (*vm->stack));
    }

  vm->stack[vm->sp++] = obj;
}

static inline obj_t *
pop (vm_t *vm)
{
  if (!vm->sp)
    {
      printf ("error: popping from empty stack\n");
      exit (1);
    }

  return vm->stack[--vm->sp];
}

// #define pop(VM) (VM)->stack[--(VM)->sp]

SF_API void
sf_vm_exec_frame (vm_t *vm)
{
  instr_t *insts = vm->insts;
  frame_t *frame = vm->frames;

start:;
  instr_t i = insts[vm->ip];

  while (1)
    {
      switch (i.op)
        {
        case OP_RETURN:
          goto end;
          break;

        case OP_LOAD_CONST:
          {
            const_t c = vm->map_consts[i.a];

            obj_t *s = sf_objstore_req_forconst (&c);

            if (s == NULL)
              {
                s = sf_objstore_req ();
                s->type = OBJ_CONST;
                s->v.o_const.v = c;
              }

            push (vm, s);
          }
          break;

        case OP_LOAD_VAR:
          {
            if (i.b == SF_VM_SLOT_GLOBAL) /* global */
              {
                obj_t *vg = vm->globals[i.a];

                if (vg == NULL)
                  {
                    vg = vm->globals[i.a] = sf_objstore_req ();
                  }

                push (vm, vg);
              }
            else if (i.b == SF_VM_SLOT_LOCAL)
              {
                obj_t *vg = frame->locals[i.a];

                if (vg == NULL)
                  {
                    vg = frame->locals[i.a] = sf_objstore_req ();
                  }

                push (vm, vg);
              }
          }
          break;

        case OP_STORE:
          {
            /**
             * the place to store should always be
             * at the top of the stack
             */
            obj_t *val = pop (vm);

            // sf_obj_print (*val);

            switch (i.b)
              {
              case SF_VM_SLOT_GLOBAL:
                {
                  if (vm->globals[i.a] != NULL)
                    DR (vm->globals[i.a]);

                  vm->globals[i.a] = val;
                }
                break;

              case SF_VM_SLOT_LOCAL:
                {
                  if (frame->locals[i.a] != NULL)
                    DR (frame->locals[i.a]);

                  frame->locals[i.a] = val;
                }
                break;

              default:
                break;
              }
          }
          break;

        case OP_ADD_1:
          {
            obj_t *s = pop (vm);

            if (s->type == OBJ_CONST && s->v.o_const.v.type == CONST_INT)
              {
                // printf ("[%d]\n", s->v.o_const.v.v.c_int.v);
                const_t d
                    = (const_t){ .type = CONST_INT,
                                 .v.c_int.v = s->v.o_const.v.v.c_int.v + 1 };

                // here;
                // D (sf_const_print (d));
                obj_t *d_obj = sf_objstore_req_forconst (&d);

                if (d_obj == NULL)
                  {
                    d_obj = sf_objstore_req ();
                    d_obj->type = OBJ_CONST;
                    d_obj->v.o_const.v = d;
                  }

                // D (sf_obj_print (*d_obj));
                push (vm, d_obj);
              }
          }
          break;

        case OP_ADD:
          {
            obj_t *r = pop (vm);
            obj_t *l = pop (vm);

            // sf_obj_print (*r);
            // sf_obj_print (*l);

            if (r->type == l->type && l->type == OBJ_CONST
                && l->v.o_const.v.type == CONST_INT)
              {
                // sf_const_print (l->v.o_const.v); putchar ('\n');
                // sf_const_print (r->v.o_const.v); putchar ('\n');

                const_t d
                    = (const_t){ .type = CONST_INT,
                                 .v.c_int.v = l->v.o_const.v.v.c_int.v
                                              + r->v.o_const.v.v.c_int.v };

                // sf_const_print (d);

                obj_t *d_obj = sf_objstore_req_forconst (&d);
                // sf_obj_print (*d_obj);

                if (d_obj == NULL)
                  {
                    d_obj = sf_objstore_req ();
                    d_obj->type = OBJ_CONST;
                    d_obj->v.o_const.v = d;
                  }

                push (vm, d_obj);
              }
          }
          break;

        case OP_SUB:
          {
            obj_t *r = pop (vm);
            obj_t *l = pop (vm);

            if (r->type == l->type && l->type == OBJ_CONST
                && l->v.o_const.v.type == CONST_INT)
              {
                const_t d
                    = (const_t){ .type = CONST_INT,
                                 .v.c_int.v = l->v.o_const.v.v.c_int.v
                                              - r->v.o_const.v.v.c_int.v };

                obj_t *d_obj = sf_objstore_req_forconst (&d);
                // sf_obj_print (*d_obj);

                if (d_obj == NULL)
                  {
                    d_obj = sf_objstore_req ();
                    d_obj->type = OBJ_CONST;
                    d_obj->v.o_const.v = d;
                  }

                push (vm, d_obj);
              }
          }
          break;

        case OP_MUL:
          {
            obj_t *r = pop (vm);
            obj_t *l = pop (vm);

            if (r->type == l->type && l->type == OBJ_CONST
                && l->v.o_const.v.type == CONST_INT)
              {
                const_t d
                    = (const_t){ .type = CONST_INT,
                                 .v.c_int.v = l->v.o_const.v.v.c_int.v
                                              * r->v.o_const.v.v.c_int.v };

                obj_t *d_obj = sf_objstore_req_forconst (&d);
                // sf_obj_print (*d_obj);

                if (d_obj == NULL)
                  {
                    d_obj = sf_objstore_req ();
                    d_obj->type = OBJ_CONST;
                    d_obj->v.o_const.v = d;
                  }

                push (vm, d_obj);
              }
          }
          break;

        case OP_DIV:
          {
            obj_t *r = pop (vm);
            obj_t *l = pop (vm);

            if (r->type == l->type && l->type == OBJ_CONST
                && l->v.o_const.v.type == CONST_INT)
              {
                const_t d = (const_t){ .type = CONST_FLOAT,
                                       .v.c_float.v
                                       = (float)l->v.o_const.v.v.c_int.v
                                         / (float)r->v.o_const.v.v.c_int.v };

                obj_t *d_obj = sf_objstore_req_forconst (&d);
                // sf_obj_print (*d_obj);

                if (d_obj == NULL)
                  {
                    d_obj = sf_objstore_req ();
                    d_obj->type = OBJ_CONST;
                    d_obj->v.o_const.v = d;
                  }

                push (vm, d_obj);
              }
          }
          break;

        case OP_CALL:
          {
            size_t argc = i.a;
            obj_t *name = pop (vm);
            obj_t *args[64];

            for (int i = 0; i < argc; i++)
              {
                args[argc - i - 1] = pop (vm);
              }

            switch (name->type)
              {
              case OBJ_FUNC:
                {
                  fun_t *f = name->v.o_fun.v;

                  assert (argc == f->argl);

                  switch (f->type)
                    {
                    case FUN_NATIVE:
                      {
                        switch (f->v.native.nf_type)
                          {
                          case NF_ARG_1:
                            {
                              obj_t *r = f->v.native.v.f_onearg (*args);

                              if (r != NULL)
                                DR (r);
                            }
                            break;

                          case NF_ARG_2:
                            {
                              obj_t *r
                                  = f->v.native.v.f_twoarg (args[0], args[1]);

                              if (r != NULL)
                                DR (r);
                            }
                            break;

                          case NF_ARG_3:
                            {
                              obj_t *r = f->v.native.v.f_threearg (
                                  args[0], args[1], args[2]);

                              if (r != NULL)
                                DR (r);
                            }
                            break;

                          default:
                            break;
                          }
                      }
                      break;

                    case FUN_CODED:
                      {
                        for (int i = 0; i < argc; i++)
                          push (vm, args[i]);

                        frame_t fr = sf_frame_new ();
                        fr.return_ip = vm->ip + 1;
                        fr.stack_base = vm->sp;

                        sf_vm_addframe (vm, fr);
                        vm->ip = f->v.coded.lp - 1;
                        frame++;
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
          break;

        case OP_JUMP_IF_FALSE:
          {
            obj_t *t = pop (vm);

            if (!_obj_tobool (t))
              {
                vm->ip = i.a - 1;
              }
          }
          break;

        case OP_JUMP:
          {
            vm->ip = i.a - 1;
          }
          break;

        default:
          break;
        }

    lend:;
      i = insts[++vm->ip];
    }

end:;

  if (vm->fp > 0)
    {
      vm->ip = frame->return_ip;
      vm->sp = frame->stack_base;
      vm->fp--;

      sf_vm_framefree (frame--);
      goto start;
    }
}

SF_API frame_t
sf_frame_new ()
{
  frame_t f;
  f.return_ip = 0;
  f.locals_cap = SF_FRAME_LOCALS_CAP;
  f.locals_count = 0;
  f.locals = SFMALLOC (f.locals_cap * sizeof (*f.locals));
  f.stack_base = 0;

  return f;
}

SF_API void
sf_vm_addframe (vm_t *vm, frame_t f)
{
  if (vm->fp >= vm->frame_cap)
    {
      vm->frame_cap += SF_VM_FRAME_CAP;
      vm->frames
          = SFREALLOC (vm->frames, vm->frame_cap * sizeof (*vm->frames));
    }

  vm->frames[vm->fp++] = f;
}

SF_API void
sf_vm_framefree (frame_t *f)
{
  for (int i = 0; i < f->locals_count; i++)
    if (f->locals[i] != NULL)
      DR (f->locals[i]);

  SFFREE (f->locals);
}