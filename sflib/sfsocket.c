#include "sfsocket.h"
#include "../bytecode.h"
#include "../mod.h"
#include "../object.h"

SF_API struct __mod_s *
sf_lib_socket_makemod ()
{
  mod_t *m = sf_mod_new ();
  m->is_native = 1;
  m->fr = SFMALLOC (sizeof (*m->fr));
  *m->fr = sf_frame_new_name ();

  m->svc = 8;
  m->slots = SFMALLOC (m->svc * sizeof (*m->slots));
  m->vals = SFMALLOC (m->svc * sizeof (*m->vals));

  {
    obj_t *o_AF_INET = sf_objstore_req ();

    o_AF_INET->type = OBJ_CONST;
    o_AF_INET->v.o_const.v.type = CONST_INT;
    o_AF_INET->v.o_const.v.v.c_int.v = AF_INET;

    IR (o_AF_INET);
    m->fr->n.names[m->fr->n.nvl] = SFSTRDUP ("AF_INET");
    m->fr->n.vals[m->fr->n.nvl++] = o_AF_INET;

    IR (o_AF_INET);
    m->slots[m->svl] = SFSTRDUP ("AF_INET");
    m->vals[m->svl++] = o_AF_INET;
  }

  {
    obj_t *o_SOCK_STREAM = sf_objstore_req ();

    o_SOCK_STREAM->type = OBJ_CONST;
    o_SOCK_STREAM->v.o_const.v.type = CONST_INT;
    o_SOCK_STREAM->v.o_const.v.v.c_int.v = SOCK_STREAM;

    IR (o_SOCK_STREAM);
    m->fr->n.names[m->fr->n.nvl] = SFSTRDUP ("SOCK_STREAM");
    m->fr->n.vals[m->fr->n.nvl++] = o_SOCK_STREAM;

    IR (o_SOCK_STREAM);
    m->slots[m->svl] = SFSTRDUP ("SOCK_STREAM");
    m->vals[m->svl++] = o_SOCK_STREAM;
  }

  return m;
}