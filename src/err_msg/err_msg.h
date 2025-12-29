#if !defined ERR_MSG_H_
#define ERR_MSG_H_


//-------------------------------------------------------------------------------------------------
enum [[nodiscard("You should check the err_msg")]] err_msg : unsigned int {
  emsg_no_error = 0,
  emsg_deadlock_detected,
  emsg_system_not_supported,
  emsg_corrupted_data_detected,
  emsg_permission_denied,
  emsg_get_rid_of_the_old_data_first,
  emsg_invalid_request,
  emsg_wrong_name_given,

  emsg_unknown = 8,
  emsg_out_of_memory,
  emsg_out_of_index,
  emsg_out_of_resource,
  emsg_parse_error,
  emsg_too_long,
  emsg_ext_not_supported,
  emsg_conn_refused,

  emsg_failed_to_load_library = 16,
  emsg_conn_aborted,
  emsg_conn_reset,
  emsg_is_conn,
  emsg_not_conn,
  emsg_not_found,
  emsg_no_such_entry,
  emsg_already_done,
};

extern const char * const arr_emsg_txt[];


#endif // ERR_MSG_H_
