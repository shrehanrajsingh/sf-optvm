#include "json.h"
#include "../bytecode.h"
#include "../mod.h"
#include "../object.h"

typedef struct
{
  obj_t **vals;
  size_t len;
  size_t cap;

} sfjson_objlist_t;

static void
_sfjson_skip_ws (const char **p)
{
  while (**p != '\0' && isspace ((unsigned char)**p))
    ++(*p);
}

static int
_sfjson_is_delim (char c)
{
  return c == '\0' || c == ',' || c == ']' || c == '}'
         || isspace ((unsigned char)c);
}

static int
_sfjson_hexval (char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F')
    return 10 + (c - 'A');
  return -1;
}

static int
_sfjson_buf_push (char **buf, size_t *len, size_t *cap, char c)
{
  if (*len + 1 >= *cap)
    {
      size_t ncap = *cap ? *cap * 2 : 16;
      char *nbuf = SFREALLOC (*buf, ncap * sizeof (*nbuf));
      if (nbuf == NULL)
        return 0;
      *buf = nbuf;
      *cap = ncap;
    }

  (*buf)[(*len)++] = c;
  return 1;
}

static int
_sfjson_buf_push_utf8 (char **buf, size_t *len, size_t *cap, uint32_t cp)
{
  if (cp <= 0x7F)
    return _sfjson_buf_push (buf, len, cap, (char)cp);
  if (cp <= 0x7FF)
    {
      return _sfjson_buf_push (buf, len, cap, (char)(0xC0 | (cp >> 6)))
             && _sfjson_buf_push (buf, len, cap, (char)(0x80 | (cp & 0x3F)));
    }
  if (cp <= 0xFFFF)
    {
      return _sfjson_buf_push (buf, len, cap, (char)(0xE0 | (cp >> 12)))
             && _sfjson_buf_push (buf, len, cap,
                                  (char)(0x80 | ((cp >> 6) & 0x3F)))
             && _sfjson_buf_push (buf, len, cap, (char)(0x80 | (cp & 0x3F)));
    }

  return _sfjson_buf_push (buf, len, cap, (char)(0xF0 | (cp >> 18)))
         && _sfjson_buf_push (buf, len, cap,
                              (char)(0x80 | ((cp >> 12) & 0x3F)))
         && _sfjson_buf_push (buf, len, cap, (char)(0x80 | ((cp >> 6) & 0x3F)))
         && _sfjson_buf_push (buf, len, cap, (char)(0x80 | (cp & 0x3F)));
}

static int
_sfjson_buf_push_str (char **buf, size_t *len, size_t *cap, const char *s)
{
  if (s == NULL)
    return 0;

  while (*s)
    {
      if (!_sfjson_buf_push (buf, len, cap, *s++))
        return 0;
    }

  return 1;
}

static int
_sfjson_buf_push_hex2 (char **buf, size_t *len, size_t *cap, unsigned char v)
{
  static const char *hex = "0123456789abcdef";
  return _sfjson_buf_push (buf, len, cap, hex[v >> 4])
         && _sfjson_buf_push (buf, len, cap, hex[v & 0x0F]);
}

static int
_sfjson_write_string (const char *s, char **buf, size_t *len, size_t *cap)
{
  if (s == NULL)
    return 0;

  if (!_sfjson_buf_push (buf, len, cap, '"'))
    return 0;

  for (const unsigned char *p = (const unsigned char *)s; *p; p++)
    {
      unsigned char c = *p;
      switch (c)
        {
        case '"':
          if (!_sfjson_buf_push_str (buf, len, cap, "\\\""))
            return 0;
          break;
        case '\\':
          if (!_sfjson_buf_push_str (buf, len, cap, "\\\\"))
            return 0;
          break;
        case '\b':
          if (!_sfjson_buf_push_str (buf, len, cap, "\\b"))
            return 0;
          break;
        case '\f':
          if (!_sfjson_buf_push_str (buf, len, cap, "\\f"))
            return 0;
          break;
        case '\n':
          if (!_sfjson_buf_push_str (buf, len, cap, "\\n"))
            return 0;
          break;
        case '\r':
          if (!_sfjson_buf_push_str (buf, len, cap, "\\r"))
            return 0;
          break;
        case '\t':
          if (!_sfjson_buf_push_str (buf, len, cap, "\\t"))
            return 0;
          break;
        default:
          if (c < 0x20)
            {
              if (!_sfjson_buf_push_str (buf, len, cap, "\\u00"))
                return 0;
              if (!_sfjson_buf_push_hex2 (buf, len, cap, c))
                return 0;
            }
          else
            {
              if (!_sfjson_buf_push (buf, len, cap, (char)c))
                return 0;
            }
          break;
        }
    }

  return _sfjson_buf_push (buf, len, cap, '"');
}

