#if !defined(MOD_H)
#define MOD_H

#include "expr.h"
#include "header.h"
#include "ht.h"
#include "malloc.h"
#include "object.h"
#include "stmt.h"

enum ModType
{
  MOD_FILE
};

typedef struct __module_s
{
  int type;

  StmtSM *body;
  hashtable_t *ht;

  struct __module_s *parent;

} mod_t;

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API mod_t *sf_mod_new (int, StmtSM *);
  SF_API void sf_mod_free (mod_t *);
  SF_API void sf_mod_addvar (mod_t *, const char *, obj_t *);
  SF_API obj_t *sf_mod_getvar (mod_t *, const char *, int *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // MOD_H
