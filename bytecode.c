#include "bytecode.h"
#include "ast.h"
#include "codegen.h"
#include "mod.h"
#include "natives.h"
#include "progdata.h"
#include "token.h"

static const_t __sf_none_obj = (const_t){ .type = CONST_NONE };

void _sf_call_fun (vm_t *_VM, obj_t *_Name, obj_t **_Args, size_t _ArgCount);

#define STDWRAP_BEGIN                                                         \
  {                                                                           \
    sf_std_push (vm->std, i.meta.line, i.meta.offset);

#define STDWRAP_END                                                           \
  sf_std_pop (vm->std);                                                       \
  }

#define SET_ERROR(...)                                                        \
  do                                                                          \
    {                                                                         \
      sf_vm_seterr (vm, __VA_ARGS__);                                         \
      goto end3;                                                              \
    }                                                                         \
  while (0);

SF_API vm_t
sf_vm_new ()
{
  vm_t v;

  v.globals_cap = SF_VM_GLOBALS_CAP;
  v.globals = SFMALLOC (v.globals_cap * sizeof (*v.globals));
  v.htc = SF_VM_HT_CAP;
  v.htl = 0;
  v.hts = SFMALLOC (v.htc * sizeof (*v.hts));

  for (size_t i = 0; i < v.htc; i++)
    v.hts[i] = NULL;

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
  v.mod_store = sf_modstore_new ();

  v.pg = SFMALLOC (sizeof (*v.pg));
  *v.pg = sf_progdata_new ();

  v.std = SFMALLOC (sizeof (*v.std));
  *v.std = sf_std_new ();

  v.signals.continue_exec = 1;
  v.signals.errn = 0;

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
    case OP_STORE_SQR:
      printf ("OP_STORE_SQR: ");
      break;
    case OP_RANGE_FAST:
      printf ("OP_RANGE_FAST: ");
      break;
    case OP_RANGE:
      printf ("OP_RANGE: ");
      break;
    case OP_GET_ITER:
      printf ("OP_GET_ITER: ");
      break;
    case OP_LOAD_ITER_NEXT:
      printf ("OP_LOAD_ITER_NEXT: ");
      break;
    case OP_IMPORT:
      printf ("OP_IMPORT: '%s' ", i.c);
      break;
    case OP_IMPORT_ALIAS:
      printf ("OP_IMPORT_ALIAS: '%s' ", i.c);
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
      printf ("(%d %p) ", i, &vm->insts[i]);
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
      D (sf_vm_print_inst (vm->insts[vm->ip]));
      D (printf ("vm_ip: %lu\n", vm->ip));
      D (printf ("error: popping from empty stack\n"));
      exit (EXIT_FAILURE);
    }

  return vm->stack[--vm->sp];
}