static char *
_sfjson_parse_string (const char **p)
{
  if (**p != '"')
    return NULL;

  ++(*p);

  char *buf = NULL;
  size_t len = 0;
  size_t cap = 0;

  while (**p != '\0')
    {
      char c = *(*p)++;

      if (c == '"')
        {
          if (!_sfjson_buf_push (&buf, &len, &cap, '\0'))
            {
              if (buf != NULL)
                SFFREE (buf);
              return NULL;
            }

          return buf;
        }

      if (c == '\\')
        {
          char e = *(*p)++;
          if (e == '\0')
            break;

          switch (e)
            {
            case '"':
            case '\\':
            case '/':
              if (!_sfjson_buf_push (&buf, &len, &cap, e))
                goto err;
              break;

            case 'b':
              if (!_sfjson_buf_push (&buf, &len, &cap, '\b'))
                goto err;
              break;
            case 'f':
              if (!_sfjson_buf_push (&buf, &len, &cap, '\f'))
                goto err;
              break;
            case 'n':
              if (!_sfjson_buf_push (&buf, &len, &cap, '\n'))
                goto err;
              break;
            case 'r':
              if (!_sfjson_buf_push (&buf, &len, &cap, '\r'))
                goto err;
              break;
            case 't':
              if (!_sfjson_buf_push (&buf, &len, &cap, '\t'))
                goto err;
              break;

            case 'u':
              {
                int h1 = _sfjson_hexval (*(*p)++);
                int h2 = _sfjson_hexval (*(*p)++);
                int h3 = _sfjson_hexval (*(*p)++);
                int h4 = _sfjson_hexval (*(*p)++);
                if (h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0)
                  goto err;

                uint32_t cp
                    = (uint32_t)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);

                if (cp >= 0xD800 && cp <= 0xDBFF)
                  {
                    if (**p == '\\' && *(*p + 1) == 'u')
                      {
                        (*p) += 2;
                        int l1 = _sfjson_hexval (*(*p)++);
                        int l2 = _sfjson_hexval (*(*p)++);
                        int l3 = _sfjson_hexval (*(*p)++);
                        int l4 = _sfjson_hexval (*(*p)++);
                        if (l1 < 0 || l2 < 0 || l3 < 0 || l4 < 0)
                          goto err;

                        uint32_t low = (uint32_t)((l1 << 12) | (l2 << 8)
                                                  | (l3 << 4) | l4);
                        if (low >= 0xDC00 && low <= 0xDFFF)
                          {
                            cp = 0x10000
                                 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                          }
                        else
                          {
                            goto err;
                          }
                      }
                    else
                      {
                        goto err;
                      }
                  }

                if (!_sfjson_buf_push_utf8 (&buf, &len, &cap, cp))
                  goto err;
              }
              break;

            default:
              goto err;
            }
        }
      else
        {
          if (!_sfjson_buf_push (&buf, &len, &cap, c))
            goto err;
        }
    }

err:
  if (buf != NULL)
    SFFREE (buf);
  return NULL;
}

static obj_t *
_sfjson_make_int (int v)
{
  const_t c = sf_const_int_new (v);
  obj_t *o = sf_objstore_req_forconst (&c);
  if (o == NULL)
    {
      o = sf_objstore_req ();
      o->type = OBJ_CONST;
      o->v.o_const.v = c;
    }
  return o;
}

