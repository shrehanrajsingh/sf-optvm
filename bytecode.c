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
  v.meta.n_slot = 0;

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
    case OP_DOT_ACCESS:
      printf ("OP_DOT_ACCESS: '%s'", i.c);
      break;
    case OP_LOAD_ARRAY:
      printf ("OP_LOAD_ARRAY: ");
      break;
    case OP_SQR_ACCESS:
      printf ("OP_SQR_ACCESS: ");
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
sf_vm_exec_single_frame (vm_t *vm)
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
      fr = &vm->frames[vm->fp - 1];
      // D (printf ("%lu", vm->ip));

      switch (i.op)
        {
        case OP_RETURN:
          {
            obj_t *o = NULL;
            if (i.a == 1)
              {
                /* user wrote a return statement */
                /* already pushed to stack */
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

            DR (p, vm);
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
            // sf_obj_print (*val);
            // D (printf ("%d\n", val->meta.ref_count));

            if (vm->globals[i.a] != NULL)
              {
                // D (sf_obj_print (*vm->globals[i.a]));
                // D (printf ("%d\n", vm->globals[i.a]->meta.ref_count));
                DR (vm->globals[i.a], vm);
              }
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

                for (size_t j = fr->l.locals_count; j < fr->l.locals_cap; j++)
                  fr->l.locals[j] = NULL;
              }

            if (i.a >= fr->l.locals_count)
              fr->l.locals_count = i.a + 1;

            if (fr->l.locals[i.a] != NULL)
              DR (fr->l.locals[i.a], vm);
            fr->l.locals[i.a] = val;

            // push (vm, val);
          }
          break;

        case OP_STORE_NAME:
          {
            obj_t *val = pop (vm);

            if (i.b == 0)
              {
                if (i.a >= fr->n.nvc)
                  {
                    fr->n.nvc += SF_FRAME_LOCALS_CAP;

                    fr->n.names = SFREALLOC (
                        fr->n.names, fr->n.nvc * sizeof (*fr->n.names));

                    fr->n.vals = SFREALLOC (fr->n.vals,
                                            fr->n.nvc * sizeof (*fr->n.vals));

                    for (size_t j = fr->n.nvl; j < fr->n.nvc; j++)
                      fr->n.vals[j] = NULL;
                  }

                if (i.a >= fr->n.nvl)
                  fr->n.nvl = i.a + 1;

                if (fr->n.vals[i.a] != NULL)
                  DR (fr->n.vals[i.a], vm);

                fr->n.vals[i.a] = val;
                fr->n.names[i.a] = i.c;
              }
            else if (i.b == 1)
              {
                /* pop from stack again, val is now the key */
                obj_t *vv = pop (vm);

                container_set (val, i.c, vv, vm);
                // D (sf_obj_print (*val));
                // D (printf ("%d\n", val->meta.ref_count));
                DR (val, vm);
              }
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

        case OP_LOAD_NAME:
          {
            obj_t *o = NULL;
            push (vm, o = fr->n.vals[i.a]);

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
                                      DR (r, vm);
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
                                      DR (r, vm);
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
                                      DR (r, vm);
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
                                      DR (r, vm);
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
                          DR (args[i], vm);
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

              case OBJ_HFF:
                {
                  size_t hf_al = name->v.o_hff.al;
                  obj_t **hf_args = name->v.o_hff.args;
                  obj_t *hf_fo = name->v.o_hff.f;

                  assert (hf_fo->type == OBJ_FUNC);
                  fun_t *f = hf_fo->v.o_fun.v;

                  // for (int j = al - 1; j > -1; j--)
                  //   {
                  //     args[j + hf_al] = args[j];
                  //   }

                  // for (size_t j = 0; j < hf_al; j++)
                  //   {
                  //     args[j] = hf_args[j];
                  //   }

                  for (size_t j = 0; j < hf_al; j++)
                    args[al++] = hf_args[j];

                  // al += hf_al;
                  assert (al == f->argl);

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
                                      DR (r, vm);
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
                                      DR (r, vm);
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
                                      DR (r, vm);
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
                                      DR (r, vm);
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
                          DR (args[i], vm);
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

              case OBJ_CLASS:
                {
                  class_t *c = name->v.o_class.v;
                  cobj_t *co = sf_cobj_new (c);

                  obj_t *o = sf_objstore_req ();
                  o->type = OBJ_COBJ;
                  o->v.o_cobj.v = co;

                  if (i.b == 1)
                    {
                      push (vm, o);
                      IR (o);
                    }
                  // else
                  //   sf_cobj_free (co);

                  obj_t *_init_method = container_access (o, "_init");
                  if (_init_method != NULL)
                    {
                      assert (_init_method->type == OBJ_HFF);
                      obj_t *hfo = _init_method->v.o_hff.f;

                      assert (hfo->type == OBJ_FUNC);
                      fun_t *f = hfo->v.o_fun.v;

                      if (f->type == FUN_CODED)
                        {
                          assert (f->argl == al + 1);
                          size_t lp = f->v.coded.lp;

                          for (size_t i = 0; i < al; i++)
                            {
                              push (vm, args[i]);
                              // IR (args[i]);
                            }

                          push (vm, o);
                          IR (o);

                          frame_t frt = sf_frame_new_local ();
                          frt.return_ip = vm->ip + 1;
                          // D (printf ("%d\n", fr.return_ip));
                          frt.stack_base = vm->sp;

                          sf_vm_addframe (vm, frt);
                          fr = &vm->frames[vm->fp - 1];
                          vm->ip = lp - 1;
                          fr->pop_ret_val = 1;
                        }
                      else if (f->type == FUN_NATIVE)
                        {
                          D (printf ("[TODO] native function as an _init"));
                        }
                    }

                  /* resolve r-values */
                  IR (_init_method);
                  DR (_init_method, vm);
                }
                break;

              default:
                break;
              }

            DR (name, vm);

            // for (size_t i = 0; i < al; i++)
            //   {
            //     DR (args[i], vm);
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

            DR (p, vm);
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

            DR (l, vm);
            DR (r, vm);
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

            DR (l, vm);
            DR (r, vm);
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

            DR (l, vm);
            DR (r, vm);
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

            DR (l, vm);
            DR (r, vm);
          }
          break;

        case OP_LOAD_BUILDCLASS:
          {
            frame_t nf = sf_frame_new_name ();

            nf.return_ip = i.a; /* buildclass_end location */
            sf_vm_addframe (vm, nf);

            fr = &vm->frames[vm->fp - 1];
          }
          break;

        case OP_LOAD_BUILDCLASS_END:
          {
            frame_t f = vm->frames[--vm->fp];
            assert (f.type == FRAME_NAME);

            class_t *cl = sf_class_new ();

            cl->svl = f.n.nvl;
            cl->svc = f.n.nvl;

            // cl->slots = SFMALLOC (sizeof (*cl->slots));
            // cl->vals = SFMALLOC (sizeof (*cl->vals));

            // for (size_t j = 0; j < f.n.nvl; j++)
            //   {
            //     cl->vals[j] = f.n.vals[j];
            //     cl->slots[j] = f.n.names[j];
            //   }

            cl->slots = f.n.names;
            cl->vals = f.n.vals;
            cl->name = vm->insts[i.a].c;

            obj_t *o = sf_objstore_req ();
            o->type = OBJ_CLASS;
            o->v.o_class.v = cl;

            push (vm, o);
            IR (o);
          }
          break;

        case OP_DOT_ACCESS:
          {
            obj_t *l = pop (vm);
            char *name = i.c;

            obj_t *o = container_access (l, name);

            if (o == NULL)
              {
                printf ("member '%s' does not exist.\n", name);
                exit (EXIT_FAILURE);
              }

            push (vm, o);
            IR (o);
            DR (l, vm);
          }
          break;

        case OP_LOAD_ARRAY:
          {
            array_t *ar = sf_array_withsize (i.a);
            for (int j = i.a - 1; j >= 0; j--)
              {
                ar->vals[j] = pop (vm);
              }

            obj_t *o = sf_objstore_req ();
            o->type = OBJ_ARRAY;
            o->v.o_array.v = ar;

            push (vm, o);
            IR (o);
          }
          break;

        case OP_SQR_ACCESS:
          {
            obj_t *idx = pop (vm);
            obj_t *par = pop (vm);

            obj_t *o = sqr_access (par, idx);
            push (vm, o);
            IR (o);
          }
          break;

        default:
          break;
        }

      i = vm->insts[++vm->ip];
    }

