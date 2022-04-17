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

EXECUTE_PROCESS(COMMAND ${DISK2ZIP} -i 0 empty.d64 empty RESULT_VARIABLE res)
IF (NOT res EQUAL 1)
  MESSAGE(FATAL_ERROR "incorrect disk2zip invocation failed: " ${res})
ENDIF()
EXECUTE_PROCESS(COMMAND ${DISK2ZIP} -i 00000 empty.d64 empty RESULT_VARIABLE
  res)
IF (NOT res EQUAL 1)
  MESSAGE(FATAL_ERROR "incorrect disk2zip invocation failed: " ${res})
ENDIF()
EXECUTE_PROCESS(COMMAND ${DISK2ZIP} -i0000 empty.d64 empty RESULT_VARIABLE res)
IF (NOT res EQUAL 1)
  MESSAGE(FATAL_ERROR "incorrect disk2zip invocation failed: " ${res})
ENDIF()
EXECUTE_PROGRAM(${DISK2ZIP} -i 0000 -- empty.d64 empty)

IF (CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
  FILE(CHMOD 4!empty PERMISSIONS OWNER_READ)
  EXECUTE_PROCESS(COMMAND ${DISK2ZIP} empty.d64 empty RESULT_VARIABLE res)
  IF (NOT res EQUAL 3)
    MESSAGE(FATAL_ERROR "incorrect disk2zip invocation failed: " ${res})
  ENDIF()
  FILE(CHMOD 3!empty PERMISSIONS OWNER_READ)
  EXECUTE_PROCESS(COMMAND ${DISK2ZIP} empty.d64 empty RESULT_VARIABLE res)
  IF (NOT res EQUAL 3)
    MESSAGE(FATAL_ERROR "incorrect disk2zip invocation failed: " ${res})
  ENDIF()
  FILE(CHMOD 2!empty PERMISSIONS OWNER_READ)
  EXECUTE_PROCESS(COMMAND ${DISK2ZIP} empty.d64 empty RESULT_VARIABLE res)
  IF (NOT res EQUAL 3)
    MESSAGE(FATAL_ERROR "incorrect disk2zip invocation failed: " ${res})
  ENDIF()
  FILE(CHMOD 1!empty PERMISSIONS OWNER_READ)
  EXECUTE_PROCESS(COMMAND ${DISK2ZIP} empty.d64 empty RESULT_VARIABLE res)
  IF (NOT res EQUAL 3)
    MESSAGE(FATAL_ERROR "incorrect disk2zip invocation failed: " ${res})
  ENDIF()
  FILE(CHMOD empty.d64 PERMISSIONS OWNER_READ)
  EXECUTE_PROCESS(COMMAND ${ZIP2DISK} empty RESULT_VARIABLE res)
  IF (NOT res EQUAL 3)
    MESSAGE(FATAL_ERROR "incorrect zip2disk invocation failed: " ${res})
  ENDIF()
  FILE(REMOVE 1!empty 2!empty 3!empty 4!empty)
  FILE(CHMOD empty.d64 PERMISSIONS OWNER_WRITE OWNER_READ)
ENDIF()

EXECUTE_PROCESS(COMMAND ${ZIP2DISK} empty e RESULT_VARIABLE res)
IF (NOT res EQUAL 3)
  MESSAGE(FATAL_ERROR "incorrect disk2zip invocation failed: " ${res})
ENDIF()

EXECUTE_PROCESS(COMMAND ${DISK2ZIP} -- - empty RESULT_VARIABLE res)
IF (NOT res EQUAL 3)
  MESSAGE(FATAL_ERROR "incorrect disk2zip invocation failed: " ${res})
ENDIF()

EXECUTE_PROCESS(COMMAND ${DISK2ZIP} - empty INPUT_FILE empty.prg
  RESULT_VARIABLE res)
IF (NOT res EQUAL 4)
  MESSAGE(FATAL_ERROR "incorrect disk2zip invocation failed: " ${res})
ENDIF()
EXECUTE_PROCESS(COMMAND ${DISK2ZIP} empty.prg empty RESULT_VARIABLE res)
IF (NOT res EQUAL 4)
  MESSAGE(FATAL_ERROR "incorrect disk2zip invocation failed: " ${res})
ENDIF()

IF (CMAKE_VERSION VERSION_GREATER_EQUAL 3.1)
  LIST(APPEND b 1)
  FOREACH(i RANGE 1 255)
    LIST(APPEND b ${i})
  ENDFOREACH()
  STRING(ASCII ${b} b)
  FOREACH(i RANGE 1 174)
    LIST(APPEND b174 ${i})
  ENDFOREACH()
  STRING(ASCII ${b174} b174)
  STRING(CONCAT b5 ${b} ${b} ${b} ${b} ${b})
  STRING(CONCAT bytes
    ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5}
    ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5}
    ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5}
    ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5}
    ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5}
    ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5}
    ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5}
    ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5}
    ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5}
    ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5}
    ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5} ${b5}
    ${b5} ${b5} ${b5} ${b5} ${b5} ${b174})
  FILE(WRITE e.d64 ${bytes})
  MD5SUM(6954800d2fe55203dd8efca3353df824 e.d64)
  EXECUTE_PROGRAM(${DISK2ZIP} -i 0000 e.d64 empty)
  MD5SUM(ff6cd56b508886a396e8b660d2e63c89 1!empty)
  MD5SUM(ef08ea0d8632ffefee04d04d9c2c4d79 2!empty)
  MD5SUM(cdb2e153c10c81161f8b446dfa492fa6 3!empty)
  MD5SUM(ffc9bd41da8bbd029a9748d605c5cece 4!empty)
  EXECUTE_PROGRAM(${ZIP2DISK} empty e.d64)
  MD5SUM(6954800d2fe55203dd8efca3353df824 e.d64)
