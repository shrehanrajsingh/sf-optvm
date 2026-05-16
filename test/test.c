#include <sunflower.h>

static int _argc;
static char **_argv;

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

  stmt_t *stvls = stt->vals;
  here;
  D (printf ("stmt len: %lu\n", stt->vl));
  for (int i = 0; i < stt->vl; i++)
    sf_stmt_print (stt->vals[i]);
  here;
  vm_t vm = sf_vm_new ();

#ifdef _WIN32
  sf_vm_addsyspath (&vm, "..\\..\\test\\");
#else
  sf_vm_addsyspath (&vm, "../../test/");
#endif

  size_t n_lines = 0;
  for (int i = 0; i < pos; i++)
    if (buf[i] == '\n')
      n_lines++;

  size_t ll = 0;
  n_lines = (n_lines / SF_PROGDATA_LC_CAP + 1) * SF_PROGDATA_LC_CAP;
  char **lines = SFMALLOC (SF_PROGDATA_LC_CAP * sizeof (*lines));
  size_t lo = 0; // last offset

  int i;
  for (i = 0; i < pos; i++)
    {
      if (buf[i] == '\n')
        {
          buf[i] = '\0';
          lines[ll++] = buf + lo;
          lo = i + 1;
        }
    }

  if (lo != pos)
    {
      buf[i] = '\0';
      lines[ll++] = buf + lo;
    }

  *vm.pg = sf_progdata_new_withLines (lines, ll);

  sf_natives_add_tovm (&vm);

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

  fclose (f);

  /* native mods have negative keys */
  {
    obj_t *o_ljson = sf_objstore_req ();
    o_ljson->type = OBJ_MOD;
    o_ljson->v.o_mod.v = sf_lib_json_makemod ();

    IR (o_ljson);
    sf_modstore_add (vm.mod_store, -1, o_ljson);

    vm.nativelib_s[0].code = -1;
    vm.nativelib_s[0].o = o_ljson;
    vm.nativelib_s[0].name = "json";
  }

  do
    {
      sf_vm_exec_frame_top (&vm);
      // sf_vm_exec_frame (&vm);
      // sf_vm_exec_frame (&vm);

      // printf ("%lu\n", c++);
      c++;
    }
  while (0);

  // sf_vm_print_b (&vm);

  // while (now_sec () < end);
  // printf ("%lu\n", c);

  // obj_t **os = sf_get_objstore ();

  // D (sf_obj_print (*os[5]));
}

int
main (int argc, char const *argv[])
{
  _argc = argc;
  _argv = (char **)argv;

  // D (printf ("%s\n", argv[0]));

  sf_objstore_init ();
  test3 ();
  return 0;
}