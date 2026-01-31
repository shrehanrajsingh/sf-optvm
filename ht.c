#include "ht.h"

static inline uint64_t
fnv1a_hash (const char *s)
{
  uint64_t h = 14695981039346656037ULL;
  while (*s)
    {
      h ^= (unsigned char)*s++;
      h *= 1099511628211ULL;
    }
  return h;
}

static inline size_t
next_pow2 (size_t x)
{
  size_t p = 1;
  while (p < x)
    p <<= 1;
  return p;
}

static void
ht_init_table (hashtable_t *ht, size_t cap)
{
  ht->cap = next_pow2 (cap);
  ht->entries = 0;
  ht->tombstones = 0;

  ht->vals = SFMALLOC (ht->cap * sizeof (*ht->vals));
  for (size_t i = 0; i < ht->cap; i++)
    ht->vals[i].state = HT_EMPTY;

  ht->fast.c = 0;
}

static void
ht_resize (hashtable_t *ht, size_t new_cap)
{
  htval_t *old_vals = ht->vals;
  size_t old_cap = ht->cap;

  ht_init_table (ht, new_cap);

  size_t mask = ht->cap - 1;

  for (size_t i = 0; i < old_cap; i++)
    {
      if (old_vals[i].state != HT_ACTIVE)
        continue;

      uint64_t h = old_vals[i].hash;
      size_t idx = h & mask;
      size_t step = 1;

      while (ht->vals[idx].state == HT_ACTIVE)
        {
          idx = (idx + step) & mask;
          step += 2;
        }

      ht->vals[idx] = old_vals[i];
      ht->entries++;
    }

  SFFREE (old_vals);
}

SF_API hashtable_t *
sf_ht_new (void)
{
  hashtable_t *ht = SFMALLOC (sizeof (*ht));
  ht_init_table (ht, 1024);
  return ht;
}

SF_API void
sf_ht_free (hashtable_t *ht)
{
  for (size_t i = 0; i < ht->cap; i++)
    {
      if (ht->vals[i].state == HT_ACTIVE)
        SFFREE (ht->vals[i].name);
    }

  SFFREE (ht->vals);
  SFFREE (ht);
}

SF_API void
sf_ht_insert (hashtable_t *ht, const char *key, void *val)
{
  if (ht->fast.c < SF_FASTCACHE_SIZE)
    {
      ht->fast.keys[ht->fast.c] = (char *)key;
      ht->fast.vals[ht->fast.c++] = val;

      return;
    }

  if ((ht->entries + ht->tombstones) * 10 >= ht->cap * 7)
    ht_resize (ht, ht->cap * 2);

  uint64_t hash = fnv1a_hash (key);
  size_t mask = ht->cap - 1;
  size_t idx = hash & mask;
  size_t step = 1;

  ssize_t tomb = -1;

  while (1)
    {
      htval_t *e = &ht->vals[idx];

      if (step > 32)
        {
          ht_resize (ht, ht->cap);
          return sf_ht_insert (ht, key, val);
        }

      if (e->state == HT_EMPTY)
        {
          if (tomb != -1)
            {
              idx = (size_t)tomb;
              ht->tombstones--;
            }

          ht->vals[idx] = (htval_t){
            .hash = hash, .name = (char *)key, .val = val, .state = HT_ACTIVE
          };
          ht->entries++;
          return;
        }

      if (e->state == HT_TOMBSTONE && tomb == -1)
        tomb = (ssize_t)idx;

      else if (e->state == HT_ACTIVE && e->hash == hash
               && strcmp (e->name, key) == 0)
        {
          e->val = val;
          return;
        }

      idx = (idx + step) & mask;
      step += 2;
    }
}

SF_API void *
sf_ht_get (hashtable_t *ht, const char *key, int *gr)
{
  for (int i = 0; i < ht->fast.c; i++)
    {
      if (!strcmp (ht->fast.keys[i], key))
        {
          if (gr != NULL)
            *gr = 1;

          return ht->fast.vals[i];
        }
    }

  uint64_t hash = fnv1a_hash (key);
  size_t mask = ht->cap - 1;
  size_t idx = hash & mask;
  size_t step = 1;

  while (1)
    {
      htval_t *e = &ht->vals[idx];

      if (e->state == HT_EMPTY)
        break;

      if (e->state == HT_ACTIVE && e->hash == hash
          && strcmp (e->name, key) == 0)
        {
          if (gr)
            *gr = 1;
          return e->val;
        }

      idx = (idx + step) & mask;
      step += 2;
    }

  if (gr)
    *gr = 0;
  return NULL;
}

SF_API void
sf_ht_delete (hashtable_t *ht, const char *key)
{
  int found_key = 0;
  for (int i = 0; i < ht->fast.c; i++)
    {
      if (!strcmp (ht->fast.keys[i], key))
        {
          found_key = 1;
          continue;
        }

      if (found_key)
        {
          ht->fast.keys[i - 1] = ht->fast.keys[i];
          ht->fast.vals[i - 1] = ht->fast.vals[i];
        }
    }

  if (found_key)
    {
      ht->fast.c--;
      return;
    }

  uint64_t hash = fnv1a_hash (key);
  size_t mask = ht->cap - 1;
  size_t idx = hash & mask;
  size_t step = 1;

  while (1)
    {
      htval_t *e = &ht->vals[idx];

      if (e->state == HT_EMPTY)
        return;

      if (e->state == HT_ACTIVE && e->hash == hash
          && strcmp (e->name, key) == 0)
        {
          SFFREE (e->name);
          e->name = NULL;
          e->val = NULL;
          e->state = HT_TOMBSTONE;

          ht->entries--;
          ht->tombstones++;
          return;
        }

      idx = (idx + step) & mask;
      step += 2;
    }
}