static obj_t *
_sfjson_make_float (float v)
{
  const_t c = sf_const_float_new (v);
  obj_t *o = sf_objstore_req ();
  o->type = OBJ_CONST;
  o->v.o_const.v = c;
  return o;
}

static obj_t *
_sfjson_make_bool (int v)
{
  const_t c = sf_const_bool_new (v);
  obj_t *o = sf_objstore_req ();
  o->type = OBJ_CONST;
  o->v.o_const.v = c;
  return o;
}

static obj_t *
_sfjson_make_none (void)
{
  const_t c = (const_t){ .type = CONST_NONE };
  obj_t *o = sf_objstore_req_forconst (&c);
  if (o == NULL)
    {
      o = sf_objstore_req ();
      o->type = OBJ_CONST;
      o->v.o_const.v = c;
    }
  return o;
}

static obj_t *
_sfjson_make_string (char *s)
{
  obj_t *o = sf_objstore_req ();
  o->type = OBJ_CONST;
  o->v.o_const.v.type = CONST_STRING;
  o->v.o_const.v.v.c_str.v = s;
  return o;
}

static int _sfjson_write_value (obj_t *o, char **buf, size_t *len,
                                size_t *cap);

static int
_sfjson_write_array (array_t *a, char **buf, size_t *len, size_t *cap)
{
  if (a == NULL)
    return 0;

  if (!_sfjson_buf_push (buf, len, cap, '['))
    return 0;

  if (a->len && a->vals == NULL)
    return 0;

  for (size_t i = 0; i < a->len; i++)
    {
      if (i && !_sfjson_buf_push (buf, len, cap, ','))
        return 0;
      if (!_sfjson_write_value (a->vals[i], buf, len, cap))
        return 0;
    }

  return _sfjson_buf_push (buf, len, cap, ']');
}

static int
_sfjson_write_object (dict_t *d, char **buf, size_t *len, size_t *cap)
{
  if (d == NULL)
    return 0;

  if (!_sfjson_buf_push (buf, len, cap, '{'))
    return 0;

  if (d->len && (d->keys == NULL || d->vals == NULL))
    return 0;

  for (size_t i = 0; i < d->len; i++)
    {
      obj_t *k = d->keys[i];
      obj_t *v = d->vals[i];

      if (k == NULL || !OBJ_IS_STRING (k))
        return 0;

      if (i && !_sfjson_buf_push (buf, len, cap, ','))
        return 0;
      if (!_sfjson_write_string (k->v.o_const.v.v.c_str.v, buf, len, cap))
        return 0;
      if (!_sfjson_buf_push (buf, len, cap, ':'))
        return 0;
      if (!_sfjson_write_value (v, buf, len, cap))
        return 0;
    }

  return _sfjson_buf_push (buf, len, cap, '}');
}

static int
_sfjson_write_value (obj_t *o, char **buf, size_t *len, size_t *cap)
{
  if (o == NULL)
    return 0;

  switch (o->type)
    {
    case OBJ_CONST:
      switch (o->v.o_const.v.type)
        {
        case CONST_INT:
          {
            char tmp[32];
            int n
                = snprintf (tmp, sizeof (tmp), "%d", o->v.o_const.v.v.c_int.v);
            if (n <= 0 || (size_t)n >= sizeof (tmp))
              return 0;
            return _sfjson_buf_push_str (buf, len, cap, tmp);
          }
        case CONST_FLOAT:
          {
            double dv = (double)o->v.o_const.v.v.c_float.v;
            if (!isfinite (dv))
              return 0;
            char tmp[64];
            int n = snprintf (tmp, sizeof (tmp), "%.9g", dv);
            if (n <= 0 || (size_t)n >= sizeof (tmp))
              return 0;
            return _sfjson_buf_push_str (buf, len, cap, tmp);
          }
        case CONST_STRING:
          return _sfjson_write_string (o->v.o_const.v.v.c_str.v, buf, len,
                                       cap);
        case CONST_BOOL:
          return _sfjson_buf_push_str (
              buf, len, cap, o->v.o_const.v.v.c_bool.v ? "true" : "false");
        case CONST_NONE:
          return _sfjson_buf_push_str (buf, len, cap, "null");
        default:
          return 0;
        }
      break;

    case OBJ_ARRAY:
      return _sfjson_write_array (o->v.o_array.v, buf, len, cap);

    case OBJ_DICT:
      return _sfjson_write_object (o->v.o_dict.v, buf, len, cap);

    default:
      return 0;
    }

  return 0;
}

