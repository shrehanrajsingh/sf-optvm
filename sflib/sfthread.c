#include "sfthread.h"
#include "../bytecode.h"
#include "../mod.h"
#include "../object.h"

#ifndef _WIN32
typedef struct
{
  int id;
  int active;
  int detached;
  pthread_t handle;
  vm_t vm;
  std_t *std;
  obj_t *arg;
  obj_t *fname;
} sf_threadinfo_t;

typedef struct
{
  sf_threadinfo_t **items;
  size_t len;
  size_t cap;
  int next_id;
  pthread_mutex_t lock;
} sf_threadstore_t;

static sf_threadstore_t sf_thread_store = {
  .items = NULL,
  .len = 0,
  .cap = 0,
  .next_id = 1,
  .lock = PTHREAD_MUTEX_INITIALIZER,
};

static void
sf_thread_store_grow (void)
{
  size_t nc = sf_thread_store.cap ? sf_thread_store.cap * 2 : 8;
  sf_thread_store.items = SFREALLOC (sf_thread_store.items,
                                     nc * sizeof (*sf_thread_store.items));
  sf_thread_store.cap = nc;
}

static void
sf_thread_store_add (sf_threadinfo_t *info)
{
  pthread_mutex_lock (&sf_thread_store.lock);

  if (sf_thread_store.len >= sf_thread_store.cap)
    sf_thread_store_grow ();

  info->id = sf_thread_store.next_id++;
  sf_thread_store.items[sf_thread_store.len++] = info;

  pthread_mutex_unlock (&sf_thread_store.lock);
}

static void
sf_thread_store_remove (sf_threadinfo_t *info)
{
  pthread_mutex_lock (&sf_thread_store.lock);

  for (size_t i = 0; i < sf_thread_store.len; i++)
    {
      if (sf_thread_store.items[i] == info)
        {
          sf_thread_store.items[i]
              = sf_thread_store.items[sf_thread_store.len - 1];
          sf_thread_store.len--;
          break;
        }
    }

  pthread_mutex_unlock (&sf_thread_store.lock);
}

static sf_threadinfo_t *
sf_thread_store_find_locked (int id)
{
  for (size_t i = 0; i < sf_thread_store.len; i++)
    {
      if (sf_thread_store.items[i]->id == id)
        return sf_thread_store.items[i];
    }

  return NULL;
}

static obj_t *
sf_thread_make_id (int id)
{
  const_t c = (const_t){ .type = CONST_INT, .v.c_int.v = id };
  obj_t *o = sf_objstore_req_forconst (&c);

  if (o == NULL)
    {
      o = sf_objstore_req ();
      o->type = OBJ_CONST;
      o->v.o_const.v = c;
    }

  return o;
}

static void *
sf_thread_entry (void *data)
{
  sf_threadinfo_t *info = (sf_threadinfo_t *)data;
  obj_t *call_args[1] = { info->arg };

  if (info->vm.rt != NULL && info->vm.rt->inst_len)
    info->vm.ip = info->vm.rt->inst_len - 1;

  _sf_call_fun (&info->vm, info->fname, call_args, 1);

  DR (info->arg, &info->vm);
  info->arg = NULL;
  info->fname = NULL;

  while (info->vm.sp)
    {
      obj_t *tmp = info->vm.stack[--info->vm.sp];
      if (tmp != NULL)
        DR (tmp, &info->vm);
    }

  SFFREE (info->vm.stack);
  SFFREE (info->vm.frames);
  if (info->std != NULL)
    {
      sf_std_free (info->std);
      SFFREE (info->std);
      info->std = NULL;
    }

  pthread_mutex_lock (&sf_thread_store.lock);
  info->active = 0;
  int should_free = info->detached;
  pthread_mutex_unlock (&sf_thread_store.lock);

  if (should_free)
    {
      sf_thread_store_remove (info);
      SFFREE (info);
    }

  return NULL;
}
#endif

#ifndef _WIN32
static obj_t *
_sfthread_join (obj_t *tid)
{
  assert (tid && OBJ_IS_INT (tid) && "thread id must be an integer");
  int id = tid->v.o_const.v.v.c_int.v;

  pthread_mutex_lock (&sf_thread_store.lock);
  sf_threadinfo_t *info = sf_thread_store_find_locked (id);
  if (info == NULL || info->detached)
    {
      pthread_mutex_unlock (&sf_thread_store.lock);
      return NULL;
    }
  pthread_mutex_unlock (&sf_thread_store.lock);

  if (pthread_join (info->handle, NULL) != 0)
    return NULL;

  sf_thread_store_remove (info);
  SFFREE (info);
  return NULL;
}

