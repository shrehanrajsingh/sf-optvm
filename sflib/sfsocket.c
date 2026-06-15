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

  {
    /* Socket class */
    class_t *cl_n = sf_class_new ();
    cl_n->name = SFSTRDUP ("Socket");

    cl_n->svc = 8;
    cl_n->slots = SFMALLOC (cl_n->svc * sizeof (*cl_n->slots));
    cl_n->vals = SFMALLOC (cl_n->svc * sizeof (*cl_n->vals));

    {
      /* port variable */
      cl_n->slots[cl_n->svl] = SFSTRDUP ("port");

      const_t i0 = (const_t){
        .type = CONST_INT,
        .v.c_int.v = 0,
      };

      cl_n->vals[cl_n->svl] = sf_objstore_req_forconst (&i0);

      cl_n->svl++;
    }

    obj_t *cl_o = sf_objstore_req ();
    cl_o->type = OBJ_CLASS;
    cl_o->v.o_class.v = cl_n;

    IR (cl_o);
    m->fr->n.names[m->fr->n.nvl] = SFSTRDUP ("Socket");
    m->fr->n.vals[m->fr->n.nvl++] = cl_o;

    IR (cl_o);
    m->slots[m->svl] = SFSTRDUP ("Socket");
    m->vals[m->svl++] = cl_o;
  }

  return m;
}