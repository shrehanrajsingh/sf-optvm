#include "object.h"

static obj_t **objstore = NULL;

#define OBJSTORE_CAP (512)
static size_t osc = OBJSTORE_CAP;
static size_t osl = 0;

static int64_t *os_freeidxs = NULL;
size_t osfic = OBJSTORE_CAP;
size_t osfil = 0;

static sfmutex_t m1;

static void
objstore_resize ()
{
  if (osl >= osc)
    {
      osc += OBJSTORE_CAP;
      objstore = SFREALLOC (objstore, osc * sizeof (*objstore));

      for (int i = osl; i < osc; i++)
        objstore[i] = NULL;
    }
}

SF_API obj_t **
sf_get_objstore ()
{
  return objstore;
}

SF_API void
sf_objstore_init ()
{
  objstore = SFMALLOC (osc * sizeof (*objstore));
  osc = OBJSTORE_CAP;
  osl = 0;

  os_freeidxs = SFMALLOC (osfic * sizeof (*os_freeidxs));

  for (int i = osl; i < osc; i++)
    objstore[i] = NULL;

  /* store constants from -5 to 255 */
  for (int i = -5; i <= 255; i++)
    {
      objstore[osl] = SFMALLOC (sizeof (**objstore));

      obj_t o = sf_objnew (OBJ_CONST);
      o.v.o_const.v.type = CONST_INT;
      o.v.o_const.v.v.c_int.v = i;
      o.meta.ref_count = 1;
      o.meta.active = 1;
      o.meta.index = osl;

      objstore[osl++][0] = o;
    }

  /* store empty string */
  objstore[osl] = SFMALLOC (sizeof (**objstore));

  obj_t o = sf_objnew (OBJ_CONST);
  o.v.o_const.v.type = CONST_STRING;
  o.v.o_const.v.v.c_str.v = SFSTRDUP ("");
  o.meta.ref_count = 1;

  objstore[osl++][0] = o;

  /* store none object */
  objstore[osl] = SFMALLOC (sizeof (**objstore));
  obj_t nobj = sf_objnew (OBJ_CONST);
  nobj.v.o_const.v.type = CONST_NONE;
  nobj.meta.ref_count = 1;

  objstore[osl++][0] = nobj;
}

SF_API obj_t *
sf_objstore_req ()
{
  sf_mutex_lock (&m1);

  if (osl >= osc)
    {
      objstore_resize ();
    }

  obj_t *r = NULL;

  if (!osfil)
    {
      obj_t **rr = &objstore[osl++];
      if (*rr == NULL)
        *rr = SFMALLOC (sizeof (**rr));

      r = *rr;
      r->meta.active = 1;
      r->meta.index = osl - 1;
    }
  else
    {
      int64_t i = os_freeidxs[--osfil];
      r = objstore[i];
      r->meta.active = 1;
      r->meta.index = i;
    }

  sf_mutex_unlock (&m1);

  return r;
}

SF_API obj_t
sf_objnew (int type)
{
  obj_t o;
  o.type = type;
  o.meta.ref_count = 0;
  o.meta.active = 0;
  o.meta.index = -1;

  return o;
}

SF_API void
sf_obj_rc_inc (obj_t *o)
{
  atomic_fetch_add_explicit (&o->meta.ref_count, 1, memory_order_relaxed);
}

SF_API void
sf_obj_rc_dec (obj_t *o)
{
  int old = atomic_fetch_sub_explicit (&o->meta.ref_count, 1,
                                       memory_order_acq_rel);

  if (old <= 1)
    {
      atomic_thread_fence (memory_order_acquire);
      /* free object */
      sf_obj_free (o);
    }
}

SF_API void
sf_obj_free (obj_t *o)
{
  o->meta.active = 0;

  if (o->type == OBJ_FUNC && o->v.o_fun.v->type == FUN_CODED)
    {
      SFFREE (o->v.o_fun.v);
    }

  if (osfil >= osfic)
    {
      osfic += OBJSTORE_CAP;
      os_freeidxs = SFREALLOC (os_freeidxs, osfic * sizeof (*os_freeidxs));
    }

  os_freeidxs[osfil++] = o->meta.index;
}

