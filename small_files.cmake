MACRO(MD5SUM expected filename)
  EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E md5sum ${filename}
    OUTPUT_VARIABLE md5)
  IF (NOT md5 MATCHES ${expected})
    MESSAGE(FATAL_ERROR "unexpected md5sum: " ${md5})
  ENDIF()
ENDMACRO()
MACRO(CBMCONVERT)
  EXECUTE_PROCESS(COMMAND ${CBMCONVERT} ${ARGV} RESULT_VARIABLE res)
  IF (res)
    MESSAGE(FATAL_ERROR "cbmconvert ${ARGV} failed: " ${res})
  ENDIF()
ENDMACRO()

EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E remove 1.prg 2.prg 3.prg
  1.p00 2.p00 3.p00 123.lnx 123.d64)
FILE(WRITE 1.prg "1")
FILE(WRITE 2.prg "12")
FILE(WRITE 3.prg "123")
CBMCONVERT(-D4 123.d64 1.prg 2.prg 3.prg)
MD5SUM(75816bb6b5f90c12db2979a42fbdf987 123.d64)
CBMCONVERT(-L 123.lnx -d 123.d64)
MD5SUM(fc71016e5bcd397d0b88e0d498f2a133 123.lnx)
EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E remove 123.lnx 123.d64)
CBMCONVERT(-L 123.lnx 1.prg 2.prg 3.prg)
MD5SUM(fc71016e5bcd397d0b88e0d498f2a133 123.lnx)
CBMCONVERT(-D4 123.d64 -l 123.lnx)
MD5SUM(75816bb6b5f90c12db2979a42fbdf987 123.d64)

CBMCONVERT(-P -l 123.lnx)
MD5SUM(ca80b5d5492283789cb53c1868783b83 1.p00)
MD5SUM(2abfea98086627c448530dae6f19970f 2.p00)
MD5SUM(ac1246d517a9d6db969185fe60746900 3.p00)

EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E remove 1.prg 2.prg 3.prg
  1.p00 2.p00 3.p00 123.lnx 123.d64)