SF_API void
sf_vm_exec_single_frame (vm_t *vm)
{
  frame_t *fr = vm->frames[vm->fp - 1];
  instr_t i = vm->insts[vm->ip];

  if (vm->meta.g_slot >= vm->globals_cap)
    {
      vm->globals_cap += SF_VM_GLOBALS_CAP;
      vm->globals
          = SFREALLOC (vm->globals, vm->globals_cap * sizeof (*vm->globals));
    }

start:;
  while (vm->signals.continue_exec)
    {
      STDWRAP_BEGIN
      // D (printf ("%lu\n", vm->ip));
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
              {
                push (vm, o = sf_objstore_req_forconst (&__sf_none_obj));
                IR (o);
              }

            sf_std_pop (vm->std);
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
            // D (sf_obj_print (*val));
            // D (printf ("%d\n", val->meta.ref_count));

            if (vm->globals[i.a] != NULL)
              {
                // D (sf_obj_print (*vm->globals[i.a]));
                // D (printf ("%d\n", vm->globals[i.a]->meta.ref_count));
                DR (vm->globals[i.a], vm);
              }
            vm->globals[i.a] = val;
            /* already IR'ed */

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

            if (i.b == 0) /* normal store */
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

        case OP_STORE_SQR:
          {
            obj_t *par = pop (vm);
            obj_t *idx = pop (vm);
            obj_t *val = pop (vm);

            sqr_set (par, idx, val, vm);

            if (!vm->signals.continue_exec)
              goto end3;
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
            D (printf ("[OP_LOAD_NAME] c{'%s'} a{%d} b{%d}", i.c, i.a, i.b))
            obj_t *o = NULL;

            if (i.b == 0)
              o = fr->n.vals[i.a];
            else
              {
                // D (sf_vm_print_inst (i));
                int j = vm->fp - 1;
                frame_t *ff = vm->frames[j];

                while (j > -1 && ff->type != FRAME_NAME)
                  ff = vm->frames[--j];

                D (printf ("%d", ff->is_class));
                if (ff == NULL || j == -1)
                  {
                    SET_ERROR ("name '%s' not found.", i.c);
                  }

                // if (ff->n.names[i.a] != NULL && strcmp (ff->n.names[i.a],
                // i.c))
                // {
                //   for (size_t j = 0; j < ff->n.nvl; j++)
                //     {
                //       if (ff->n.names[j] != NULL)
                //         printf ("(%s) (%s)\n", ff->n.names[j], i.c);
                //       if (ff->n.names[j] != NULL
                //           && !strcmp (ff->n.names[j], i.c))
                //         o = ff->n.vals[j];
                //     }
                // }
                // else
                assert (ff->n.names[i.a] != NULL
                        && !strcmp (ff->n.names[i.a], i.c));
                o = ff->n.vals[i.a];
              }

            assert (o != NULL);
            push (vm, o);

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

                push (vm, o = vm->frames[vm->fp - 1 - i.b]->l.locals[i.a]);
              }

            if (o != NULL)
              IR (o);
          }
          break;

        case OP_LOAD_FUNC_CODED:
          {
            obj_t *o = sf_objstore_req ();
            o->type = OBJ_FUNC;
            o->v.o_fun.v = sf_fun_new (FUN_CODED);
            o->v.o_fun.v->v.coded.lp = i.a;
            o->v.o_fun.v->argc = o->v.o_fun.v->argl = i.b;

            int fi = vm->fp - 1;
            while (fi >= 0 && vm->frames[fi]->type == FRAME_LOCAL)
              fi--;

            if (fi < 0)
              fi = 0;
            o->v.o_fun.v->parent_frame = vm->frames[fi];

            IR (o);
            push (vm, o);

            // obj_t *o = sf_objstore_req (&__sf_none_obj);
            // IR (o);
            // push (vm, o);
          }
          break;

        case OP_CALL:
          {
            // here;
            size_t argc = i.a;
            obj_t *name = pop (vm);

            // sf_obj_print (*name);
            int saw_modwrap = 0;
            obj_t *ppres = NULL;

            if (name->type == OBJ_MODWRAP)
              {
                saw_modwrap = 1;
                sf_vm_addframe (vm, *name->v.o_mw.v);
                ppres = name;
                name = name->v.o_mw.f;
              }

            // IR (name);

            obj_t *args[64];
            size_t al = 0;

            // assert (argc < 64 && "only 64 arguments allowed in a function");
            if (argc >= 64)
              {
                SET_ERROR ("only 64 arguments allowed in a function.\n");
              }

            while (al < argc)
              {
                args[al] = pop (vm);
                /* reuse the IR from stack push */
                // IR (args[al]);
                al++;
                // sf_obj_print (*args[al - 1]);
                // IR (args[al++]);
              }

            IR (name);

            _sf_call_fun (vm, name, args, al); /* handles DR (name, vm); */

            if (!vm->signals.continue_exec)
              goto end3;

            for (size_t j = 0; j < al; j++)
              {
                // D (printf ("%d\n", args[j]->meta.ref_count));
                DR (args[j], vm);
              }

            if (saw_modwrap)
              {
                vm->fp--;
                DR (ppres, vm);
              }
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

            // if (r->type == l->type && l->type == OBJ_CONST
            //     && l->v.o_const.v.type == r->v.o_const.v.type
            //     && l->v.o_const.v.type == CONST_INT)
            if (OBJ_IS_INT (l) && OBJ_IS_INT (r))
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
            else if (OBJ_IS_STRING (l))
              {
                if (OBJ_IS_STRING (r))
                  {
                    char *lstr = l->v.o_const.v.v.c_str.v;
                    char *rstr = r->v.o_const.v.v.c_str.v;

                    size_t ln = strlen (lstr);
                    size_t rn = strlen (rstr);

                    char *nstr = SFMALLOC ((ln + rn + 1) * sizeof (*nstr));

                    strncpy (nstr, rstr, rn);
                    strncpy (nstr + rn, lstr, ln);
                    nstr[ln + rn] = '\0';

                    obj_t *o = sf_objstore_req ();
                    o->type = OBJ_CONST;
                    o->v.o_const.v.type = CONST_STRING;
                    o->v.o_const.v.v.c_str.v = nstr;

                    push (vm, o);
                    IR (o);
                  }
                else
                  {
                    SET_ERROR (
                        "string addition with type %d is not supported\n",
                        r->type);
                  }
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
            D (printf ("[OP_LOAD_BUILDCLASS]"));
            frame_t nf = sf_frame_new_name ();
            nf.is_class = 1;

            nf.return_ip = i.a; /* buildclass_end location */
            sf_vm_addframe (vm, nf);

            vm->ip++;
            sf_vm_exec_single_frame (vm);
          }
          break;

        case OP_LOAD_BUILDCLASS_END:
          {
            frame_t f = *vm->frames[vm->fp - 1];
            assert (f.type == FRAME_NAME && f.is_class);

            class_t *cl = sf_class_new ();

            cl->svl = f.n.nvl;
            cl->svc = f.n.nvl;

            frame_t *ff = vm->frames[vm->fp - 2];
            // if (ff != NULL && ff->type == FRAME_NAME && ff->is_mod)
            {
              cl->par_fr = ff;
            }

            // cl->slots = SFMALLOC (sizeof (*cl->slots));
            // cl->vals = SFMALLOC (sizeof (*cl->vals));

            // for (size_t j = 0; j < f.n.nvl; j++)
            //   {
            //     cl->vals[j] = f.n.vals[j];
            //     cl->slots[j] = f.n.names[j];
            //   }

            cl->slots = SFMALLOC (f.n.nvl * sizeof (*cl->slots));
            cl->vals = SFMALLOC (f.n.nvl * sizeof (*cl->vals));

            for (size_t j = 0; j < f.n.nvl; j++)
              {
                if (f.n.names[j] == NULL)
                  continue;

                cl->slots[j] = SFSTRDUP (f.n.names[j]);
                cl->vals[j] = f.n.vals[j];
                IR (f.n.vals[j]);
              }

            cl->name = SFSTRDUP (vm->insts[i.a].c);

            obj_t *o = sf_objstore_req ();
            o->type = OBJ_CLASS;
            o->v.o_class.v = cl;

            push (vm, o);
            IR (o);

            // sf_vm_popframe (vm);
            vm->fp--;

            sf_std_pop (vm->std);
            goto end2;
          }
          break;

        case OP_DOT_ACCESS:
          {
            obj_t *l = pop (vm);
            // D (sf_obj_print (*l); printf ("%d\n", l->meta.ref_count));
            char *name = i.c;
            // D (printf ("%s\n", name));

            obj_t *o = container_access (l, name);

            if (o == NULL)
              {
                // printf ("member '%s' does not exist.\n", name);
                sf_vm_seterr (vm, "member '%s' does not exist.\n", name);
                // printf ("line %d: %s\n", i.meta.line,
                //         vm->pg->lines[i.meta.line]);
                goto end3;
              }

            push (vm, o);
            IR (o);
            DR (l, vm);
          }
          break;

        case OP_LOAD_ARRAY:
          {
            array_t *ar = sf_array_withsize (i.a);
            // for (int j = i.a - 1; j >= 0; j--)
            //   {
            //     ar->vals[c++] = pop (vm);
            //   }

            for (int j = i.a - 1; j > -1; j--)
              ar->vals[j] = pop (vm);

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

            obj_t *o = sqr_access (par, idx, vm);

            if (!vm->signals.continue_exec)
              goto end3;

            push (vm, o);
            IR (o);

            DR (idx, vm);
            DR (par, vm);
          }
          break;

        case OP_RANGE_FAST:
          {
            int lv = i.a;
            int rv = i.b;
            int step = (int)i.c;

            if (step == 0)
              {
                // printf ("range step cannot be zero\n");
                // exit (EXIT_FAILURE);
                // sf_vm_seterr (vm, "range step cannot be zero\n");
                // goto end3;

                SET_ERROR ("range step cannot be 0");
              }

            if (lv < rv)
              {
                if (step < 0)
                  SET_ERROR ("infinite range [%d..%d step %d]", lv, rv, step);

                array_t *a = sf_array_withsize (((rv - 1 - lv) / step) + 1);

                int c = 0;
                for (int j = lv; j < rv; j += step)
                  {
                    obj_t *o
                        = sf_objstore_req_forconst ((const_t *)(const_t[]){ {
                            .type = CONST_INT,
                            .v.c_int.v = j,
                        } });

                    if (o == NULL)
                      {
                        o = sf_objstore_req ();
                        o->type = OBJ_CONST;
                        o->v.o_const.v.type = CONST_INT;
                        o->v.o_const.v.v.c_int.v = j;
                      }

                    a->vals[c++] = o;
                    IR (o);
                  }

                if (a->len != c)
                  a->len = c;

                obj_t *o = sf_objstore_req ();
                o->type = OBJ_ARRAY;
                o->v.o_array.v = a;

                push (vm, o);
                IR (o);
              }
            else
              {
                if (step > 0)
                  SET_ERROR ("infinite range [%d..%d step %d]", lv, rv, step);

                array_t *a = sf_array_withsize (((lv - 1 - rv) / (-step)) + 1);

                int c = 0;
                for (int j = lv; j > rv; j += step)
                  {
                    obj_t *o
                        = sf_objstore_req_forconst ((const_t *)(const_t[]){ {
                            .type = CONST_INT,
                            .v.c_int.v = j,
                        } });

                    if (o == NULL)
                      {
                        o = sf_objstore_req ();
                        o->type = OBJ_CONST;
                        o->v.o_const.v.type = CONST_INT;
                        o->v.o_const.v.v.c_int.v = j;
                      }

                    a->vals[c++] = o;
                    IR (o);
                  }

                if (a->len != c)
                  a->len = c;

                obj_t *o = sf_objstore_req ();
                o->type = OBJ_ARRAY;
                o->v.o_array.v = a;

                push (vm, o);
                IR (o);
              }
          }
          break;

        case OP_RANGE:
          {
            obj_t *lv = NULL;
            obj_t *rv = NULL;
            obj_t *stp = NULL;

            if (i.a)
              {
                stp = pop (vm);
              }

            rv = pop (vm);
            lv = pop (vm);

            // D (sf_obj_print (*rv));
            // D (sf_obj_print (*lv));

            if (OBJ_IS_INT (lv) && OBJ_IS_INT (rv))
              {
                if (stp != NULL && !OBJ_IS_INT (stp))
                  {
                    SET_ERROR ("expected step to be an integer");
                  }

                int lvi = lv->v.o_const.v.v.c_int.v;
                int rvi = rv->v.o_const.v.v.c_int.v;
                int stpi = stp != NULL ? stp->v.o_const.v.v.c_int.v : 1;

                if (lvi < rvi)
                  {
                    array_t *a
                        = sf_array_withsize (((rvi - 1 - lvi) / stpi) + 1);

                    int c = 0;
                    for (int j = lvi; j < rvi; j += stpi)
                      {
                        obj_t *o = sf_objstore_req_forconst (
                            (const_t *)(const_t[]){ {
                                .type = CONST_INT,
                                .v.c_int.v = j,
                            } });

                        if (o == NULL)
                          {
                            o = sf_objstore_req ();
                            o->type = OBJ_CONST;
                            o->v.o_const.v.type = CONST_INT;
                            o->v.o_const.v.v.c_int.v = j;
                          }

                        a->vals[c++] = o;
                        IR (o);
                      }

                    if (a->len != c)
                      a->len = c;

                    obj_t *o = sf_objstore_req ();
                    o->type = OBJ_ARRAY;
                    o->v.o_array.v = a;

                    push (vm, o);
                    IR (o);
                  }
              }

            if (i.a)
              {
                DR (stp, vm);
              }

            DR (rv, vm);
            DR (lv, vm);
          }
          break;

        case OP_GET_ITER:
          {
            obj_t *v = pop (vm);

            obj_t *o = sf_objstore_req ();
            o->type = OBJ_ITER;
            o->v.o_iter.v = sf_iter_new (v);

            push (vm, o);
            IR (o);
          }
          break;

        case OP_LOAD_ITER_NEXT:
          {
            obj_t *iter = pop (vm);

            assert (iter->type == OBJ_ITER);
            obj_t *n = sf_iter_next (&iter->v.o_iter.v);

            if (n == NULL)
              {
                vm->ip = i.a - 1;
                DR (iter, vm);
              }
            else
              {
                push (vm, iter);
                if (i.b == 1) /* just push the value */
                  {
                    push (vm, n);
                    IR (n);
                  }
                else
                  {
                    /* split the nth iterable */
                    switch (n->type)
                      {
                      case OBJ_ARRAY:
                        {
                          array_t *na = n->v.o_array.v;
                          assert (na->len == i.b
                                  && "Insufficient values to unpack");

                          for (int j = i.b - 1; j > -1; j--)
                            {
                              obj_t *ji = na->vals[j];
                              IR (ji);
                              push (vm, ji);
                            }
                        }
                        break;

                      default:
                        break;
                      }
                  }
              }
          }
          break;

        case OP_IMPORT:
          {
            const char *path = i.c;
            const char *alias = vm->insts[++vm->ip].c;

            D (printf ("[OP_IMPORT] path = '%s', alias = '%s'", path, alias));

            if (sf_modstore_haskey (vm->mod_store, vm->ip))
              {
                obj_t *mg = sf_modstore_get (vm->mod_store, vm->ip);

                IR (mg);
                push (vm, mg);
                goto end3;
              }

            FILE *f = fopen (path, "r");
            // D (printf ("%s\n", path));

            if (f == NULL)
              {
                SET_ERROR ("error reading file: %s", strerror (errno));
              }

            fseek (f, 0, SEEK_END);
            long pos = ftell (f);

            // D (printf ("file size: %zu\n", pos));
            fseek (f, 0, SEEK_SET);

            char *buf = SFMALLOC ((pos + 2) * sizeof (char));
            fread (buf, sizeof (char), pos, f);

            buf[pos++] = '\n';
            buf[pos] = '\0';

            fclose (f);

            TokenSM *smt = sf_statem_token_new (buf);
            // printf ("%s\n", smt->raw);
            sf_token_gen (smt);
            token_t *vp = smt->vals;

            // for (int i = 0; i < smt->vl; i++)
            //   {
            //     sf_token_print (smt->vals[i]);
            //   }

            // frame_t *frms = vm->frames;
            // size_t frc = vm->frame_cap;
            // size_t vmfp = vm->fp;

            // obj_t **gp = vm->globals;
            // hashtable_t **hts = vm->hts;
            // size_t htl = vm->htl;

            // vm->globals = SFMALLOC (vm->globals_cap * sizeof
            // (*vm->globals));

            // for (size_t i = 0; i < vm->globals_cap; i++)
            //   vm->globals[i] = NULL;

            // vm->htl = 1;
            // vm->hts = SFMALLOC (vm->htc * sizeof (*vm->hts));
            // vm->hts[0] = sf_ht_new ();

            // for (size_t i = vm->htl; i < vm->htc; i++)
            //   vm->hts[i] = NULL;

            // vm->frames = SFMALLOC (vm->frame_cap * sizeof (*vm->frames));
            // vm->fp = 0;

            // sf_natives_add_tovm (vm);

            StmtSM *stt = sf_ast_gen (smt);
            stmt_t *stt_vals = stt->vals;

            // approach 0
            if (0)
              {
                // D (printf ("%lu\n", stt->vl));
                // for (size_t i = 0; i < stt->vl; i++)
                //   sf_stmt_print (stt->vals[i]);

                // vm_t mm = sf_vm_new ();
                // mm.meta.slot = SF_VM_SLOT_NAME;
                // sf_natives_add_tovm (&mm);

                // mm.fp = 1;
                // sf_vm_gen_bytecode (&mm, stt);
                // mm.fp = 0;

                // // for (int i = 0; i < mm.inst_len; i++)
                // //   sf_vm_print_inst (mm.insts[i]);

                // frame_t top = sf_frame_new_name ();
                // top.pop_ret_val = 0;
                // top.return_ip = mm.inst_len - 1;
                // top.stack_base = mm.sp;

                // sf_vm_addframe (&mm, top);
                // sf_vm_exec_single_frame (&mm);

                // frame_t *bf = &mm.frames[mm.fp - 1];

                // // // D (printf ("%lu\n", bf->n.nvl));
                // // // for (int i = 0; i < bf->n.nvl; i++)
                // // //   printf ("%s\n", bf->n.names[i]);

                // mod_t *mod = sf_mod_new ();

                // mod->name = SFSTRDUP (alias);
                // mod->slots = SFMALLOC (bf->n.nvc * sizeof (*mod->slots));
                // mod->vals = SFMALLOC (bf->n.nvc * sizeof (*mod->vals));

                // for (int i = 0; i < bf->n.nvl; i++)
                //   {
                //     mod->slots[i] = SFSTRDUP (bf->n.names[i]);
                //     mod->vals[i] = bf->n.vals[i];
                //     IR (mod->vals[i]);
                //   }

                // mod->svc = bf->n.nvc;
                // mod->svl = bf->n.nvl;

                // obj_t *o = sf_objstore_req ();
                // o->type = OBJ_MOD;
                // o->v.o_mod.v = mod;

                // push (vm, o);
                // IR (o);

                // sf_vm_framefree (bf, &mm);

                // for (int i = 0; i < mm.globals_cap; i++)
                //   {
                //     if (mm.globals[i] != NULL)
                //       {
                //         DR (mm.globals[i], vm);
                //       }
                //   }

                // SFFREE (mm.globals);

                // for (int i = 0; i < mm.htc; i++)
                //   {
                //     if (mm.hts[i] != NULL)
                //       sf_ht_free (mm.hts[i]);
                //   }

                // SFFREE (mm.hts);
                // SFFREE (mm.insts);
                // SFFREE (mm.map_consts);
                // SFFREE (mm.stack);
                // SFFREE (mm.frames);
              }

            // approach 1
            size_t ip = vm->inst_len;

            PRESERVE (vm);
            vm->meta.slot = SF_VM_SLOT_NAME;

            hashtable_t **hts = vm->hts;
            size_t htsp = vm->htl;

            vm->hts = SFMALLOC (vm->htc * sizeof (*vm->hts));
            vm->htl = 0;

            for (size_t i = 0; i < vm->htc; i++)
              vm->hts[i] = NULL;

            vm->hts[vm->htl++] = hts[0];
            vm->hts[vm->htl++] = sf_ht_new ();

            /**
             * this adds bytecode to the end
             * of current generated bytecode
             * so control has
             * [main program bytes]...[module bytes]
             */
            sf_vm_gen_bytecode (vm, stt);

            // sf_vm_print_b (vm);

            for (size_t i = 1; i < vm->htc; i++)
              if (vm->hts[i] != NULL)
                sf_ht_free (vm->hts[i]);

            SFFREE (vm->hts);

            vm->hts = hts;
            vm->htl = htsp;

            // for (size_t i = ip; i < vm->inst_len; i++)
            //   sf_vm_print_inst (vm->insts[i]);

            RESTORE (vm);

            frame_t fr = sf_frame_new_name ();
            fr.is_mod = 1;
            fr.pop_ret_val = 1;
            fr.return_ip = vm->ip;
            fr.stack_base = vm->sp;

            // D (printf ("%d\n", ip));
            vm->ip = ip;

            // frame_t *fpres = vm->frames;
            // size_t fpc = vm->fp;

            // vm->frames = SFMALLOC (vm->frame_cap * sizeof (*vm->frames));
            // vm->fp = 0;

            sf_vm_addframe (vm, fr);
            frame_t *bf = vm->frames[vm->fp - 1];

            sf_vm_exec_single_frame (vm);

            // D (printf ("%d\n", vm->ip));

            // SFFREE (vm->frames);
            // vm->frames = fpres;
            // vm->fp = fpc;

            // // D (printf ("%lu\n", bf->n.nvl));
            // // for (int i = 0; i < bf->n.nvl; i++)
            // //   printf ("%s\n", bf->n.names[i]);

            mod_t *mod = sf_mod_new ();

            mod->name = SFSTRDUP (alias);
            mod->slots = SFMALLOC (bf->n.nvc * sizeof (*mod->slots));
            mod->vals = SFMALLOC (bf->n.nvc * sizeof (*mod->vals));

            for (int i = 0; i < bf->n.nvl; i++)
              {
                if (bf->n.names[i] == NULL)
                  continue;

                mod->slots[i] = SFSTRDUP (bf->n.names[i]);
                mod->vals[i] = bf->n.vals[i];
                IR (mod->vals[i]);
              }

            mod->svc = bf->n.nvc;
            mod->svl = bf->n.nvl;

            obj_t *o = sf_objstore_req ();
            o->type = OBJ_MOD;
            o->v.o_mod.v = mod;

            push (vm, o);
            IR (o);

            IR (o);
            sf_modstore_add (vm->mod_store, vm->ip, o);

            /**
             * hide the frame and save it
             * from deletion
             */
            vm->fp--;

            /* the frame (which will not be freed)
              is allocated to mod */
            mod->fr = bf;

            // for (size_t i = 0; i < mod->fr->n.nvl; i++)
            //   D (printf ("%s\n", mod->fr->n.names[i]));

            // vm->frames = frms;
            // vm->frame_cap = frc;
            // vm->fp = vmfp;
            // vm->globals = gp;
            // vm->hts = hts;
            // vm->htl = htl;

            SFFREE (buf);

            for (size_t i = 0; i < stt->vl; i++)
              sf_stmt_free (&stt_vals[i]);

            SFFREE (stt_vals);
            SFFREE (stt);

            for (size_t i = 0; i < smt->vc; i++)
              {
                if (vp[i].type != -1)
                  sf_token_free (&vp[i]);
              }

            SFFREE (vp);
            SFFREE (smt);
          }
          break;

        case OP_IMPORT_ALIAS:
          {
            assert (
                0 && "control shouldn't reach here (possible ip corruption)");
          }
          break;

        default:
          break;
        }

    end3:;
      if (!vm->signals.continue_exec)
        {
          printf ("error: %s\n", vm->err);

          printf ("stack trace: \n");
          for (int j = vm->std->ll - 1; j >= 0; j--)
            {
              char *p = vm->pg->lines[vm->std->lis[j] + vm->std->lof[j]];

              while (*p && isspace (*p))
                p++;

              printf ("%lu | %s\n", vm->std->lis[j] + vm->std->lof[j] + 1, p);
            }

          exit (EXIT_FAILURE);
        }

      i = vm->insts[++vm->ip];

      STDWRAP_END
    }

