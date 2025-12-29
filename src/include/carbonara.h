#if !defined CARBONARA_H_
#define CARBONARA_H_


#include <stdint.h>

//#include "appdev_support-processor_map.h"
#include "err_msg.h"




//-------------------------------------------------------------------------------------------------
struct carbonara_scene {
  uint16_t idx_first_job;
  uint8_t flags;
};



//-------------------------------------------------------------------------------------------------
/**
 * \brief Initialize the engine of carbonara.
 *
 * \return 0 if it was successful, an enum err_msg otherwise.
 */
enum err_msg init_carbonara(uint32_t len_each_arena, uint64_t proc_info);
//uint8_t init_carbonara(struct carbonara_engine * carbonara);



#endif  // CARBONARA_H_