SF_API obj_t *
sf_objstore_req_forconst (const_t *c)
{
  switch (c->type)
    {
    case CONST_INT:
      {
        int i = c->v.c_int.v;
        // D (printf ("%d\n", i));

        // D (sf_obj_print (*objstore[5]));
        if (i >= -5 && i <= 255)
          return objstore[i + 5];
      }
      break;

    case CONST_STRING:
      {
        if (c->v.c_str.v[0] == '\0')
          return objstore[5 + 255 + 1] /* all int constants + 1 */;
      }
      break;
    case CONST_NONE:
      return objstore[5 + 255 + 2];
      break;

    default:
      break;
    }

  return NULL;
}

SF_API void
sf_obj_print (obj_t o)
{
  switch (o.type)
    {
    case OBJ_CONST:
      {
        const_t *c = &o.v.o_const.v;
        switch (c->type)
          {
          case CONST_INT:
            printf ("%d", c->v.c_int.v);
            break;
          case CONST_FLOAT:
            printf ("%f", c->v.c_float.v);
            break;
          case CONST_STRING:
            printf ("%s", c->v.c_str.v);
            break;
          case CONST_NONE:
            printf ("none");
            break;
          case CONST_BOOL:
            printf ("%s", c->v.c_bool.v ? "true" : "false");
            break;
          default:
            printf ("<const:unknown>");
            break;
          }
      }
      break;

    case OBJ_FUNC:
      printf ("<function:%p>", (void *)o.v.o_fun.v);
      break;

    case OBJ_CLASS:
      printf ("<class '%s'>", o.v.o_class.v->name);
      break;

    case OBJ_COBJ:
      printf ("<object '%s'>", o.v.o_cobj.v->p->name);
      break;

    case OBJ_HFF:
      D (printf ("[hff]"));
      sf_obj_print (*o.v.o_hff.f);
      break;

    default:
      printf ("<object:unknown>");
      break;
    }

  // putchar ('\n');
}

SF_API int
sf_obj_isfalse (obj_t o)
{
  int r = 1;
  switch (o.type)
    {
    case OBJ_CONST:
      {
        switch (o.v.o_const.v.type)
          {
          case CONST_INT:
            r = o.v.o_const.v.v.c_int.v == 0;
            break;

          case CONST_BOOL:
            r = !o.v.o_const.v.v.c_bool.v;
            break;

          case CONST_FLOAT:
            r = o.v.o_const.v.v.c_float.v == 0.0f;
            break;

          case CONST_NONE:
            r = 1;
            break;

          case CONST_STRING:
            r = o.v.o_const.v.v.c_str.v[0] == '\0';
            break;

          default:
            break;
          }
      }
      break;

    case OBJ_FUNC:
    case OBJ_CLASS:
    case OBJ_COBJ:
    case OBJ_HFF:
      r = 0;
      break;

    default:
      break;
    }

  return r;
}

SF_API int
sf_obj_eqeq (obj_t *o, obj_t *p)
{
  if (o->type != p->type)
    return 0;

  switch (o->type)
    {
    case OBJ_CONST:
      {
        switch (o->v.o_const.v.type)
          {
          case CONST_BOOL:
            return p->v.o_const.v.type == CONST_BOOL
                   && p->v.o_const.v.v.c_bool.v == o->v.o_const.v.v.c_bool.v;
            break;

          case CONST_NONE:
            return p->v.o_const.v.type == CONST_NONE;
            break;

          case CONST_STRING:
            return p->v.o_const.v.type == CONST_STRING
                   && !strcmp (p->v.o_const.v.v.c_str.v,
                               o->v.o_const.v.v.c_str.v);
            break;

          case CONST_INT:
            {
              if (p->v.o_const.v.type == CONST_INT)
                return p->v.o_const.v.v.c_int.v == o->v.o_const.v.v.c_int.v;
              else if (p->v.o_const.v.type == CONST_FLOAT)
                return p->v.o_const.v.v.c_float.v == o->v.o_const.v.v.c_int.v;
              else
                return 0;
            }
            break;

          case CONST_FLOAT:
            {
              if (p->v.o_const.v.type == CONST_INT)
                return p->v.o_const.v.v.c_int.v == o->v.o_const.v.v.c_float.v;
              else if (p->v.o_const.v.type == CONST_FLOAT)
                return p->v.o_const.v.v.c_float.v
                       == o->v.o_const.v.v.c_float.v;
              else
                return 0;
            }
            break;

          default:
            break;
          }
      }
      break;

    case OBJ_FUNC:
      return p->v.o_fun.v == o->v.o_fun.v;
      break;

    default:
      break;
    }

  return 0;
}

