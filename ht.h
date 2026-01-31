#ifndef HT_H
#define HT_H

#include "header.h"
#include "malloc.h"

typedef enum
{
  HT_EMPTY = 0,
  HT_ACTIVE,
  HT_TOMBSTONE
} ht_state_t;

typedef struct
{
  uint64_t hash;
  char *name;
  void *val;
  ht_state_t state;
} htval_t;

#define SF_FASTCACHE_SIZE (8)

typedef struct
{
  char *keys[SF_FASTCACHE_SIZE];
  void *vals[SF_FASTCACHE_SIZE];
  size_t c;

} fastcache_t;

typedef struct
{
  htval_t *vals;
  size_t cap;
  size_t entries;
  size_t tombstones;

  fastcache_t fast;

} hashtable_t;

#define SF_HT_LINEAR_CUTOFF 12

#if defined(__cplusplus)
extern "C"
{
#endif

  SF_API hashtable_t *sf_ht_new (void);
  SF_API void sf_ht_free (hashtable_t *);

  SF_API void sf_ht_insert (hashtable_t *, const char *, void *);
  SF_API void *sf_ht_get (hashtable_t *, const char *, int *);
  SF_API void sf_ht_delete (hashtable_t *, const char *);

#if defined(__cplusplus)
}
#endif

#endif /* HT_H */