end:;
  if (fr->pop_ret_val)
    {
      obj_t *p = pop (vm);
      if (p != NULL)
        DR (p, vm);
    }

end2:;
  vm->ip = fr->return_ip;
}

SF_API void
sf_vm_exec_frame_top (vm_t *vm)
{
  frame_t *fr = vm->frames[vm->fp - 1];
  instr_t i = vm->insts[vm->ip];

  if (vm->meta.g_slot >= vm->globals_cap)
    {
      vm->globals_cap += SF_VM_GLOBALS_CAP;
      vm->globals
          = SFREALLOC (vm->globals, vm->globals_cap * sizeof (*vm->globals));
    }

start:;
  sf_vm_exec_single_frame (vm);

end:;
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
            {
              if (vm->globals[i]->type == OBJ_CLASS)
                {
                  continue;
                }

              if (vm->globals[i]->type == OBJ_COBJ)
                {
                  cobj_t *c = vm->globals[i]->v.o_cobj.v;

                  // D (printf ("%d\n", c->p->svl));

                  // for (int i = 0; i < c->p->svl; i++)
                  //   D (printf ("%s\n", c->p->slots[i]));
                  DR (vm->globals[i], vm);
                  vm->globals[i] = NULL;
                }
            }
        }

      for (size_t i = 0; i < vm->globals_cap; i++)
        {
          if (vm->globals[i] != NULL)
            {
              DR (vm->globals[i], vm);
            }
        }

      SFFREE (vm->globals);
      vm->globals_cap = 0;
    }
  else if (vm->fp > 1)
    {
      sf_vm_popframe (vm);
      fr = vm->frames[vm->fp - 1];
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
  f.is_mod = 0;
  f.is_class = 0;

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
  f.is_mod = 0;
  f.is_class = 0;

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

  vm->frames[vm->fp] = SFMALLOC (sizeof (**vm->frames));
  *vm->frames[vm->fp++] = f;
}

