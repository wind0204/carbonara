#if !defined FUNC_OVERLOADING_AND_DEF_ARGS_AND_NAMED_ARGS_H_
#define FUNC_OVERLOADING_AND_DEF_ARGS_AND_NAMED_ARGS_H_


//-------------------------------------------------------------------------------------------------
/**
 * \brief Verbatim! You can use it to pass more than one argument packed as
 * one single argument into a macro function.
 */
#define V(...) __VA_ARGS__

#define EXPAND(x) x




#define PICK_64TH( \
  _1, _2, _3, _4, _5, _6, _7, _8, _9, _10,  \
  _11,_12,_13,_14,_15,_16,_17,_18,_19,_20,  \
  _21,_22,_23,_24,_25,_26,_27,_28,_29,_30,  \
  _31,_32,_33,_34,_35,_36,_37,_38,_39,_40,  \
  _41,_42,_43,_44,_45,_46,_47,_48,_49,_50,  \
  _51,_52,_53,_54,_55,_56,_57,_58,_59,_60,  \
  _61,_62,_63, N, ...) N

#define SEQ_FROM_63_TO_0() \
  63, 62, 61, 60, \
  59, 58, 57, 56, 55, 54, 53, 52, 51, 50, \
  49, 48, 47, 46, 45, 44, 43, 42, 41, 40, \
  39, 38, 37, 36, 35, 34, 33, 32, 31, 30, \
  29, 28, 27, 26, 25, 24, 23, 22, 21, 20, \
  19, 18, 17, 16, 15, 14, 13, 12, 11, 10, \
   9,  8,  7,  6,  5,  4,  3,  2,  1,  0

#define ARG_CNT_IMPL(...) PICK_64TH(__VA_ARGS__)
//#define ARG_CNT(...) ARG_CNT_IMPL(__VA_ARGS__, SEQ_FROM_63_TO_0())

#define SEQ_FOR_COMMA_DETECTION() \
  1,  1,  1,  1,                          \
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  \
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  \
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  \
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  \
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  \
  1,  1,  1,  1,  1,  1,  1,  1,  0,  0

#define HASCOMMA(...) \
  ARG_CNT_IMPL(__VA_ARGS__, SEQ_FOR_COMMA_DETECTION())

#define COMMA(...)  ,

#define NARG_HELPER1(a, b, N)  NARG_HELPER2(a, b, N)
#define NARG_HELPER2(a, b, N)  NARG_HELPER3_ ## a ## b(N)
#define NARG_HELPER3_01(N)     1
#define NARG_HELPER3_00(N)     0
#define NARG_HELPER3_11(N)     N

#define NARG_IMPL(...) \
  NARG_HELPER1( \
      HASCOMMA(__VA_ARGS__), \
      HASCOMMA(COMMA __VA_ARGS__), \
      ARG_CNT_IMPL(__VA_ARGS__, SEQ_FROM_63_TO_0()))

#define START_GLUEING(a, b) GLUE_IMPL(a, b)
#define GLUE_IMPL(a, b) a ## b

#define VA_ARGCNT_WITH_UNDERSCORE(...) \
  START_GLUEING(_, NARG_IMPL(__VA_ARGS__))

#define VAFUNC_IMPL(func_name, ...) \
  START_GLUEING(func_name, VA_ARGCNT_WITH_UNDERSCORE(__VA_ARGS__))

//-------------------------------------------------------------------------------------------------
/**
 * \details Usage Example:
 *   #define foo(...) VAFUNC(foo, __VA_ARGS__)
 *   foo(0, 0)  // gets converted to foo2(0, 0)
 */
#define VAFUNC(func_name, ...) \
  VAFUNC_IMPL(func_name, __VA_ARGS__) (__VA_ARGS__)