static int
_sfjson_list_push (sfjson_objlist_t *l, obj_t *o)
{
  if (l->len >= l->cap)
    {
      size_t ncap = l->cap ? l->cap * 2 : 8;
      obj_t **nv = SFREALLOC (l->vals, ncap * sizeof (*nv));
      if (nv == NULL)
        return 0;
      l->vals = nv;
      l->cap = ncap;
    }

  l->vals[l->len++] = o;
  return 1;
}

static obj_t *_sfjson_parse_value (const char **p);

static obj_t *
_sfjson_parse_array (const char **p)
{
  if (**p != '[')
    return NULL;
  ++(*p);

  _sfjson_skip_ws (p);

  sfjson_objlist_t items = { 0 };

  if (**p == ']')
    {
      ++(*p);
      array_t *a = sf_array_new ();
      obj_t *o = sf_objstore_req ();
      o->type = OBJ_ARRAY;
      o->v.o_array.v = a;
      return o;
    }

  while (**p != '\0')
    {
      obj_t *v = _sfjson_parse_value (p);
      if (v == NULL)
        goto err;

      IR (v);
      if (!_sfjson_list_push (&items, v))
        goto err;

      _sfjson_skip_ws (p);
      if (**p == ',')
        {
          ++(*p);
          _sfjson_skip_ws (p);
          continue;
        }
      if (**p == ']')
        {
          ++(*p);
          break;
        }

      goto err;
    }

  {
    array_t *a = sf_array_new ();
    if (items.len)
      {
        a->vals = SFMALLOC (items.len * sizeof (*a->vals));
        for (size_t i = 0; i < items.len; i++)
          a->vals[i] = items.vals[i];
        a->len = items.len;
      }

    obj_t *o = sf_objstore_req ();
    o->type = OBJ_ARRAY;
    o->v.o_array.v = a;
    if (items.vals != NULL)
      SFFREE (items.vals);
    return o;
  }

err:
  if (items.vals != NULL)
    SFFREE (items.vals);
  return NULL;
}

static obj_t *
_sfjson_parse_object (const char **p)
{
  if (**p != '{')
    return NULL;
  ++(*p);

  _sfjson_skip_ws (p);

  sfjson_objlist_t keys = { 0 };
  sfjson_objlist_t vals = { 0 };

  if (**p == '}')
    {
      ++(*p);
      dict_t *d = sf_dict_new ();
      obj_t *o = sf_objstore_req ();
      o->type = OBJ_DICT;
      o->v.o_dict.v = d;
      return o;
    }

  while (**p != '\0')
    {
      _sfjson_skip_ws (p);
      if (**p != '"')
        goto err;

      char *kstr = _sfjson_parse_string (p);
      if (kstr == NULL)
        goto err;

      obj_t *k = _sfjson_make_string (kstr);
      IR (k);
      if (!_sfjson_list_push (&keys, k))
        goto err;

      _sfjson_skip_ws (p);
      if (**p != ':')
        goto err;
      ++(*p);
      _sfjson_skip_ws (p);

      obj_t *v = _sfjson_parse_value (p);
      if (v == NULL)
        goto err;

      IR (v);
      if (!_sfjson_list_push (&vals, v))
        goto err;

      _sfjson_skip_ws (p);
      if (**p == ',')
        {
          ++(*p);
          continue;
        }
      if (**p == '}')
        {
          ++(*p);
          break;
        }

      goto err;
    }

  {
    dict_t *d = sf_dict_new ();
    d->len = keys.len;
    if (d->len)
      {
        d->keys = SFMALLOC (d->len * sizeof (*d->keys));
        d->vals = SFMALLOC (d->len * sizeof (*d->vals));
        for (size_t i = 0; i < d->len; i++)
          {
            d->keys[i] = keys.vals[i];
            d->vals[i] = vals.vals[i];
          }
      }

    obj_t *o = sf_objstore_req ();
    o->type = OBJ_DICT;
    o->v.o_dict.v = d;

    if (keys.vals != NULL)
      SFFREE (keys.vals);
    if (vals.vals != NULL)
      SFFREE (vals.vals);
    return o;
  }

err:
  if (keys.vals != NULL)
    SFFREE (keys.vals);
  if (vals.vals != NULL)
    SFFREE (vals.vals);
  return NULL;
}

