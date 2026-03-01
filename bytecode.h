#if !defined(BYTECODE_H)
#define BYTECODE_H

#include "arith.h"
#include "cl.h"
#include "const.h"
#include "expr.h"
#include "header.h"
#include "ht.h"
#include "iter.h"
#include "object.h"
#include "stmt.h"

typedef enum OpcodeType
{
  OP_LOAD_CONST,
  OP_LOAD_FAST, /* local var */
  OP_LOAD,
  OP_STORE,
  OP_STORE_FAST,
  OP_STORE_NAME,
  OP_STORE_SQR,
  OP_CALL,
  OP_ADD_1,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_LOAD_FUNC_CODED,
  OP_CMP,
  OP_LOAD_BUILDCLASS,
  OP_LOAD_BUILDCLASS_END,
  OP_LOAD_NAME,
  OP_DOT_ACCESS,
  OP_LOAD_ARRAY,
  OP_SQR_ACCESS,
  OP_RANGE_FAST,
  OP_GET_ITER,
  OP_LOAD_ITER_NEXT,
  //   OP_STACK_POP,
  //   OP_STACK_PUSH,
  OP_RETURN

} opcode_t;

typedef struct _inst_s
{
  opcode_t op;
  int a;
  int b;
  char *c;

} instr_t;

enum FrameType
{
  FRAME_LOCAL,
  FRAME_GLOBAL,
  FRAME_NAME,
};

typedef struct _frame_s
{
  int type;
  size_t return_ip;

  union
  {
    struct
    {
      obj_t **locals;
      size_t locals_cap;
      size_t locals_count;

    } l;

    struct
    {
      char **names;
      obj_t **vals;
      size_t nvl;
      size_t nvc;

    } n;
  };

  size_t stack_base;
  int pop_ret_val; // 1: yes, 0: no

} frame_t;

#define SF_FRAME_LOCALS_CAP (64)

typedef struct
{
  size_t pos;
  int slot;

} vval_t;

typedef struct _vm_s
{
  size_t ip;
  instr_t *insts;
  size_t inst_len;
  size_t inst_cap;

  const_t *map_consts;
  size_t s_ml;
  size_t s_mc;

  hashtable_t **hts;
  size_t htl;
  size_t htc;

  obj_t **globals; /* var name -> slot; globals[slot] = var_value */
  size_t globals_cap;

  obj_t **stack;
  size_t stack_cap;
  size_t sp;

  frame_t *frames;
  size_t fp;
  size_t frame_cap;

  struct
  {
    int slot;      /* GLOBAL/LOCAL */
    size_t g_slot; /* global slot count */
    size_t l_slot; /* local slot count */
    size_t n_slot; /* name slot count */
  } meta;

} vm_t;

#define SF_VM_GLOBALS_CAP (512)
#define SF_VM_STACK_CAP (128)
#define SF_VM_FRAME_CAP (500)
#define SF_VM_HT_CAP (8)
#define SF_VM_NAME_CAP (8)

#define SF_VM_SLOT_GLOBAL (0)
#define SF_VM_SLOT_LOCAL (1)
#define SF_VM_SLOT_NAME (2) // class

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API vm_t sf_vm_new ();
  SF_API void sf_vm_print_inst (instr_t);
  SF_API void sf_vm_print_b (vm_t *);

  SF_API void sf_vm_exec_frame_top (vm_t *);
  SF_API void sf_vm_exec_single_frame (vm_t *);
  SF_API frame_t sf_frame_new_local ();
  SF_API frame_t sf_frame_new_name ();
  SF_API void sf_vm_addframe (vm_t *, frame_t);
  SF_API void sf_vm_framefree (frame_t *, vm_t *);
  SF_API void sf_vm_popframe (vm_t *);
  SF_API obj_t *container_access (obj_t *, char *);
  SF_API void container_set (obj_t *, char *, obj_t *, vm_t *);
  SF_API obj_t *sqr_access (obj_t *, obj_t *);
  SF_API void sqr_set (obj_t *, obj_t *, obj_t *, vm_t *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // BYTECODE_H