static obj_t *
_sfthread_detach (obj_t *tid)
{
  assert (tid && OBJ_IS_INT (tid) && "thread id must be an integer");
  int id = tid->v.o_const.v.v.c_int.v;
  int should_free = 0;

  pthread_mutex_lock (&sf_thread_store.lock);
  sf_threadinfo_t *info = sf_thread_store_find_locked (id);
  if (info == NULL || info->detached)
    {
      pthread_mutex_unlock (&sf_thread_store.lock);
      return NULL;
    }

  info->detached = 1;
  should_free = (info->active == 0);
  pthread_mutex_unlock (&sf_thread_store.lock);

  pthread_detach (info->handle);

  if (should_free)
    {
      sf_thread_store_remove (info);
      SFFREE (info);
    }

  return NULL;
}
#endif

obj_t *
_sfthread_thread (obj_t *args, obj_t *fname)
{
  vm_t *gvm = _sf_get_globalvm ();

#ifdef _WIN32
  if (gvm != NULL)
    _sf_call_fun (gvm, fname, &args, 1);
#else
  int wants_ret = 0;

  if (gvm == NULL || gvm->rt == NULL)
    return NULL;

  if (gvm->ip < gvm->rt->inst_len && gvm->rt->insts[gvm->ip].b == 1)
    wants_ret = 1;

  sf_threadinfo_t *info = SFMALLOC (sizeof (*info));
  info->active = 1;
  info->detached = 0;
  info->arg = args;
  info->fname = fname;
  info->vm = sf_vm_new_nort ();
  info->vm.rt = gvm->rt;
  info->vm.meta = gvm->meta;
  info->std = SFMALLOC (sizeof (*info->std));
  *info->std = sf_std_new ();
  info->vm.std = info->std;

  IR (args);
  IR (fname);

  sf_thread_store_add (info);

  if (pthread_create (&info->handle, NULL, sf_thread_entry, info) != 0)
    {
      sf_thread_store_remove (info);
      DR (args, gvm);
      DR (fname, gvm);
      SFFREE (info->vm.stack);
      SFFREE (info->vm.frames);
      if (info->std != NULL)
        {
          sf_std_free (info->std);
          SFFREE (info->std);
        }
      SFFREE (info);
      return NULL;
    }

  if (wants_ret)
    return sf_thread_make_id (info->id);
#endif

  return NULL;
}

SF_API struct __mod_s *
sf_lib_thread_makemod ()
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
    sf_fun_addarg (f, "b");

    f->v.native.nf_type = NF_ARG_2;
    f->v.native.v.f_twoarg = _sfthread_thread;

    obj_t *thread_o = sf_objstore_req ();
    thread_o->type = OBJ_FUNC;
    thread_o->v.o_fun.v = f;

    IR (thread_o);

    m->slots[m->svl] = SFSTRDUP ("thread");
    m->vals[m->svl] = thread_o;
    m->svl++;
  }

#ifndef _WIN32
  {
    fun_t *f = sf_fun_new (FUN_NATIVE);
    sf_fun_addarg (f, "id");

    f->v.native.nf_type = NF_ARG_1;
    f->v.native.v.f_onearg = _sfthread_join;

    obj_t *join_o = sf_objstore_req ();
    join_o->type = OBJ_FUNC;
    join_o->v.o_fun.v = f;

    IR (join_o);

    m->slots[m->svl] = SFSTRDUP ("join");
    m->vals[m->svl] = join_o;
    m->svl++;
  }

  {
    fun_t *f = sf_fun_new (FUN_NATIVE);
    sf_fun_addarg (f, "id");

    f->v.native.nf_type = NF_ARG_1;
    f->v.native.v.f_onearg = _sfthread_detach;

    obj_t *detach_o = sf_objstore_req ();
    detach_o->type = OBJ_FUNC;
    detach_o->v.o_fun.v = f;

    IR (detach_o);

    m->slots[m->svl] = SFSTRDUP ("detach");
    m->vals[m->svl] = detach_o;
    m->svl++;
  }
#endif

  return m;
}