#if !defined CARBONARA_ASSET_FUNC_H
#define CARBONARA_ASSET_FUNC_H

#include <SDL3/SDL_asyncio.h>

#include "carbonara-job.h"
#include "err_msg.h"


//-------------------------------------------------------------------------------------------------
enum err_msg cara_deal_with_asyncio(
    SDL_AsyncIOQueue * restrict p_aioq,
    struct cara_worker_state * restrict p_ws
    );


#endif  // FUNC_HARBONARA_ASSET_H