//-------------------------------------------------------------------------------------------------
/**
 * \brief Call with the parameter struct of the function
 * \details Give up utilizing rdi,rsi,rdx,rcx as function arguments to get fancy named arguments.
 *   Usage Example:
 *   struct func_name_params { int foobar; int x; int y; int z; };
 *   void func_name_(int handle, struct func_name_params params[static 1]);
 *   #define func_name(handle, ...) CWPS(func_name, handle, __VA_ARGS__)
 *   func_name(h, .x=1, .y=2);  // CWPS and CWPS_WO_HEADER are enough if every default value is 0.
 */
#define CWPS(func_name, header, ...) \
  func_name ## _((header), &(struct func_name ## _params){__VA_ARGS__})

//-------------------------------------------------------------------------------------------------
/**
 * \brief Call with the parameter struct of the function, with default values
 * \details Give up utilizing rdi,rsi,rdx,rcx as function arguments to get fancy named arguments.
 *   Usage Example:
 *   struct infos_gmm_rent_aligned_params { uint32_t length; uint32_t growth; uint8_t alignment; };
 *   void * infos_gmm_rent_aligned_(struct infos_growing_mem_manager * manager,
 *       struct infos_gmm_rent_aligned_params params[static 1]);
 *   #define infos_gmm_rent_aligned(manager, ...) CWPS_WITH_DEF_VAL(infos_gmm_rent_aligned, manager, \
 *       V(.length=4096, .growth=65536, .alignment=sizeof(void*)), __VA_ARGS__)
 *   void * ptr = infos_gmm_rent_aligned(gmm, .growth = 4096 * 32);
 */
#define CWPS_WITH_DEF_VAL(func_name, header, defvals, ...) \
  _Pragma ( "GCC diagnostic push" ) \
  _Pragma ( "GCC diagnostic ignored \"-Winitializer-overrides\"" ) \
  func_name ## _((header), &(struct func_name ## _params){defvals, __VA_ARGS__}) \
  _Pragma ( "GCC diagnostic pop" )

//-------------------------------------------------------------------------------------------------
/**
 * \brief Call with the parameter struct of the function, without header
 * \details Give up utilizing rdi,rsi,rdx,rcx as function arguments to get fancy named arguments.
 *   Usage Example:
 *   struct infos_gmm_init_params {
 *       void * (*custom_malloc)(size_t size, void * userdata);
 *       void (*custom_free)(void * ptr, void * userdata);
 *       void * userdata;
 *       uint32_t initial_length;
 *   };
 *   struct infos_growing_mem_manager * infos_gmm_init_(struct infos_gmm_init_params params[static 1]);
 *   #define infos_gmm_init(...) CWPS_WO_HEADER(infos_gmm_init, __VA_ARGS__)
 *   // CWPS and CWPS_WO_HEADER are enough if every default value is 0.
 *   struct infos_growing_mem_manager * gmm = infos_gmm_init(.initial_length = 4096 * 32);
 */
#define CWPS_WO_HEADER(func_name, ...) \
  func_name ## _(&(struct func_name ## _params){__VA_ARGS__})

//-------------------------------------------------------------------------------------------------
/**
 * \brief Call with the parameter struct of the function, with default values, without header
 * \details Give up utilizing rdi,rsi,rdx,rcx as function arguments to get fancy named arguments.
 *   Usage Example:
 *   struct infos_al_init_params {
 *     void * (*custom_malloc)(size_t size, void * userdata);
 *     void (*custom_free)(void * ptr, void * userdata);
 *     void * userdata;
 *     uint32_t node_size; uint16_t elem_size;
 *   };
 *   struct infos_arr_list * infos_al_init_(struct infos_al_init_params params[static 1]);
 *   #define infos_al_init(...) CWPS_WITH_DEF_VAL_WO_HEADER(infos_al_init, \
 *       V(.elem_size=sizeof(int), .node_size=4096), __VA_ARGS__)
 *   struct infos_arr_list * al = infos_al_init(.elemsize=sizeof(long), node_size=8192, userdata=the_data);
 */
#define CWPS_WITH_DEF_VAL_WO_HEADER(func_name, defvals, ...) \
  _Pragma ( "GCC diagnostic push" ) \
  _Pragma ( "GCC diagnostic ignored \"-Winitializer-overrides\"" ) \
  func_name ## _(&(struct func_name ## _params){defvals, __VA_ARGS__}) \
  _Pragma ( "GCC diagnostic pop" )