SF_API void
sf_vm_popframe (vm_t *vm)
{
  // frame_t *f = &vm->frames[vm->fp - 1];

  // for (int i = 0; i < f->locals_count; i++)
  //   if (f->locals[i] != NULL)
  //     DR (f->locals[i], vm);

  // frame_t *fr = vm->frames[vm->fp - 1];
  // sf_vm_framefree (fr, vm);
  // SFFREE (fr);
  // vm->frames[vm->fp - 1] = NULL;
  --vm->fp;
}

SF_API void
sf_vm_framefree (frame_t *f, vm_t *vm)
{
  return;
  // D (printf ("%p", f));
  // D (printf ("%lu\n", f->locals_count));

  switch (f->type)
    {
    case FRAME_LOCAL:
      {
        here;
        // if (f->l.locals != NULL)
        for (int i = 0; i < f->l.locals_cap; i++)
          {
            if (f->l.locals[i] != NULL)
              {
                // sf_obj_print (*f->l.locals[i]);
                DR (f->l.locals[i], vm);
              }
          }

        // here;

        // if (f->l.locals != NULL)
        {
          SFFREE (f->l.locals);
          // f->l.locals = NULL;
          // f->l.locals_count = 0;
          // f->l.locals_cap = 0;
        }
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

        // if (c->par_fr == NULL)
        {
          for (int i = 0; i < c->svl; i++)
            if (c->slots[i] != NULL && !strcmp (c->slots[i], name))
              return c->vals[i];
        }
        // else
        //   {
        //     obj_t *r = NULL;

        //     for (int i = 0; i < c->svl; i++)
        //       {
        //         if (!strcmp (c->slots[i], name))
        //           {
        //             r = c->vals[i];
        //             break;
        //           }
        //       }

        //     obj_t *j = sf_objstore_req ();
        //     j->type = OBJ_MODWRAP;
        //     j->v.o_mw.v = c->par_fr;
        //     j->v.o_mw.f = r;
        //   }
      }
      break;

    case OBJ_COBJ:
      {
        cobj_t *c = o->v.o_cobj.v;
        obj_t *r = NULL;

        /* check cobj dict */
        for (int i = 0; i < c->svl; i++)
          {
            if (c->slots[i] != NULL && !strcmp (c->slots[i], name))
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
                if (c->p->slots[i] != NULL && !strcmp (c->p->slots[i], name))
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
            IR (r);

            oj->v.o_hff.al = 1;
            oj->v.o_hff.args = SFMALLOC (sizeof (*oj->v.o_hff.args));
            *oj->v.o_hff.args = o;
            IR (o);

            // class_t *cp = c->p;

            // if (cp->par_fr != NULL)
            //   {
            //     obj_t *oo = sf_objstore_req ();
            //     oo->type = OBJ_MODWRAP;
            //     oo->v.o_mw.v = cp->par_fr;
            //     oo->v.o_mw.f = oj;

            //     IR (oj);
            //     return oo;
            //   }
            // else
            return oj;
          }
        else
          {
            /**
             * * Key not found
             * * when key is not found,
             * * return NULL because
             * * NULL is used as an
             * * indicator that
             * * key is not present
             */
            if (r != NULL)
              return r;
          }
      }
      break;

    case OBJ_MOD:
      {
        /* we can combine both function and class
        under a unified container whose purpose is to
        append module frame to the stack */
        mod_t *mo = o->v.o_mod.v;

        obj_t *r = NULL;
        for (size_t i = 0; i < mo->svl; i++)
          {
            // D (printf ("(%s)\n", mo->slots[i]));
            if (mo->slots[i] != NULL && !strcmp (mo->slots[i], name))
              {
                r = mo->vals[i];
                break;
              }
          }

        if (r != NULL && r->type == OBJ_FUNC)
          {
            obj_t *oj = sf_objstore_req ();

            oj->type = OBJ_MODHF;
            oj->v.o_modhf.f = r;
            oj->v.o_modhf.v = o;

            IR (r);
            IR (o);

            return oj;
          }
        else if (r != NULL && r->type == OBJ_CLASS)
          {
            obj_t *oj = sf_objstore_req ();

            oj->type = OBJ_MODHC;
            oj->v.o_modcf.f = r;
            oj->v.o_modcf.v = o;

            IR (r);
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

            co->slots[co->svl] = SFSTRDUP (n);
            co->vals[co->svl++] = v;
          }
      }
      break;

    default:
      break;
    }
}

