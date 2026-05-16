#include "json.h"
#include "../bytecode.h"
#include "../mod.h"
#include "../object.h"

static obj_t *
_sfjson_from (obj_t *s)
{
  assert (OBJ_IS_STRING (s) && "from () expects a string argument");

  printf ("Inside json.from ()\n");
  return NULL;
}

SF_API mod_t *
sf_lib_json_makemod ()
{
  mod_t *m = sf_mod_new ();
  m->is_native = 1;
  m->fr = SFMALLOC (sizeof (*m->fr));
  *m->fr = sf_frame_new_name ();

  m->svc = 8;
  m->slots = SFMALLOC (m->svc * sizeof (*m->slots));
  m->vals = SFMALLOC (m->svc * sizeof (*m->vals));

  {
    fun_t *f = sf_fun_new (FUN_NATIVE);
    sf_fun_addarg (f, "a");
    f->v.native.nf_type = NF_ARG_1;
    f->v.native.v.f_onearg = _sfjson_from;

    obj_t *from_o = sf_objstore_req ();
    from_o->type = OBJ_FUNC;
    from_o->v.o_fun.v = f;

    IR (from_o);

    m->slots[m->svl] = SFSTRDUP ("from");
    m->vals[m->svl] = from_o;
    m->svl++;
  }

  return m;
}