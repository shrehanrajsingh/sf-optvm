#include "natives.h"

SF_API obj_t *
sf_native_putln (obj_t *v)
{
  sf_obj_print (*v);
  putchar ('\n');

  return NULL;
}

SF_API obj_t *
sf_native_put (obj_t *v)
{
  sf_obj_print (*v);
  return NULL;
}

SF_API obj_t *
sf_native_write (obj_t **vals, size_t vl)
{
  for (size_t i = 0; i < vl; i++)
    {
      obj_t *v = vals[i];

      switch (v->type)
        {
        case OBJ_CONST:
          {
            switch (v->v.o_const.v.type)
              {
              case CONST_INT:
                printf ("%d", v->v.o_const.v.v.c_int.v);
                break;

              case CONST_FLOAT:
                printf ("%f", v->v.o_const.v.v.c_float.v);
                break;

              case CONST_BOOL:
                printf ("%s", v->v.o_const.v.v.c_bool.v ? "true" : "false");
                break;

              case CONST_STRING:
                printf ("%s", v->v.o_const.v.v.c_str.v);
                break;

              case CONST_NONE:
                printf ("none");
                break;

              default:
                break;
              }
          }
          break;

        case OBJ_FUNC:
          {
            printf ("<function '%p'>\n", v->v.o_fun.v);
          }
          break;

        default:
          break;
        }

      if (i < vl)
        putchar (' ');
    }

  putchar ('\n');

  return NULL;
}

SF_API void
sf_natives_add_tovm (vm_t *vm)
{
  {
    fun_t *f = sf_fun_new (FUN_NATIVE);
    sf_fun_addarg (f, "a");
    f->v.native.nf_type = NF_ARG_1;
    f->v.native.v.f_onearg = sf_native_putln;
    f->v.native.scc = (int)'W';

    obj_t *putln_o = sf_objstore_req ();
    putln_o->type = OBJ_FUNC;
    putln_o->v.o_fun.v = f;

    IR (putln_o);

    vval_t *s = SFMALLOC (sizeof (*s));
    s->pos = vm->meta.g_slot;
    s->slot = SF_VM_SLOT_GLOBAL;
    sf_ht_insert (vm->hts[vm->htl - 1], "putln", (void *)s);
    vm->globals[vm->meta.g_slot++] = putln_o;
  }

  {
    fun_t *f = sf_fun_new (FUN_NATIVE);
    sf_fun_addarg (f, "a");
    f->v.native.nf_type = NF_ARG_1;
    f->v.native.v.f_onearg = sf_native_put;
    f->v.native.scc = (int)'w';

    obj_t *put_o = sf_objstore_req ();
    put_o->type = OBJ_FUNC;
    put_o->v.o_fun.v = f;

    IR (put_o);

    vval_t *s = SFMALLOC (sizeof (*s));
    s->pos = vm->meta.g_slot;
    s->slot = SF_VM_SLOT_GLOBAL;
    sf_ht_insert (vm->hts[vm->htl - 1], "put", (void *)s);
    vm->globals[vm->meta.g_slot++] = put_o;
  }
}