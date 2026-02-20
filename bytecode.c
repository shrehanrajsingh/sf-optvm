#include "bytecode.h"

static const_t __sf_none_obj = (const_t){ .type = CONST_NONE };

SF_API vm_t
sf_vm_new ()
{
  vm_t v;

  v.globals_cap = SF_VM_GLOBALS_CAP;
  v.globals = SFMALLOC (v.globals_cap * sizeof (*v.globals));
  v.htc = SF_VM_HT_CAP;
  v.htl = 0;
  v.hts = SFMALLOC (v.htc * sizeof (*v.hts));
  v.hts[v.htl++] = sf_ht_new ();
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
  v.meta.slot = SF_VM_SLOT_GLOBAL;
  v.meta.g_slot = 0;
  v.meta.l_slot = 0;

  for (int i = 0; i < v.globals_cap; i++)
    v.globals[i] = NULL;

  return v;
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
    case OP_LOAD:
      fputs ("OP_LOAD:", stdout);
      break;
    case OP_LOAD_NAME:
      printf ("OP_LOAD_NAME: '%s' ", i.c);
      break;
    case OP_STORE:
      fputs ("OP_STORE:", stdout);
      break;
    case OP_STORE_FAST:
      fputs ("OP_STORE_FAST:", stdout);
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
    case OP_LOAD_FUNC_CODED:
      fputs ("OP_LOAD_FUNC_CODED:", stdout);
      break;
    case OP_CMP:
      fputs ("OP_CMP:", stdout);
      break;
    case OP_LOAD_BUILDCLASS:
      fputs ("OP_LOAD_BUILDCLASS:", stdout);
      break;
    case OP_LOAD_BUILDCLASS_END:
      fputs ("OP_LOAD_BUILDCLASS_END:", stdout);
      break;
    case OP_STORE_NAME:
      printf ("OP_STORE_NAME: '%s'", i.c);
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
      D (printf ("vm_ip: %lu\n", vm->ip));
      D (printf ("error: popping from empty stack\n"));
      exit (1);
    }

  return vm->stack[--vm->sp];
}