static obj_t *
_sfjson_parse_number (const char **p)
{
  const char *start = *p;
  char *end = NULL;
  errno = 0;

  double d = strtod (start, &end);
  if (end == start)
    return NULL;

  if (!_sfjson_is_delim (*end))
    return NULL;

  int is_float = 0;
  for (const char *c = start; c < end; c++)
    {
      if (*c == '.' || *c == 'e' || *c == 'E')
        {
          is_float = 1;
          break;
        }
    }

  obj_t *o = NULL;
  if (!is_float)
    {
      long long ll = strtoll (start, NULL, 10);
      if (ll >= INT_MIN && ll <= INT_MAX)
        {
          o = _sfjson_make_int ((int)ll);
        }
      else
        {
          is_float = 1;
        }
    }

  if (is_float)
    {
      o = _sfjson_make_float ((float)d);
    }

  *p = end;
  return o;
}

static obj_t *
_sfjson_parse_value (const char **p)
{
  _sfjson_skip_ws (p);

  char c = **p;
  if (c == '\0')
    return NULL;

  if (c == '"')
    {
      char *s = _sfjson_parse_string (p);
      if (s == NULL)
        return NULL;
      return _sfjson_make_string (s);
    }

  if (c == '{')
    return _sfjson_parse_object (p);

  if (c == '[')
    return _sfjson_parse_array (p);

  if (c == '-' || (c >= '0' && c <= '9'))
    return _sfjson_parse_number (p);

  if (!strncmp (*p, "true", 4) && _sfjson_is_delim ((*p)[4]))
    {
      *p += 4;
      return _sfjson_make_bool (1);
    }

  if (!strncmp (*p, "false", 5) && _sfjson_is_delim ((*p)[5]))
    {
      *p += 5;
      return _sfjson_make_bool (0);
    }

  if (!strncmp (*p, "null", 4) && _sfjson_is_delim ((*p)[4]))
    {
      *p += 4;
      return _sfjson_make_none ();
    }

  return NULL;
}

static obj_t *
_sfjson_from (obj_t *ss)
{
  assert (OBJ_IS_STRING (ss) && "from () expects a string argument");

  const char *p = ss->v.o_const.v.v.c_str.v;
  obj_t *out = _sfjson_parse_value (&p);
  if (out == NULL)
    return NULL;

  _sfjson_skip_ws (&p);
  if (*p != '\0')
    {
      return NULL;
    }

  return out;
}

static obj_t *
_sfjson_to (obj_t *dd)
{
  assert (OBJ_IS_DICT (dd) && "to () expects a dict argument");

  char *buf = NULL;
  size_t len = 0;
  size_t cap = 0;

  if (!_sfjson_write_value (dd, &buf, &len, &cap))
    goto err;

  if (!_sfjson_buf_push (&buf, &len, &cap, '\0'))
    goto err;

  return _sfjson_make_string (buf);

err:
  if (buf != NULL)
    SFFREE (buf);
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

  {
    fun_t *f = sf_fun_new (FUN_NATIVE);
    sf_fun_addarg (f, "a");
    f->v.native.nf_type = NF_ARG_1;
    f->v.native.v.f_onearg = _sfjson_to;

    obj_t *to_o = sf_objstore_req ();
    to_o->type = OBJ_FUNC;
    to_o->v.o_fun.v = f;

    IR (to_o);

    m->slots[m->svl] = SFSTRDUP ("to_str");
    m->vals[m->svl] = to_o;
    m->svl++;
  }

  return m;
}