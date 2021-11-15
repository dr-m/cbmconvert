EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E remove empty.prg empty.d64
  empty.p00 empty.lnx emptz.d64 1!empty 2!empty 3!empty 4!empty)
EXECUTE_PROCESS(COMMAND ${CBMCONVERT} -D4 empty.d64 empty.prg
  RESULT_VARIABLE res)
IF (NOT res EQUAL 2)
  MESSAGE(FATAL_ERROR "creating empty.d64 failed: " ${res})
ENDIF()
EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E md5sum empty.d64
  OUTPUT_VARIABLE md5)
IF (NOT md5 MATCHES "274f94a63ada0913cf717677e536cdf9")
  MESSAGE(FATAL_ERROR "unexpected empty image contents: " ${md5})
ENDIF()
EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E touch empty.prg)
EXECUTE_PROCESS(COMMAND ${CBMCONVERT} -D4 empty.d64 empty.prg
  RESULT_VARIABLE res)
IF (res)
  MESSAGE(FATAL_ERROR "copying empty.prg to empty.d64 failed: " ${res})
ENDIF()
EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E md5sum empty.d64
  OUTPUT_VARIABLE md5)
IF (NOT md5 MATCHES "b92a237dc6356940542593037d2184cf")
  MESSAGE(FATAL_ERROR "unexpected empty.prg image contents: " ${md5})
ENDIF()
EXECUTE_PROCESS(COMMAND ${CBMCONVERT} -L empty.lnx -d empty.d64
  RESULT_VARIABLE res)
IF (res)
  MESSAGE(FATAL_ERROR "copying empty.d64 to empty.lnx failed: " ${res})
ENDIF()
EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E md5sum empty.lnx
  OUTPUT_VARIABLE md5)
IF (NOT md5 MATCHES "67715c3da9c4c73168d2e7177ea25545")
  MESSAGE(FATAL_ERROR "unexpected empty.lnx contents: " ${md5})
ENDIF()
EXECUTE_PROCESS(COMMAND ${CBMCONVERT} -P -l empty.lnx
  RESULT_VARIABLE res)
EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E md5sum empty.p00
  OUTPUT_VARIABLE md5)
IF (NOT md5 MATCHES "34dca27bbc851ac44ca0817dabeb9593")
  MESSAGE(FATAL_ERROR "unexpected empty.p00 contents: " ${md5})
ENDIF()
EXECUTE_PROCESS(COMMAND ${DISK2ZIP} empty.d64 empty
  RESULT_VARIABLE res)
IF (res)
  MESSAGE(FATAL_ERROR "disk2zip failed: " ${res})
ENDIF()
EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E md5sum 1!empty
  OUTPUT_VARIABLE md5)
IF (NOT md5 MATCHES "0c66b6561fb968faeb751eb94a68098b")
  MESSAGE(FATAL_ERROR "unexpected 1!empty contents: " ${md5})
ENDIF()
EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E md5sum 2!empty
  OUTPUT_VARIABLE md5)
IF (NOT md5 MATCHES "ef4bae7508940492d5452b4f13965cab")
  MESSAGE(FATAL_ERROR "unexpected 2!empty contents: " ${md5})
ENDIF()
EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E md5sum 3!empty
  OUTPUT_VARIABLE md5)
IF (NOT md5 MATCHES "026a1e36d2b20820920922d907394cf5")
  MESSAGE(FATAL_ERROR "unexpected 3!empty contents: " ${md5})
ENDIF()
EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E md5sum 4!empty
  OUTPUT_VARIABLE md5)
IF (NOT md5 MATCHES "0cae9f355cf09bc153272c4b8d2b97cd")
  MESSAGE(FATAL_ERROR "unexpected 4!empty contents: " ${md5})
ENDIF()
EXECUTE_PROCESS(COMMAND ${ZIP2DISK} empty emptz.d64
  RESULT_VARIABLE res)
IF (res)
  MESSAGE(FATAL_ERROR "zip2disk failed: " ${res})
ENDIF()
EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E compare_files empty.d64 emptz.d64
  OUTPUT_VARIABLE res)
IF (res)
  MESSAGE(FATAL_ERROR "empty.d64 and emptz.d64 differ: " ${res})
ENDIF()
EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E remove empty.prg empty.d64
  empty.p00 empty.lnx emptz.d64 1!empty 2!empty 3!empty 4!empty)