#define REPLACE_0_WITH_DEF_VAL_1(def1, arg1) \
      (arg1) ? (arg1) : (def1)
#define REPLACE_0_WITH_DEF_VAL_2(def1, def2, arg1, arg2) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2)
#define REPLACE_0_WITH_DEF_VAL_3(def1, def2, def3, arg1, arg2, arg3) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3)
#define REPLACE_0_WITH_DEF_VAL_4(def1, def2, def3, def4, arg1, arg2, arg3, arg4) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4)
#define REPLACE_0_WITH_DEF_VAL_5(def1, def2, def3, def4, def5, arg1, arg2, arg3, arg4, arg5) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4), \
      (arg5) ? (arg5) : (def5)
#define REPLACE_0_WITH_DEF_VAL_6(def1, def2, def3, def4, def5, def6, arg1, arg2, arg3, arg4, arg5, arg6) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4), \
      (arg5) ? (arg5) : (def5), \
      (arg6) ? (arg6) : (def6)
#define REPLACE_0_WITH_DEF_VAL_7(def1, def2, def3, def4, def5, def6, def7, arg1, arg2, arg3, arg4, arg5, arg6, arg7) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4), \
      (arg5) ? (arg5) : (def5), \
      (arg6) ? (arg6) : (def6), \
      (arg7) ? (arg7) : (def7)
#define REPLACE_0_WITH_DEF_VAL_8(def1, def2, def3, def4, def5, def6, def7, def8, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4), \
      (arg5) ? (arg5) : (def5), \
      (arg6) ? (arg6) : (def6), \
      (arg7) ? (arg7) : (def7), \
      (arg8) ? (arg8) : (def8)
#define REPLACE_0_WITH_DEF_VAL_9(def1, def2, def3, def4, def5, def6, def7, def8, def9, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4), \
      (arg5) ? (arg5) : (def5), \
      (arg6) ? (arg6) : (def6), \
      (arg7) ? (arg7) : (def7), \
      (arg8) ? (arg8) : (def8), \
      (arg9) ? (arg9) : (def9)
#define REPLACE_0_WITH_DEF_VAL_10(def1, def2, def3, def4, def5, def6, def7, def8, def9, def10, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4), \
      (arg5) ? (arg5) : (def5), \
      (arg6) ? (arg6) : (def6), \
      (arg7) ? (arg7) : (def7), \
      (arg8) ? (arg8) : (def8), \
      (arg9) ? (arg9) : (def9), \
      (arg10) ? (arg10) : (def10)
#define REPLACE_0_WITH_DEF_VAL_11(def1, def2, def3, def4, def5, def6, def7, def8, def9, def10, def11, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4), \
      (arg5) ? (arg5) : (def5), \
      (arg6) ? (arg6) : (def6), \
      (arg7) ? (arg7) : (def7), \
      (arg8) ? (arg8) : (def8), \
      (arg9) ? (arg9) : (def9), \
      (arg10) ? (arg10) : (def10), \
      (arg11) ? (arg11) : (def11)
#define REPLACE_0_WITH_DEF_VAL_12(def1, def2, def3, def4, def5, def6, def7, def8, def9, def10, def11, def12, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4), \
      (arg5) ? (arg5) : (def5), \
      (arg6) ? (arg6) : (def6), \
      (arg7) ? (arg7) : (def7), \
      (arg8) ? (arg8) : (def8), \
      (arg9) ? (arg9) : (def9), \
      (arg10) ? (arg10) : (def10), \
      (arg11) ? (arg11) : (def11), \
      (arg12) ? (arg12) : (def12)