SF_API obj_t *
sqr_access (obj_t *p, obj_t *v, vm_t *vm)
{
  obj_t *r = NULL;
  switch (p->type)
    {
    case OBJ_ARRAY:
      {
        assert (v->type == OBJ_CONST && v->v.o_const.v.type == CONST_INT);
        int idx = v->v.o_const.v.v.c_int.v;

        array_t *a = p->v.o_array.v;

        if (idx < 0 || (size_t)idx >= a->len)
          {
            SET_ERROR ("array index out of range");
          }

        r = a->vals[idx];
      }
      break;

    default:
      break;
    }

end3:;
  return r;
}

SF_API void
sqr_set (obj_t *p, obj_t *i, obj_t *val, vm_t *vm)
{
  switch (p->type)
    {
    case OBJ_ARRAY:
      {
        assert (i->type == OBJ_CONST && i->v.o_const.v.type == CONST_INT);

        int idx = i->v.o_const.v.v.c_int.v;
        array_t *a = p->v.o_array.v;

        if (idx < 0 || (size_t)idx >= a->len)
          {
            SET_ERROR ("array index out of range");
          }

        DR (a->vals[idx], vm);
        a->vals[idx] = val;
      }
      break;

    default:
      break;
    }

end3:;
}

void
_sf_call_fun (vm_t *vm, obj_t *name, obj_t **args, size_t argc)
{
  instr_t inst = vm->insts[vm->ip];

  switch (name->type)
    {
    case OBJ_FUNC:
      {
        fun_t *f = name->v.o_fun.v;
        if (argc >= 64)
          {
            SET_ERROR ("only 64 arguments allowed in a function");
          }

        int remove_pf = 0;

        if (f->type == FUN_CODED)
          {
            int fi = vm->fp - 1;
            while (fi > -1 && vm->frames[fi] != f->parent_frame)
              --fi;

            if (fi == -1)
              {
                sf_vm_addframe (vm, *f->parent_frame);
                remove_pf = 1;
                // here;
              }
          }

        switch (f->type)
          {
          case FUN_NATIVE:
            {
              /* no need to push to stack for native functions */
              int nf_type = f->v.native.nf_type;
              int scc = f->v.native.scc;

              obj_t *r = NULL;
              switch (nf_type)
                {
                case NF_ARG_1:
                  {
                    r = f->v.native.v.f_onearg (args[0]);
                  }
                  break;

                case NF_ARG_2:
                  {
                    r = f->v.native.v.f_twoarg (args[0], args[1]);
                  }
                  break;

                case NF_ARG_3:
                  {
                    r = f->v.native.v.f_threearg (args[0], args[1], args[2]);
                  }
                  break;

                case NF_ARG_ANY:
                  {
                    r = f->v.native.v.f_anyarg (args, argc);
                  }
                  break;

                default:
                  break;
                }

              if (inst.b == 0)
                {
                  if (r != NULL)
                    DR (r, vm);
                }
              else
                {
                  if (r == NULL)
                    r = sf_objstore_req_forconst (&__sf_none_obj);

                  push (vm, r);
                  IR (r);
                }
            }
            break;

          case FUN_CODED:
            {
              // here;
              for (size_t i = 0; i < argc; i++)
                {
                  push (vm, args[i]);
                  IR (args[i]);
                }

              size_t lp = f->v.coded.lp;

              frame_t cf = sf_frame_new_local ();
              cf.return_ip = vm->ip;
              cf.stack_base = vm->sp;
              vm->ip = lp;

              if (inst.b == 0)
                cf.pop_ret_val = 1;
              else if (inst.b == 1)
                cf.pop_ret_val = 0;

              sf_vm_addframe (vm, cf);
              sf_vm_exec_single_frame (vm);
              sf_vm_popframe (vm);
              // vm->fp--;
            }
            break;

          default:
            break;
          }

        if (remove_pf)
          {
            /* soft remove */
            --vm->fp;
          }
      }
      break;

    case OBJ_CLASS:
      {
        class_t *cl = name->v.o_class.v;
        obj_t *_init_method = container_access (name, "_init");

        if (_init_method != NULL)
          {
            D (sf_obj_print (*_init_method));
          }

        obj_t *o = sf_objstore_req ();
        o->type = OBJ_COBJ;

        cobj_t *cobj = sf_cobj_new (cl);
        o->v.o_cobj.v = cobj;

        // push (vm, o);
        // IR (o);

        args[argc++] = o;
        IR (o);

        if (_init_method != NULL)
          {
            size_t init_sp = vm->sp;
            IR (_init_method);
            _sf_call_fun (vm, _init_method, args, argc);

            while (vm->sp > init_sp)
              {
                obj_t *tmp = pop (vm);
                DR (tmp, vm);
              }
          }

        push (vm, o);
        IR (o);
      }
      break;

    case OBJ_HFF:
      {
        // here;
        obj_t *fobj = name->v.o_hff.f;
        size_t e_al = name->v.o_hff.al;
        obj_t **e_args = name->v.o_hff.args;

        for (size_t i = 0; i < e_al; i++)
          {
            if (argc >= 64)
              {
                SET_ERROR ("only 64 arguments allowed in a function\n");
              }

            args[argc++] = e_args[i];
            IR (e_args[i]);
          }

        _sf_call_fun (vm, fobj, args, argc);

        for (size_t i = 0; i < e_al; i++)
          {
            DR (e_args[i], vm);
          }
      }
      break;

    default:
      {
        D (sf_obj_print (*name));
        D (printf ("Unsupported callable"));
      }
      break;
    }

end3:;
  DR (name, vm);
}