SF_API int
sf_obj_neq (obj_t *o, obj_t *p)
{
  return !sf_obj_eqeq (o, p);
}

SF_API int
sf_obj_le (obj_t *o, obj_t *p)
{
  if (o->type == OBJ_CONST && p->type == OBJ_CONST
      && (o->v.o_const.v.type == CONST_INT
          || o->v.o_const.v.type == CONST_FLOAT)
      && (p->v.o_const.v.type == CONST_INT
          || p->v.o_const.v.type == CONST_FLOAT))
    {
      float v1 = 0.0f;
      float v2 = 0.0f;

      if (o->v.o_const.v.type == CONST_INT)
        v1 = (float)o->v.o_const.v.v.c_int.v;
      else
        v1 = o->v.o_const.v.v.c_float.v;

      if (p->v.o_const.v.type == CONST_INT)
        v2 = (float)p->v.o_const.v.v.c_int.v;
      else
        v2 = p->v.o_const.v.v.c_float.v;

      return v1 < v2;
    }

  return 0;
}

SF_API int
sf_obj_ge (obj_t *o, obj_t *p)
{
  if (o->type == OBJ_CONST && p->type == OBJ_CONST
      && (o->v.o_const.v.type == CONST_INT
          || o->v.o_const.v.type == CONST_FLOAT)
      && (p->v.o_const.v.type == CONST_INT
          || p->v.o_const.v.type == CONST_FLOAT))
    {
      float v1 = 0.0f;
      float v2 = 0.0f;

      if (o->v.o_const.v.type == CONST_INT)
        v1 = (float)o->v.o_const.v.v.c_int.v;
      else
        v1 = o->v.o_const.v.v.c_float.v;

      if (p->v.o_const.v.type == CONST_INT)
        v2 = (float)p->v.o_const.v.v.c_int.v;
      else
        v2 = p->v.o_const.v.v.c_float.v;

      return v1 > v2;
    }

  return 0;
}

SF_API int
sf_obj_leq (obj_t *o, obj_t *p)
{
  if (o->type == OBJ_CONST && p->type == OBJ_CONST
      && (o->v.o_const.v.type == CONST_INT
          || o->v.o_const.v.type == CONST_FLOAT)
      && (p->v.o_const.v.type == CONST_INT
          || p->v.o_const.v.type == CONST_FLOAT))
    {
      float v1 = 0.0f;
      float v2 = 0.0f;

      if (o->v.o_const.v.type == CONST_INT)
        v1 = (float)o->v.o_const.v.v.c_int.v;
      else
        v1 = o->v.o_const.v.v.c_float.v;

      if (p->v.o_const.v.type == CONST_INT)
        v2 = (float)p->v.o_const.v.v.c_int.v;
      else
        v2 = p->v.o_const.v.v.c_float.v;

      return v1 <= v2;
    }

  return 0;
}

SF_API int
sf_obj_geq (obj_t *o, obj_t *p)
{
  if (o->type == OBJ_CONST && p->type == OBJ_CONST
      && (o->v.o_const.v.type == CONST_INT
          || o->v.o_const.v.type == CONST_FLOAT)
      && (p->v.o_const.v.type == CONST_INT
          || p->v.o_const.v.type == CONST_FLOAT))
    {
      float v1 = 0.0f;
      float v2 = 0.0f;

      if (o->v.o_const.v.type == CONST_INT)
        v1 = (float)o->v.o_const.v.v.c_int.v;
      else
        v1 = o->v.o_const.v.v.c_float.v;

      if (p->v.o_const.v.type == CONST_INT)
        v2 = (float)p->v.o_const.v.v.c_int.v;
      else
        v2 = p->v.o_const.v.v.c_float.v;

      return v1 >= v2;
    }

  return 0;
}