SF_API void
sf_vm_exec_frame_top (vm_t *vm)
{
  frame_t *fr = &vm->frames[vm->fp - 1];
  instr_t i = vm->insts[vm->ip];

  if (vm->meta.g_slot >= vm->globals_cap)
    {
      vm->globals_cap += SF_VM_GLOBALS_CAP;
      vm->globals
          = SFREALLOC (vm->globals, vm->globals_cap * sizeof (*vm->globals));
    }

start:;
  while (1)
    {
      // D (printf ("%lu", vm->ip));

      switch (i.op)
        {
        case OP_RETURN:
          {
            obj_t *o = NULL;
            if (i.a == 1)
              {
                /* user wrote a return statement */
              }
            else
              push (vm, o = sf_objstore_req_forconst (&__sf_none_obj));

            if (o != NULL)
              IR (o);

            goto end;
          }
          break;

        case OP_LOAD_CONST:
          {
            const_t d = vm->map_consts[i.a];

            obj_t *d_obj = sf_objstore_req_forconst (&d);

            if (d_obj == NULL)
              {
                d_obj = sf_objstore_req ();
                d_obj->type = OBJ_CONST;
                d_obj->v.o_const.v = d;
              }

            IR (d_obj);
            push (vm, d_obj);
          }
          break;

        case OP_JUMP_IF_FALSE:
          {
            obj_t *p = pop (vm);

            if (sf_obj_isfalse (*p))
              vm->ip = i.a - 1;

            DR (p);
          }
          break;

        case OP_JUMP:
          {
            vm->ip = i.a - 1;
          }
          break;

        case OP_STORE:
          {
            obj_t *val = pop (vm);
            // IR (val);

            if (vm->globals[i.a] != NULL)
              DR (vm->globals[i.a]);
            vm->globals[i.a] = val;

            // push (vm, val);
          }
          break;

        case OP_STORE_FAST:
          {
            obj_t *val = pop (vm);
            // IR (val);

            if (i.a >= fr->l.locals_cap)
              {
                fr->l.locals_cap += SF_FRAME_LOCALS_CAP;

                fr->l.locals = SFREALLOC (
                    fr->l.locals, fr->l.locals_cap * sizeof (*fr->l.locals));
              }

            if (i.a >= fr->l.locals_count)
              fr->l.locals_count = i.a + 1;

            if (fr->l.locals[i.a] != NULL)
              DR (fr->l.locals[i.a]);
            fr->l.locals[i.a] = val;

            // push (vm, val);
          }
          break;

        case OP_LOAD:
          {
            obj_t *o = NULL;
            push (vm, o = vm->globals[i.a]);

            if (o != NULL)
              IR (o);
          }
          break;

        case OP_LOAD_FAST:
          {
            obj_t *o = NULL;

            if (i.a >= fr->l.locals_cap)
              {
                fr->l.locals_cap += SF_FRAME_LOCALS_CAP;

                fr->l.locals = SFREALLOC (
                    fr->l.locals, fr->l.locals_cap * sizeof (*fr->l.locals));
              }

            if (i.a >= fr->l.locals_count)
              fr->l.locals_count = i.a + 1;

            if (i.b == 0)
              push (vm, o = fr->l.locals[i.a]);
            else
              {
                /* number of levels to go up is less than number of frames */
                assert (i.b < vm->fp);

                push (vm, o = vm->frames[i.b].l.locals[i.a]);
              }

            if (o != NULL)
              IR (o);
          }
          break;

        case OP_LOAD_FUNC_CODED:
          {
            obj_t *o = sf_objstore_req ();
            o->type = OBJ_FUNC;
            o->v.o_fun.v = SFMALLOC (sizeof (fun_t));
            o->v.o_fun.v->type = FUN_CODED;
            o->v.o_fun.v->v.coded.lp = i.a;
            o->v.o_fun.v->argc = o->v.o_fun.v->argl = i.b;

            IR (o);
            push (vm, o);
          }
          break;

        case OP_CALL:
          {
            size_t argc = i.a;
            obj_t *name = pop (vm);

            // IR (name);

            obj_t *args[64];
            size_t al = 0;

            while (al < argc)
              {
                args[al++] = pop (vm);
                // IR (args[al++]);
              }

            switch (name->type)
              {
              case OBJ_FUNC:
                {
                  fun_t *f = name->v.o_fun.v;
                  assert (f->argl == argc);

                  switch (f->type)
                    {
                    case FUN_NATIVE:
                      {
                        switch (f->v.native.nf_type)
                          {
                          case NF_ARG_1:
                            {
                              obj_t *r = f->v.native.v.f_onearg (args[0]);

                              if (r != NULL)
                                {
                                  if (i.b != 1)
                                    {
                                      DR (r);
                                    }
                                  else
                                    {
                                      push (vm, r);
                                    }
                                }
                              else
                                {
                                  if (i.b == 1)
                                    {
                                      obj_t *o = sf_objstore_req_forconst (
                                          &__sf_none_obj);

                                      push (vm, o);
                                    }
                                }
                            }
                            break;

                          case NF_ARG_2:
                            {
                              obj_t *r
                                  = f->v.native.v.f_twoarg (args[0], args[1]);

                              if (r != NULL)
                                {
                                  if (i.b != 1)
                                    {
                                      DR (r);
                                    }
                                  else
                                    {
                                      push (vm, r);
                                    }
                                }
                              else
                                {
                                  if (i.b == 1)
                                    {
                                      obj_t *o = sf_objstore_req_forconst (
                                          &__sf_none_obj);
                                    }
                                }
                            }
                            break;

                          case NF_ARG_3:
                            {
                              obj_t *r = f->v.native.v.f_threearg (
                                  args[0], args[1], args[2]);

                              if (r != NULL)
                                {
                                  if (i.b != 1)
                                    {
                                      DR (r);
                                    }
                                  else
                                    {
                                      push (vm, r);
                                    }
                                }
                              else
                                {
                                  if (i.b == 1)
                                    {
                                      obj_t *o = sf_objstore_req_forconst (
                                          &__sf_none_obj);
                                    }
                                }
                            }
                            break;

                          case NF_ARG_ANY:
                            {
                              obj_t *r = f->v.native.v.f_anyarg (args, al);

                              if (r != NULL)
                                {
                                  if (i.b != 1)
                                    {
                                      DR (r);
                                    }
                                  else
                                    {
                                      push (vm, r);
                                    }
                                }
                              else
                                {
                                  if (i.b == 1)
                                    {
                                      obj_t *o = sf_objstore_req_forconst (
                                          &__sf_none_obj);
                                    }
                                }
                            }
                            break;

                          default:
                            break;
                          }

                        for (size_t i = 0; i < al; i++)
                          DR (args[i]);
                      }
                      break;

                    case FUN_CODED:
                      {
                        size_t lp = f->v.coded.lp;

                        for (size_t i = 0; i < al; i++)
                          {
                            push (vm, args[i]);
                            // IR (args[i]);
                          }

                        frame_t frt = sf_frame_new_local ();
                        frt.return_ip = vm->ip + 1;
                        // D (printf ("%d\n", fr.return_ip));
                        frt.stack_base = vm->sp;

                        sf_vm_addframe (vm, frt);
                        fr = &vm->frames[vm->fp - 1];
                        vm->ip = lp - 1;

                        if (i.b == 1)
                          fr->pop_ret_val = 0; /* need return value */
                        else
                          fr->pop_ret_val
                              = 1; /* dont need return value (stmt call) */
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

            DR (name);

            // for (size_t i = 0; i < al; i++)
            //   {
            //     DR (args[i]);
            //   }
          }
          break;

        case OP_ADD_1:
          {
            obj_t *p = pop (vm);
            // IR (p);

            if (p->type == OBJ_CONST && p->v.o_const.v.type == CONST_INT)
              {
                int r = p->v.o_const.v.v.c_int.v + 1;

                obj_t *o = sf_objstore_req_forconst ((const_t *)(const_t[]){
                    { .type = CONST_INT, .v.c_int.v = r } });

                if (o == NULL)
                  {
                    o = sf_objstore_req ();
                    o->type = OBJ_CONST;
                    o->v.o_const.v.type = CONST_INT;
                    o->v.o_const.v.v.c_int.v = r;
                  }

                push (vm, o);
                IR (o);
              }

            DR (p);
          }
          break;

        case OP_ADD:
          {
            obj_t *l = pop (vm);
            obj_t *r = pop (vm);

            // IR (l);
            // IR (r);

            if (r->type == l->type && l->type == OBJ_CONST
                && l->v.o_const.v.type == r->v.o_const.v.type
                && l->v.o_const.v.type == CONST_INT)
              {
                int e = r->v.o_const.v.v.c_int.v + l->v.o_const.v.v.c_int.v;

                obj_t *o = sf_objstore_req_forconst ((const_t *)(const_t[]){
                    { .type = CONST_INT, .v.c_int.v = e } });

                if (o == NULL)
                  {
                    o = sf_objstore_req ();
                    o->type = OBJ_CONST;
                    o->v.o_const.v.type = CONST_INT;
                    o->v.o_const.v.v.c_int.v = e;
                  }

                push (vm, o);
                IR (o);
              }

            DR (l);
            DR (r);
          }
          break;

        case OP_SUB:
          {
            obj_t *l = pop (vm);
            obj_t *r = pop (vm);

            // IR (l);
            // IR (r);

            if (r->type == l->type && l->type == OBJ_CONST
                && l->v.o_const.v.type == r->v.o_const.v.type
                && l->v.o_const.v.type == CONST_INT)
              {
                int e = r->v.o_const.v.v.c_int.v - l->v.o_const.v.v.c_int.v;

                obj_t *o = sf_objstore_req_forconst ((const_t *)(const_t[]){
                    { .type = CONST_INT, .v.c_int.v = e } });

                if (o == NULL)
                  {
                    o = sf_objstore_req ();
                    o->type = OBJ_CONST;
                    o->v.o_const.v.type = CONST_INT;
                    o->v.o_const.v.v.c_int.v = e;
                  }

                push (vm, o);
                IR (o);
              }

            DR (l);
            DR (r);
          }
          break;

        case OP_MUL:
          {
            obj_t *l = pop (vm);
            obj_t *r = pop (vm);

            // IR (l);
            // IR (r);

            if (r->type == l->type && l->type == OBJ_CONST
                && l->v.o_const.v.type == r->v.o_const.v.type
                && l->v.o_const.v.type == CONST_INT)
              {
                int e = r->v.o_const.v.v.c_int.v * l->v.o_const.v.v.c_int.v;

                obj_t *o = sf_objstore_req_forconst ((const_t *)(const_t[]){
                    { .type = CONST_INT, .v.c_int.v = e } });

                if (o == NULL)
                  {
                    o = sf_objstore_req ();
                    o->type = OBJ_CONST;
                    o->v.o_const.v.type = CONST_INT;
                    o->v.o_const.v.v.c_int.v = e;
                  }

                push (vm, o);
                IR (o);
              }

            DR (l);
            DR (r);
          }
          break;

        case OP_CMP:
          {
            obj_t *r = pop (vm);
            obj_t *l = pop (vm);

            // IR (l);
            // IR (r);

            int rc = 0;

            switch (i.a)
              {
              case CMP_EQEQ:
                {
                  rc = sf_obj_eqeq (l, r);
                }
                break;

              case CMP_GE:
                {
                  rc = sf_obj_ge (l, r);
                }
                break;

              case CMP_GEQ:
                {
                  rc = sf_obj_geq (l, r);
                }
                break;

              case CMP_LE:
                {
                  rc = sf_obj_le (l, r);
                }
                break;

              case CMP_LEQ:
                {
                  rc = sf_obj_leq (l, r);
                }
                break;

              case CMP_NEQ:
                {
                  rc = sf_obj_neq (l, r);
                }
                break;

              default:
                break;
              }

            const_t bc = (const_t){ .type = CONST_BOOL, .v.c_bool.v = rc };

            obj_t *o_bc = sf_objstore_req_forconst (&bc);

            if (o_bc == NULL)
              {
                o_bc = sf_objstore_req ();

                o_bc->type = OBJ_CONST;
                o_bc->v.o_const.v = bc;
              }

            push (vm, o_bc);
            IR (o_bc);

            DR (l);
            DR (r);
          }
          break;

        default:
          break;
        }

      i = vm->insts[++vm->ip];
    }

end:;

  vm->ip = fr->return_ip;
  i = vm->insts[vm->ip];

  // while (vm->sp > fr->stack_base)
  //   {
  //     DR (pop (vm));
  //   }
  //   //   vm->sp = fr->stack_base;
  // if (vm->fp)
  //   {
  //     sf_vm_popframe (vm);
  //   }

  if (fr->pop_ret_val)
    {
      DR (pop (vm));
    }
  else
    {
      // obj_t *o = pop (vm);

      // while (vm->sp > fr->stack_base)
      //   {
      //     DR (pop (vm));
      //   }

      // push (vm, o);
    }

  if (vm->fp == 1)
    {
    }
  else if (vm->fp > 1)
    {
      sf_vm_popframe (vm);
      fr = &vm->frames[vm->fp - 1];
      goto start;
    }
}

SF_API frame_t
sf_frame_new_local ()
{
  frame_t f;
  f.type = FRAME_LOCAL;
  f.return_ip = 0;
  f.l.locals_cap = SF_FRAME_LOCALS_CAP;
  f.l.locals_count = 0;
  f.l.locals = SFMALLOC (f.l.locals_cap * sizeof (*f.l.locals));
  f.stack_base = 0;

  return f;
}

SF_API frame_t
sf_frame_new_name ()
{
  frame_t f;
  f.type = FRAME_NAME;
  f.return_ip = 0;
  f.n.nvc = SF_VM_NAME_CAP;
  f.n.nvl = 0;
  f.n.vals = SFMALLOC (f.n.nvc * sizeof (*f.n.vals));
  f.n.names = SFMALLOC (f.n.nvc * sizeof (*f.n.names));
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
sf_vm_popframe (vm_t *vm)
{
  // frame_t *f = &vm->frames[vm->fp - 1];

  // for (int i = 0; i < f->locals_count; i++)
  //   if (f->locals[i] != NULL)
  //     DR (f->locals[i]);

  sf_vm_framefree (&vm->frames[--vm->fp]);
}

SF_API void
sf_vm_framefree (frame_t *f)
{
  // D (printf ("%lu\n", f->locals_count));
  for (int i = 0; i < f->l.locals_cap; i++)
    {
      if (f->l.locals[i] != NULL)
        {
          DR (f->l.locals[i]);
        }
    }

  SFFREE (f->l.locals);
}