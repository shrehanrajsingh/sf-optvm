#if !defined(BYTECODE_H)
#define BYTECODE_H

#include "arith.h"
#include "const.h"
#include "expr.h"
#include "header.h"
#include "ht.h"
#include "object.h"
#include "stmt.h"

typedef enum OpcodeType
{
  OP_LOAD_CONST,
  OP_LOAD_FAST, /* local var */
  OP_LOAD,
  OP_STORE,
  OP_CALL,
  OP_ADD_1,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  //   OP_STACK_POP,
  //   OP_STACK_PUSH,
  OP_RETURN

} opcode_t;

typedef struct _inst_s
{
  opcode_t op;
  int a;
  int b;

} instr_t;

typedef struct _frame_s
{
  size_t return_ip;

  obj_t **locals;
  size_t locals_cap;
  size_t locals_count;

  size_t stack_base;

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

  hashtable_t *ht;

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
  } meta;

} vm_t;

#define SF_VM_GLOBALS_CAP (512)
#define SF_VM_STACK_CAP (128)
#define SF_VM_FRAME_CAP (500)

#define SF_VM_SLOT_GLOBAL (0)
#define SF_VM_SLOT_LOCAL (1)

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

  SF_API vm_t sf_vm_new ();
  SF_API void sf_vm_print_inst (instr_t);
  SF_API void sf_vm_print_b (vm_t *);

  SF_API void sf_vm_exec_frame_top (vm_t *);
  SF_API frame_t sf_frame_new ();
  SF_API void sf_vm_addframe (vm_t *, frame_t);
  SF_API void sf_vm_framefree (frame_t *);
  SF_API void sf_vm_popframe (vm_t *);

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // BYTECODE_H
