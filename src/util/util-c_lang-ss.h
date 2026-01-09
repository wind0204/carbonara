#if !defined UTIL_C_LANG_SS_H_
#define UTIL_C_LANG_SS_H_

/*
 * Syntactic Sugar
 */


//-------------------------------------------------------------------------------------------------
/*
 * \brief Usage: _set_idx(creature_idx); _get_obj(arr_gold_deposit) += 99999;
 */
#define _set_idx( idx ) typeof((idx)) THE_INDEX_OF_THE_OBJECT = (idx);

//-------------------------------------------------------------------------------------------------
/*
 * \brief Usage: _set_idx(creature_idx); _get_obj(arr_gold_deposit) += 99999;
 */
#define _get_obj( ptr_to_array ) (ptr_to_array)[THE_INDEX_OF_THE_OBJECT]


#endif  // UTIL_C_LANG_SS_H_
