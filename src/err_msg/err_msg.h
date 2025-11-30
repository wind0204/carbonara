#if !defined ERR_MSG_H_
#define ERR_MSG_H_


//-------------------------------------------------------------------------------------------------
enum [[nodiscard("You should check the err_msg")]] err_msg : unsigned int {
  err_msg_no_error = 0,
  err_msg_deadlock_detected,
  err_msg_system_not_supported,
  err_msg_corrupted_data_detected,
  err_msg_permission_denied,
  err_msg_get_rid_of_the_old_data_first,
  err_msg_invalid_request,
  err_msg_wrong_name_given,

  err_msg_unknown = 8,
  err_msg_out_of_memory,
  err_msg_out_of_index,
  err_msg_out_of_resource,
  err_msg_parse_error,
  err_msg_too_long,
  err_msg_ext_not_supported,
  err_msg_conn_refused,

  err_msg_failed_to_load_library = 16,
  err_msg_conn_aborted,
  err_msg_conn_reset,
  err_msg_is_conn,
  err_msg_not_conn,
  err_msg_not_found,
  err_msg_no_such_entry,
  err_msg_already_done,
};

extern const char * const err_msg_text[];


#endif // ERR_MSG_H_
