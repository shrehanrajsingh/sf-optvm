#include <sunflower.h>

obj_t *
sf_putln (obj_t *v)
{
  sf_obj_print (*v);
  putchar ('\n');

  return NULL;
}

obj_t *
sf_put (obj_t *v)
{
  sf_obj_print (*v);
  return NULL;
}

obj_t *
sf_write (obj_t **vals, size_t vl)
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

static inline double
now_sec (void)
{
  struct timespec ts;
  clock_gettime (CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void
test3 ()
{
  FILE *f = fopen ("../../test/test.sf", "r");

  if (f == NULL)
    {
      perror ("File not found '../../test/test.sf'");
      exit (1);
    }

  fseek (f, 0, SEEK_END);
  long pos = ftell (f);

  // D (printf ("file size: %zu\n", pos));
  fseek (f, 0, SEEK_SET);

  char *buf = SFMALLOC ((pos + 2) * sizeof (char));
  fread (buf, sizeof (char), pos, f);

  buf[pos++] = '\n';
  buf[pos] = '\0';

  TokenSM *smt = sf_statem_token_new (buf);
  //   printf ("%s\n", smt->raw);
  sf_token_gen (smt);

  for (int i = 0; i < smt->vl; i++)
    {
      sf_token_print (smt->vals[i]);
    }

  StmtSM *stt = sf_ast_gen (smt);

  here;
  D (printf ("stmt len: %lu\n", stt->vl));
  for (int i = 0; i < stt->vl; i++)
    sf_stmt_print (stt->vals[i]);
  here;
  vm_t vm = sf_vm_new ();

  {
    fun_t *f = sf_fun_new (FUN_NATIVE);
    sf_fun_addarg (f, "a");
    f->v.native.nf_type = NF_ARG_1;
    f->v.native.v.f_onearg = sf_putln;
    f->v.native.scc = (int)'W';

    obj_t *putln_o = sf_objstore_req ();
    putln_o->type = OBJ_FUNC;
    putln_o->v.o_fun.v = f;

    IR (putln_o);

    vval_t *s = SFMALLOC (sizeof (*s));
    s->pos = vm.meta.g_slot;
    s->slot = SF_VM_SLOT_GLOBAL;
    sf_ht_insert (vm.hts[vm.htl - 1], "putln", (void *)s);
    vm.globals[vm.meta.g_slot++] = putln_o;
  }

  {
    fun_t *f = sf_fun_new (FUN_NATIVE);
    sf_fun_addarg (f, "a");
    f->v.native.nf_type = NF_ARG_1;
    f->v.native.v.f_onearg = sf_put;
    f->v.native.scc = (int)'w';

    obj_t *put_o = sf_objstore_req ();
    put_o->type = OBJ_FUNC;
    put_o->v.o_fun.v = f;

    IR (put_o);

    vval_t *s = SFMALLOC (sizeof (*s));
    s->pos = vm.meta.g_slot;
    s->slot = SF_VM_SLOT_GLOBAL;
    sf_ht_insert (vm.hts[vm.htl - 1], "put", (void *)s);
    vm.globals[vm.meta.g_slot++] = put_o;
  }

  vm.fp = 1;
  sf_vm_gen_bytecode (&vm, stt);
  sf_vm_print_b (&vm);
  vm.fp = 0;

  frame_t top = sf_frame_new_local ();
  top.pop_ret_val = 0;
  top.return_ip = vm.inst_len - 1;
  top.stack_base = vm.sp;
  sf_vm_addframe (&vm, top);

  double start = now_sec ();
  double end = start + 5;
  size_t c = 0;

  do
    {
      sf_vm_exec_frame_top (&vm);
      // sf_vm_exec_frame (&vm);
      // sf_vm_exec_frame (&vm);

      // printf ("%lu\n", c++);
      c++;
    }
  while (0);
  // while (now_sec () < end);
  // printf ("%lu\n", c);

  // obj_t **os = sf_get_objstore ();

  // D (sf_obj_print (*os[5]));
}

int
main (int argc, char const *argv[])
{
  sf_objstore_init ();
  test3 ();
  return 0;
}