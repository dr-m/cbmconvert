MACRO(MD5SUM expected filename)
  EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E md5sum ${filename}
    OUTPUT_VARIABLE md5)
  IF (NOT md5 MATCHES ${expected})
    MESSAGE(FATAL_ERROR "unexpected md5sum: " ${md5})
  ENDIF()
ENDMACRO()

MACRO(EXECUTE_PROGRAM)
  EXECUTE_PROCESS(COMMAND ${ARGV} RESULT_VARIABLE res)
  IF (res)
    MESSAGE(FATAL_ERROR "${ARGV} failed: " ${res})
  ENDIF()
ENDMACRO()
MACRO(CBMCONVERT)
  EXECUTE_PROGRAM(${CBMCONVERT} ${ARGV})
ENDMACRO()

EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E remove empty.prg empty.d64
  empty.p00 empty.lnx emptz.d64 1!empty 2!empty 3!empty 4!empty)

EXECUTE_PROCESS(COMMAND ${CBMCONVERT} -D4 empty.d64 empty.prg
  RESULT_VARIABLE res)
IF (NOT res EQUAL 2)
  MESSAGE(FATAL_ERROR "creating empty.d64 failed: " ${res})
ENDIF()
MD5SUM(274f94a63ada0913cf717677e536cdf9 empty.d64)
FILE(WRITE empty.prg "")
CBMCONVERT(-D4 empty.d64 empty.prg)
MD5SUM(b92a237dc6356940542593037d2184cf empty.d64)
CBMCONVERT(-L empty.lnx -d empty.d64)
MD5SUM(67715c3da9c4c73168d2e7177ea25545 empty.lnx)
CBMCONVERT(-P -l empty.lnx)
MD5SUM(34dca27bbc851ac44ca0817dabeb9593 empty.p00)

EXECUTE_PROGRAM(${DISK2ZIP} empty.d64 empty)
MD5SUM(0c66b6561fb968faeb751eb94a68098b 1!empty)
MD5SUM(ef4bae7508940492d5452b4f13965cab 2!empty)
MD5SUM(026a1e36d2b20820920922d907394cf5 3!empty)
MD5SUM(0cae9f355cf09bc153272c4b8d2b97cd 4!empty)

EXECUTE_PROGRAM(${ZIP2DISK} empty emptz.d64)
EXECUTE_PROGRAM(${CMAKE_COMMAND} -E compare_files empty.d64 emptz.d64)

EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E remove empty.prg empty.d64
  empty.p00 empty.lnx emptz.d64 1!empty 2!empty 3!empty 4!empty)