#define REPLACE_0_WITH_DEF_VAL_13(def1, def2, def3, def4, def5, def6, def7, def8, def9, def10, def11, def12, def13, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4), \
      (arg5) ? (arg5) : (def5), \
      (arg6) ? (arg6) : (def6), \
      (arg7) ? (arg7) : (def7), \
      (arg8) ? (arg8) : (def8), \
      (arg9) ? (arg9) : (def9), \
      (arg10) ? (arg10) : (def10), \
      (arg11) ? (arg11) : (def11), \
      (arg12) ? (arg12) : (def12), \
      (arg13) ? (arg13) : (def13)
#define REPLACE_0_WITH_DEF_VAL_14(def1, def2, def3, def4, def5, def6, def7, def8, def9, def10, def11, def12, def13, def14, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4), \
      (arg5) ? (arg5) : (def5), \
      (arg6) ? (arg6) : (def6), \
      (arg7) ? (arg7) : (def7), \
      (arg8) ? (arg8) : (def8), \
      (arg9) ? (arg9) : (def9), \
      (arg10) ? (arg10) : (def10), \
      (arg11) ? (arg11) : (def11), \
      (arg12) ? (arg12) : (def12), \
      (arg13) ? (arg13) : (def13), \
      (arg14) ? (arg14) : (def14)
#define REPLACE_0_WITH_DEF_VAL_15(def1, def2, def3, def4, def5, def6, def7, def8, def9, def10, def11, def12, def13, def14, def15, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4), \
      (arg5) ? (arg5) : (def5), \
      (arg6) ? (arg6) : (def6), \
      (arg7) ? (arg7) : (def7), \
      (arg8) ? (arg8) : (def8), \
      (arg9) ? (arg9) : (def9), \
      (arg10) ? (arg10) : (def10), \
      (arg11) ? (arg11) : (def11), \
      (arg12) ? (arg12) : (def12), \
      (arg13) ? (arg13) : (def13), \
      (arg14) ? (arg14) : (def14), \
      (arg15) ? (arg15) : (def15)
#define REPLACE_0_WITH_DEF_VAL_16(def1, def2, def3, def4, def5, def6, def7, def8, def9, def10, def11, def12, def13, def14, def15, def16, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15, arg16) \
      (arg1) ? (arg1) : (def1), \
      (arg2) ? (arg2) : (def2), \
      (arg3) ? (arg3) : (def3), \
      (arg4) ? (arg4) : (def4), \
      (arg5) ? (arg5) : (def5), \
      (arg6) ? (arg6) : (def6), \
      (arg7) ? (arg7) : (def7), \
      (arg8) ? (arg8) : (def8), \
      (arg9) ? (arg9) : (def9), \
      (arg10) ? (arg10) : (def10), \
      (arg11) ? (arg11) : (def11), \
      (arg12) ? (arg12) : (def12), \
      (arg13) ? (arg13) : (def13), \
      (arg14) ? (arg14) : (def14), \
      (arg15) ? (arg15) : (def15), \
      (arg16) ? (arg16) : (def16)

#define ARGS_FOR_CALL_FUNC_WITH_DEF_VAL_IMPL(...) \
  START_GLUEING( REPLACE_0_WITH_DEF_VAL_, NARG_IMPL(__VA_ARGS__) )

#define ARGS_FOR_CALL_FUNC_WITH_DEF_VAL(defvals, ...) \
  ARGS_FOR_CALL_FUNC_WITH_DEF_VAL_IMPL(V(defvals)) (defvals, __VA_ARGS__)

//-------------------------------------------------------------------------------------------------
/**
 * \brief Call a function with default values, a default value replaces a 0.
 * \details
 *   Usage Example:
 *   int add(int a, int b) {
 *     return a + b;
 *   }
 *   #define add_(...) CWDV(add, V(0, 100), __VA_ARGS__)
 *   fprintf(stdout, "add_(8,0) = %d\n", add_(8,0));  // 108
 */
#define CWDV(func_name, defvals, ...) \
  func_name( \
      ARGS_FOR_CALL_FUNC_WITH_DEF_VAL(V(defvals), __VA_ARGS__) \
      )



#endif  // FUNC_OVERLOADING_AND_DEF_ARGS_AND_NAMED_ARGS_H_
