#include <string.h>
#include <stdint.h>

#include "info_struct.h"



//-------------------------------------------------------------------------------------------------
struct infos_stack * infos_stack_create(void * buffer, uint32_t buf_len) {
  assume( buf_len > sizeof(struct infos_stack) );

  struct infos_stack * stack = buffer;
  stack->total_len = buf_len - sizeof(struct infos_stack);
  stack->data = buffer + sizeof(struct infos_stack);
  stack->idx_last_elem = 0;

  return stack;
}


//-------------------------------------------------------------------------------------------------
uint_fast8_t infos_stack_push( struct infos_stack * restrict stack,
    uint_fast16_t nr_elems, const void * elements, uint_fast8_t log2_elem_len ) {
  typeof(stack->idx_last_elem) idx_last_slot = stack->total_len >> log2_elem_len; 
  if ( idx_last_slot - stack->idx_last_elem < nr_elems ) {
    return 0;
  }

  uint_fast16_t elem_len = 1 << log2_elem_len;
  void * ptr = stack->data + stack->idx_last_elem * elem_len;
  memcpy(ptr, elements, elem_len * nr_elems);

  stack->idx_last_elem += nr_elems;

  return UINT_FAST8_MAX;
}


//-------------------------------------------------------------------------------------------------
void * infos_stack_pop(struct infos_stack * stack, uint_fast16_t elem_len) {
  if ( stack->idx_last_elem == 0 )
    return 0;

  void * ret = stack->data + (stack->idx_last_elem - 1) * elem_len;
  stack->idx_last_elem--;
  return ret;
}