end:;
  vm->ip = fr->return_ip;
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
      fr = &vm->frames[vm->fp - 1];
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

            DR (p, vm);
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
            // sf_obj_print (*val);
            // D (printf ("%d\n", val->meta.ref_count));

            if (vm->globals[i.a] != NULL)
              {
                // D (sf_obj_print (*vm->globals[i.a]));
                // D (printf ("%d\n", vm->globals[i.a]->meta.ref_count));
                DR (vm->globals[i.a], vm);
              }
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

                for (size_t j = fr->l.locals_count; j < fr->l.locals_cap; j++)
                  fr->l.locals[j] = NULL;
              }

            if (i.a >= fr->l.locals_count)
              fr->l.locals_count = i.a + 1;

            if (fr->l.locals[i.a] != NULL)
              DR (fr->l.locals[i.a], vm);
            fr->l.locals[i.a] = val;

            // push (vm, val);
          }
          break;

        case OP_STORE_NAME:
          {
            obj_t *val = pop (vm);

            if (i.b == 0)
              {
                if (i.a >= fr->n.nvc)
                  {
                    fr->n.nvc += SF_FRAME_LOCALS_CAP;

                    fr->n.names = SFREALLOC (
                        fr->n.names, fr->n.nvc * sizeof (*fr->n.names));

                    fr->n.vals = SFREALLOC (fr->n.vals,
                                            fr->n.nvc * sizeof (*fr->n.vals));

                    for (size_t j = fr->n.nvl; j < fr->n.nvc; j++)
                      fr->n.vals[j] = NULL;
                  }

                if (i.a >= fr->n.nvl)
                  fr->n.nvl = i.a + 1;

                if (fr->n.vals[i.a] != NULL)
                  DR (fr->n.vals[i.a], vm);

                fr->n.vals[i.a] = val;
                fr->n.names[i.a] = i.c;
              }
            else if (i.b == 1)
              {
                /* pop from stack again, val is now the key */
                obj_t *vv = pop (vm);

                container_set (val, i.c, vv, vm);
                // D (sf_obj_print (*val));
                // D (printf ("%d\n", val->meta.ref_count));
                DR (val, vm);
              }
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

        case OP_LOAD_NAME:
          {
            obj_t *o = NULL;
            push (vm, o = fr->n.vals[i.a]);

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
                                      DR (r, vm);
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
                                      DR (r, vm);
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
                                      DR (r, vm);
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
                                      DR (r, vm);
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
                          DR (args[i], vm);
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

              case OBJ_HFF:
                {
                  size_t hf_al = name->v.o_hff.al;
                  obj_t **hf_args = name->v.o_hff.args;
                  obj_t *hf_fo = name->v.o_hff.f;

                  assert (hf_fo->type == OBJ_FUNC);
                  fun_t *f = hf_fo->v.o_fun.v;

                  // for (int j = al - 1; j > -1; j--)
                  //   {
                  //     args[j + hf_al] = args[j];
                  //   }

                  // for (size_t j = 0; j < hf_al; j++)
                  //   {
                  //     args[j] = hf_args[j];
                  //   }

                  for (size_t j = 0; j < hf_al; j++)
                    args[al++] = hf_args[j];

                  // al += hf_al;
                  assert (al == f->argl);

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
                                      DR (r, vm);
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
                                      DR (r, vm);
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
                                      DR (r, vm);
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
                                      DR (r, vm);
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
                          DR (args[i], vm);
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

              case OBJ_CLASS:
                {
                  class_t *c = name->v.o_class.v;
                  cobj_t *co = sf_cobj_new (c);

                  obj_t *o = sf_objstore_req ();
                  o->type = OBJ_COBJ;
                  o->v.o_cobj.v = co;

                  if (i.b == 1)
                    {
                      push (vm, o);
                      IR (o);
                    }
                  // else
                  //   sf_cobj_free (co);

                  obj_t *_init_method = container_access (o, "_init");
                  if (_init_method != NULL)
                    {
                      assert (_init_method->type == OBJ_HFF);
                      obj_t *hfo = _init_method->v.o_hff.f;

                      assert (hfo->type == OBJ_FUNC);
                      fun_t *f = hfo->v.o_fun.v;

                      if (f->type == FUN_CODED)
                        {
                          assert (f->argl == al + 1);
                          size_t lp = f->v.coded.lp;

                          for (size_t i = 0; i < al; i++)
                            {
                              push (vm, args[i]);
                              // IR (args[i]);
                            }

                          push (vm, o);
                          IR (o);

                          frame_t frt = sf_frame_new_local ();
                          frt.return_ip = vm->ip + 1;
                          // D (printf ("%d\n", fr.return_ip));
                          frt.stack_base = vm->sp;

                          sf_vm_addframe (vm, frt);
                          fr = &vm->frames[vm->fp - 1];
                          vm->ip = lp - 1;
                          fr->pop_ret_val = 1;
                        }
                      else if (f->type == FUN_NATIVE)
                        {
                          D (printf ("[TODO] native function as an _init"));
                        }
                    }

                  /* resolve r-values */
                  IR (_init_method);
                  DR (_init_method, vm);
                }
                break;

              default:
                break;
              }

            DR (name, vm);

            // for (size_t i = 0; i < al; i++)
            //   {
            //     DR (args[i], vm);
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

            DR (p, vm);
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

            DR (l, vm);
            DR (r, vm);
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

            DR (l, vm);
            DR (r, vm);
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

            DR (l, vm);
            DR (r, vm);
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

            DR (l, vm);
            DR (r, vm);
          }
          break;

        case OP_LOAD_BUILDCLASS:
          {
            frame_t nf = sf_frame_new_name ();

            nf.return_ip = i.a; /* buildclass_end location */
            sf_vm_addframe (vm, nf);

            fr = &vm->frames[vm->fp - 1];
          }
          break;

        case OP_LOAD_BUILDCLASS_END:
          {
            frame_t f = vm->frames[--vm->fp];
            assert (f.type == FRAME_NAME);

            class_t *cl = sf_class_new ();

            cl->svl = f.n.nvl;
            cl->svc = f.n.nvl;

            // cl->slots = SFMALLOC (sizeof (*cl->slots));
            // cl->vals = SFMALLOC (sizeof (*cl->vals));

            // for (size_t j = 0; j < f.n.nvl; j++)
            //   {
            //     cl->vals[j] = f.n.vals[j];
            //     cl->slots[j] = f.n.names[j];
            //   }

            cl->slots = f.n.names;
            cl->vals = f.n.vals;
            cl->name = vm->insts[i.a].c;

            obj_t *o = sf_objstore_req ();
            o->type = OBJ_CLASS;
            o->v.o_class.v = cl;

            push (vm, o);
            IR (o);
          }
          break;

        case OP_DOT_ACCESS:
          {
            obj_t *l = pop (vm);
            char *name = i.c;

            obj_t *o = container_access (l, name);

            if (o == NULL)
              {
                printf ("member '%s' does not exist.\n", name);
                exit (EXIT_FAILURE);
              }

            push (vm, o);
            IR (o);
            DR (l, vm);
          }
          break;

        case OP_LOAD_ARRAY:
          {
            array_t *ar = sf_array_withsize (i.a);
            for (int j = i.a - 1; j >= 0; j--)
              {
                ar->vals[j] = pop (vm);
              }

            obj_t *o = sf_objstore_req ();
            o->type = OBJ_ARRAY;
            o->v.o_array.v = ar;

            push (vm, o);
            IR (o);
          }
          break;

        case OP_SQR_ACCESS:
          {
            obj_t *idx = pop (vm);
            obj_t *par = pop (vm);

            obj_t *o = sqr_access (par, idx);
            push (vm, o);
            IR (o);
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
  //     DR (pop (vm), vm);
  //   }
  //   //   vm->sp = fr->stack_base;
  // if (vm->fp)
  //   {
  //     sf_vm_popframe (vm);
  //   }

  if (fr->pop_ret_val)
    {
      DR (pop (vm), vm);
    }
  else
    {
      // obj_t *o = pop (vm);

      // while (vm->sp > fr->stack_base)
      //   {
      //     DR (pop (vm), vm);
      //   }

      // push (vm, o);
    }

  if (vm->fp == 1)
    {
      sf_vm_popframe (vm);

      for (size_t i = 0; i < vm->globals_cap; i++)
        {
          if (vm->globals[i] != NULL)
            DR (vm->globals[i], vm);
        }

      SFFREE (vm->globals);
    }
  else if (vm->fp > 1)
    {
      sf_vm_popframe (vm);
      fr = &vm->frames[vm->fp - 1];
      goto start;
    }

  if (vm->fp)
    goto start;
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

  for (int i = 0; i < f.l.locals_cap; i++)
    f.l.locals[i] = NULL;

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

  for (int i = 0; i < f.n.nvc; i++)
    f.n.vals[i] = NULL;

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
  //     DR (f->locals[i], vm);

  sf_vm_framefree (&vm->frames[--vm->fp], vm);
}

SF_API void
sf_vm_framefree (frame_t *f, vm_t *vm)
{
  // D (printf ("%lu\n", f->locals_count));

  switch (f->type)
    {
    case FRAME_LOCAL:
      {
        for (int i = 0; i < f->l.locals_cap; i++)
          {
            if (f->l.locals[i] != NULL)
              {
                DR (f->l.locals[i], vm);
              }
          }

        SFFREE (f->l.locals);
      }
      break;

    case FRAME_NAME:
      {
        for (size_t i = 0; i < f->n.nvc; i++)
          {
            if (f->n.vals[i] != NULL)
              DR (f->n.vals[i], vm);
          }

        SFFREE (f->n.names);
        SFFREE (f->n.vals);
      }
      break;

    case FRAME_GLOBAL:
      {
        here;
      }
      break;

    default:
      break;
    }
}

obj_t *
container_access (obj_t *o, char *name)
{
  switch (o->type)
    {
    case OBJ_CLASS:
      {
        class_t *c = o->v.o_class.v;

        for (int i = 0; i < c->svl; i++)
          if (!strcmp (c->slots[i], name))
            return c->vals[i];
      }
      break;

    case OBJ_COBJ:
      {
        cobj_t *c = o->v.o_cobj.v;
        obj_t *r = NULL;

        /* check cobj dict */
        for (int i = 0; i < c->svl; i++)
          {
            if (!strcmp (c->slots[i], name))
              {
                r = c->vals[i];
                break;
              }
          }

        if (r == NULL)
          {
            /* check class */
            for (int i = 0; i < c->p->svl; i++)
              {
                if (!strcmp (c->p->slots[i], name))
                  {
                    r = c->p->vals[i];
                    break;
                  }
              }
          }

        if (r != NULL && r->type == OBJ_FUNC)
          {
            obj_t *oj = sf_objstore_req ();

            oj->type = OBJ_HFF;
            oj->v.o_hff.f = r;

            oj->v.o_hff.al = 1;
            oj->v.o_hff.args = SFMALLOC (sizeof (*oj->v.o_hff.args));
            *oj->v.o_hff.args = o;
            IR (o);

            return oj;
          }
        else
          return r;
      }
      break;

    default:
      break;
    }

  return NULL;
}

void
container_set (obj_t *p, char *n, obj_t *v, vm_t *vm)
{
  switch (p->type)
    {
    case OBJ_COBJ:
      {
        cobj_t *co = p->v.o_cobj.v;

        int saw_var = 0;
        size_t vidx = 0;

        for (int i = 0; i < co->svl; i++)
          if (!strcmp (co->slots[i], n))
            {
              saw_var = 1;
              vidx = i;
              break;
            }

        if (saw_var)
          {
            DR (co->vals[vidx], vm);
            co->vals[vidx] = v;
          }
        else
          {
            if (co->svl >= co->svc)
              {
                co->svc += SF_VM_NAME_CAP;
                co->slots
                    = SFREALLOC (co->slots, co->svc * sizeof (*co->slots));
                co->vals = SFREALLOC (co->vals, co->svc * sizeof (*co->vals));
              }

            co->slots[co->svl] = n;
            co->vals[co->svl++] = v;
          }
      }
      break;

    default:
      break;
    }
}

SF_API obj_t *
sqr_access (obj_t *p, obj_t *v)
{
  obj_t *r = NULL;
  switch (p->type)
    {
    case OBJ_ARRAY:
      {
        assert (v->type == OBJ_CONST && v->v.o_const.v.type == CONST_INT);
        int idx = v->v.o_const.v.v.c_int.v;

        array_t *a = p->v.o_array.v;

        assert (a->len > idx);
        r = a->vals[idx];
      }
      break;

    default:
      break;
    }

  return r;
}

SF_API void
sqr_set (obj_t *p, obj_t *i, obj_t *val, vm_t *vm)
{
  /* TODO */
}