SF_API void
sf_vm_seterr (vm_t *vm, const char *s, ...)
{
  vm->signals.continue_exec = 0;

  va_list args;
  va_start (args, s);

  vsnprintf (vm->err, sizeof (vm->err), s, args);

  va_end (args);
}

/*
void
_sf_call_fun (vm_t *vm, obj_t *name, obj_t **args, size_t argc)
{
  int saw_modwrap = 0;
  obj_t *ppres = NULL;

  if (name->type == OBJ_MODWRAP)
    {
      saw_modwrap = 1;
      sf_vm_addframe (vm, *name->v.o_mw.v);
      ppres = name;
      name = name->v.o_mw.f;
    }

  instr_t i = vm->insts[vm->ip];
  size_t al = argc;

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
                            obj_t *o
                                = sf_objstore_req_forconst (&__sf_none_obj);

                            push (vm, o);
                          }
                      }
                  }
                  break;

                case NF_ARG_2:
                  {
                    obj_t *r = f->v.native.v.f_twoarg (args[0], args[1]);

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
                            obj_t *o
                                = sf_objstore_req_forconst (&__sf_none_obj);
                          }
                      }
                  }
                  break;

                case NF_ARG_3:
                  {
                    obj_t *r
                        = f->v.native.v.f_threearg (args[0], args[1], args[2]);

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
                            obj_t *o
                                = sf_objstore_req_forconst (&__sf_none_obj);
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
                            obj_t *o
                                = sf_objstore_req_forconst (&__sf_none_obj);
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
              frt.return_ip = vm->ip;
              // D (printf ("%d\n", fr.return_ip));
              frt.stack_base = vm->sp;

              // fr = &vm->frames[vm->fp - 1];
              vm->ip = lp;

              if (i.b == 1)
                frt.pop_ret_val = 0;
              else
                frt.pop_ret_val = 1;

              sf_vm_addframe (vm, frt);
              sf_vm_exec_single_frame (vm);
              sf_vm_popframe (vm);
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
          {
            args[al++] = hf_args[j];
            IR (hf_args[j]);
          }

        // al += hf_al;
        // assert (al == f->argl);
        _sf_call_fun (vm, hf_fo, args, al);
      }
      break;

    case OBJ_MODHF:
      {
        sf_vm_addframe (vm, *name->v.o_modhf.v->v.o_mod.v->fr);
        // sf_vm_addframe (vm,
        //                 *name->v.o_modhf.f->v.o_fun.v->parent_frame);

        fun_t *f = name->v.o_modhf.f->v.o_fun.v;
        // assert (f->argl == argc);

        _sf_call_fun (vm, name->v.o_modhf.f, args, al);

        vm->fp--;
      }
      break;

    case OBJ_MODHC:
      {
        sf_vm_addframe (vm, *name->v.o_modcf.v->v.o_mod.v->fr);

        class_t *c = name->v.o_modcf.f->v.o_class.v;
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
            _sf_call_fun (vm, _init_method, args, al);
            push (vm, o);

            IR (_init_method);
            DR (_init_method, vm);
          }

        vm->fp--;
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
            _sf_call_fun (vm, _init_method, args, al);
            push (vm, o);
            // IR (o);
            IR (_init_method);
            DR (_init_method, vm);
          }
      }
      break;

    default:
      break;
    }

  if (saw_modwrap)
    {
      name = ppres;
      vm->fp--;
    }

  DR (name, vm);
}
*/