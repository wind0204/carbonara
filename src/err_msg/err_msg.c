#include "err_msg.h"


//-------------------------------------------------------------------------------------------------
const char * const arr_emsg_txt[] = {
  "",
  "A deadlock was detected.",  // 1
  "The system is not supported by this application.",
  "Corrupted data was detected.",
  "You don't have the required permissions.",
  "Please get rid of the old data first.",
  "The request is invalid.",
  "It seems like a wrong name was given.",
  "Unknown error.",  // 8
  "It's run out of memory.",
  "The index number was out of range.",
  "Not able to allocate more resource.",
  "There was an error during parsing a string.",
  "It's too long.",
  "The extension is not supported.",
  "Connection is refused.",
  "Failed to load the shared library.",  // 16
  "Connection is aborted.",
  "The connection has been reset by the peer.",
  "Already connected.",
  "Not connected yet.",
  "Couldn't find it.",
  "No such entry.",
  "It has already been done.",
};
