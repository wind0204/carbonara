#if !defined UTIL_C_LANG_SS_H_
#define UTIL_C_LANG_SS_H_

/*
 * Syntactic Sugar
 */


//-------------------------------------------------------------------------------------------------
/*
 * \brief Usage: SET_IDX(idx_of_character); ACC_OBJ(arr_gold_deposit) += 99999;
 */
#define SET_IDX( idx ) typeof((idx)) THE_INDEX_OF_THE_OBJECT = (idx);

//-------------------------------------------------------------------------------------------------
/*
 * \brief Usage: SET_IDX(idx_of_character); ACC_OBJ(arr_gold_deposit) += 99999;
 */
#define ACC_OBJ( ptr_to_array ) (ptr_to_array)[THE_INDEX_OF_THE_OBJECT]


#endif  // UTIL_C_LANG_SS_H_
