#include "bytecode.h"

SF_API vm_t
sf_vm_new ()
{
  vm_t v;

  v.globals_cap = SF_VM_GLOBALS_CAP;
  v.globals = SFMALLOC (v.globals_cap * sizeof (*v.globals));
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

  while (1)
    {
      switch (i.op)
        {
        case OP_RETURN:
          goto end;
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

            push (vm, d_obj);
          }
          break;

        case OP_JUMP_IF_FALSE:
          {
            obj_t *p = pop (vm);

            if (sf_obj_isfalse (*p))
              vm->ip = i.a - 1;
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
            IR (val);

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
                  fr->locals[i.a] = val;
                }
                break;

              default:
                break;
              }
          }
          break;

        case OP_LOAD:
          {
            push (vm, vm->globals[i.a]);
          }
          break;

        case OP_CALL:
          {
            size_t argc = i.a;
            obj_t *name = pop (vm);

            obj_t *args[64];
            size_t al = 0;

            while (al < argc)
              args[argc - al++ - 1] = pop (vm);

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

                          case NF_ARG_ANY:
                            {
                              obj_t *r = f->v.native.v.f_anyarg (args, al);

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

        case OP_ADD_1:
          {
            obj_t *p = pop (vm);

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
              }
          }
          break;

        case OP_ADD:
          {
            obj_t *l = pop (vm);
            obj_t *r = pop (vm);

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
              }
          }
          break;

        case OP_SUB:
          {
            obj_t *l = pop (vm);
            obj_t *r = pop (vm);

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
              }
          }
          break;

        case OP_MUL:
          {
            obj_t *l = pop (vm);
            obj_t *r = pop (vm);

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
              }
          }
          break;

        default:
          break;
        }

      i = vm->insts[++vm->ip];
    }

end:;

  vm->ip = fr->return_ip;

  //   while (vm->sp > fr->stack_base)
  //     pop (vm);
  //   //   vm->sp = fr->stack_base;
  //   sf_vm_popframe (vm);
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
sf_vm_popframe (vm_t *vm)
{
  frame_t *f = &vm->frames[vm->fp - 1];

  for (int i = 0; i < f->locals_count; i++)
    if (f->locals[i] != NULL)
      DR (f->locals[i]);

  sf_vm_framefree (&vm->frames[--vm->fp]);
}

SF_API void
sf_vm_framefree (frame_t *f)
{
  for (int i = 0; i < f->locals_count; i++)
    if (f->locals[i] != NULL)
      DR (f->locals[i]);

  SFFREE (f->locals);
}