ENDIF()

EXECUTE_PROGRAM(${DISK2ZIP} empty.d64 ././././././empty)
MD5SUM(0c66b6561fb968faeb751eb94a68098b 1!empty)
MD5SUM(ef4bae7508940492d5452b4f13965cab 2!empty)
MD5SUM(026a1e36d2b20820920922d907394cf5 3!empty)
MD5SUM(0cae9f355cf09bc153272c4b8d2b97cd 4!empty)

EXECUTE_PROGRAM(${ZIP2DISK} ././empty e.d64)
EXECUTE_PROGRAM(${CMAKE_COMMAND} -E compare_files empty.d64 e.d64)
EXECUTE_PROGRAM(${ZIP2DISK} ./empty)
EXECUTE_PROGRAM(${CMAKE_COMMAND} -E compare_files empty.d64 e.d64)
FILE(RENAME 4!empty 4!e)
EXECUTE_PROCESS(COMMAND ${ZIP2DISK} empty fail.d64 RESULT_VARIABLE res)
IF (NOT res EQUAL 3)
  MESSAGE(FATAL_ERROR "incorrect zip2disk invocation failed: " ${res})
ENDIF()
FILE(WRITE 4!empty "")
EXECUTE_PROCESS(COMMAND ${ZIP2DISK} empty fail.d64 RESULT_VARIABLE res)
IF (NOT res EQUAL 4)
  MESSAGE(FATAL_ERROR "incorrect zip2disk invocation failed: " ${res})
ENDIF()
FILE(RENAME 3!empty 3!e)
EXECUTE_PROCESS(COMMAND ${ZIP2DISK} empty fail.d64 RESULT_VARIABLE res)
IF (NOT res EQUAL 3)
  MESSAGE(FATAL_ERROR "incorrect zip2disk invocation failed: " ${res})
ENDIF()
FILE(WRITE 3!empty "")
EXECUTE_PROCESS(COMMAND ${ZIP2DISK} empty fail.d64 RESULT_VARIABLE res)
IF (NOT res EQUAL 4)
  MESSAGE(FATAL_ERROR "incorrect zip2disk invocation failed: " ${res})
ENDIF()
FILE(RENAME 2!empty 2!e)
EXECUTE_PROCESS(COMMAND ${ZIP2DISK} empty fail.d64 RESULT_VARIABLE res)
IF (NOT res EQUAL 3)
  MESSAGE(FATAL_ERROR "incorrect zip2disk invocation failed: " ${res})
ENDIF()
FILE(WRITE 2!empty "")
EXECUTE_PROCESS(COMMAND ${ZIP2DISK} empty fail.d64 RESULT_VARIABLE res)
IF (NOT res EQUAL 4)
  MESSAGE(FATAL_ERROR "incorrect zip2disk invocation failed: " ${res})
ENDIF()
FILE(RENAME 1!empty 1!e)
EXECUTE_PROCESS(COMMAND ${ZIP2DISK} empty fail.d64 RESULT_VARIABLE res)
IF (NOT res EQUAL 3)
  MESSAGE(FATAL_ERROR "incorrect zip2disk invocation failed: " ${res})
ENDIF()
EXECUTE_PROGRAM(${CMAKE_COMMAND} -E compare_files fail.d64 empty.prg)
FILE(WRITE 1!empty "")
EXECUTE_PROCESS(COMMAND ${ZIP2DISK} empty fail.d64 RESULT_VARIABLE res)
IF (NOT res EQUAL 4)
  MESSAGE(FATAL_ERROR "incorrect zip2disk invocation failed: " ${res})
ENDIF()
EXECUTE_PROGRAM(${CMAKE_COMMAND} -E compare_files fail.d64 empty.prg)
EXECUTE_PROGRAM(${ZIP2DISK} e empty)
EXECUTE_PROGRAM(${CMAKE_COMMAND} -E compare_files empty.d64 e.d64)

EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E remove empty.prg empty.d64
  empty.p00 empty.lnx e.d64 fail.d64
  1!empty 2!empty 3!empty 4!empty 1!e 2!e 3!e 4!e)
