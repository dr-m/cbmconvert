MACRO(MD5SUM expected filename)
  EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E md5sum ${filename}
    OUTPUT_VARIABLE md5)
  IF (NOT md5 MATCHES ${expected})
    MESSAGE(FATAL_ERROR "unexpected md5sum: " ${expected} " != " ${md5})
  ENDIF()
ENDMACRO()
MACRO(EXECUTE_PROGRAM)
  EXECUTE_PROCESS(COMMAND ${ARGV} RESULT_VARIABLE res)
  IF (res)
    MESSAGE(FATAL_ERROR "${ARGV} failed: " ${res})
  ENDIF()
ENDMACRO()
MACRO(EXECUTE_PROGRAM_EXPECT expect_res)
  EXECUTE_PROCESS(COMMAND ${ARGN} RESULT_VARIABLE res)
  IF (NOT res EQUAL ${expect_res})
    MESSAGE(FATAL_ERROR "${ARGN} failed: " ${res})
  ENDIF()
ENDMACRO()
MACRO(CBMCONVERT)
  EXECUTE_PROGRAM(${CBMCONVERT} ${ARGV})
ENDMACRO()

FILE(REMOVE 123.d64 123.d71 123.d81 124.d64)
FILE(REMOVE 123.lnx 123.c2n 5.d64 5.lnx 5.l7f 5.rel)
FILE(WRITE 1,S "1")
FILE(WRITE 2,U "12")
FILE(WRITE 3,D "123")
FOREACH(i RANGE 1 254)
  LIST(APPEND b ${i})
ENDFOREACH()
LIST(REMOVE_ITEM b 10 13 59)
STRING(ASCII ${b} b)
FILE(WRITE 4,P "${b};: ")
FILE(WRITE 5.l7F "${b}: ;")

CBMCONVERT(-D4 123.d64 1,S 2,U 3,D 4,P 5.l7F)
MD5SUM(5d7682a959ce78c07e7a6ac24bfd4799 123.d64)

EXECUTE_PROGRAM_EXPECT(1 ${CBMCONVERT} -D 123.d64 5.l7F)
CBMCONVERT(-D4o 123.d64 5.l7F)
MD5SUM(5d7682a959ce78c07e7a6ac24bfd4799 123.d64)
CBMCONVERT(-L 123.lnx -d 123.d64)
MD5SUM(99c30961746ece8de28cd524511162bf 123.lnx)
CBMCONVERT(-vv -C 123.c2n -d 123.d64)
MD5SUM(cc439f7db11441055aac1439494883fd 123.c2n)

EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D4 123.d64 -c 123.c2n)
MD5SUM(5d7682a959ce78c07e7a6ac24bfd4799 123.d64)
FILE(REMOVE 123.lnx 123.d64)
CBMCONVERT(-L 123.lnx -c 123.c2n)
MD5SUM(9da8cd65bf210daa4b86eda9461dc7ef 123.lnx)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -C 123.c2n -p 123.lnx)
FILE(REMOVE 123.c2n 123.lnx)

FILE(RENAME 1,S 1,s)
FILE(RENAME 2,U 2,u)
FILE(RENAME 3,D 3,d)
FILE(RENAME 4,P 4,p)
FILE(RENAME 5.l7F 5.l7f)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -L 123.lnx 4,p 4,p)
CBMCONVERT(-L 123.lnx -o1 -n 4,p 4,p)
MD5SUM(31036a537e19832da30b21e630867606 123.lnx)
CBMCONVERT(-L 123.lnx -n 1,s 2,u 3,d 4,p 5.l7f)
MD5SUM(99c30961746ece8de28cd524511162bf 123.lnx)
CBMCONVERT(-D4o 123.d64 -l 123.lnx)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D4 123.d64 -l 123.lnx)
MD5SUM(5d7682a959ce78c07e7a6ac24bfd4799 123.d64)
CBMCONVERT(-D4o 123.d64 5.l7f)
MD5SUM(5d7682a959ce78c07e7a6ac24bfd4799 123.d64)

CBMCONVERT(-P -l 123.lnx)
MD5SUM(ca80b5d5492283789cb53c1868783b83 1.s00)
MD5SUM(2abfea98086627c448530dae6f19970f 2.u00)
MD5SUM(ac1246d517a9d6db969185fe60746900 3.d00)
MD5SUM(bfd9329a6de5030771240bfe3305c89b 4.p00)
MD5SUM(bdbf8dae4e0a90f61ab9ad0bccd679b4 5.r00)

EXECUTE_PROGRAM(${DISK2ZIP} 123.d64 123)
MD5SUM(0c66b6561fb968faeb751eb94a68098b 1!123)
MD5SUM(ef4bae7508940492d5452b4f13965cab 2!123)
MD5SUM(d7cc35a754179773a8b55e234d2fefda 3!123)
MD5SUM(0cae9f355cf09bc153272c4b8d2b97cd 4!123)
EXECUTE_PROGRAM(${ZIP2DISK} 123 4.d64)
EXECUTE_PROGRAM(${CMAKE_COMMAND} -E compare_files 123.d64 4.d64)
EXECUTE_PROGRAM_EXPECT(3 ${CBMCONVERT} -i0 -D4 123.d64 4.d64)
EXECUTE_PROGRAM_EXPECT(3 ${CBMCONVERT} -i1 -D4 123.d64 4.d64)
EXECUTE_PROGRAM_EXPECT(3 ${CBMCONVERT} -i2 -D4 123.d64 4.d64)
EXECUTE_PROGRAM_EXPECT(3 ${CBMCONVERT} -o1 -D4 123.d64 4.d64)
EXECUTE_PROGRAM_EXPECT(3 ${CBMCONVERT} -o2 -D4 123.d64 4.d64)
EXECUTE_PROGRAM_EXPECT(1 ${CBMCONVERT} -o3 -D4 123.d64 4.d64)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -o2 -D4 123.d64 4,p)
CBMCONVERT(-o1 -D4 123.d64 4,p)
MD5SUM(274f94a63ada0913cf717677e536cdf9 124.d64)
CBMCONVERT(-D4o 124.d64 4,p)
MD5SUM(9f97674c11283f44f126cc2a0f9ba626 124.d64)
CBMCONVERT(-D4d 124.d64 4,p)
MD5SUM(b81b94575160a67dd08473ff59697cd6 124.d64)
FILE(REMOVE 124.d64)
EXECUTE_PROGRAM(${CMAKE_COMMAND} -E compare_files 123.d64 4.d64)
EXECUTE_PROGRAM_EXPECT(1 ${CBMCONVERT} -D4 123.d64 -i-)
EXECUTE_PROGRAM_EXPECT(1 ${CBMCONVERT} -L 4.lnx -i)

FILE(REMOVE 123.d64)
CBMCONVERT(-p -D4o 123.d64 1.s00 2.u00 3.d00 4.p00 5.r00)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT}
  -p -D4 123.d64 1.s00 2.u00 3.d00 4.p00 5.r00)
CBMCONVERT(-D4o 123.d64 -n 5.l7f)
EXECUTE_PROGRAM(${CMAKE_COMMAND} -E compare_files 123.d64 4.d64)
CBMCONVERT(-D7 123.d71 -d 123.d64)
MD5SUM(e68e3ec85b137a27135e1fcf6642b6e6 123.d71)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D8 123.d81 -d 123.d71)
MD5SUM(d9f43c20861e389dec40a976c82f104f 123.d81)
FILE(REMOVE 123.d81)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D8 123.d81 -d 123.d64)
MD5SUM(d9f43c20861e389dec40a976c82f104f 123.d81)
FILE(REMOVE cpm.d64 cpm.d71 cpm.d81 123.d81 123.d64)
CBMCONVERT(-D4 123.d64 -d 123.d71)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -M4 cpm.d64 -n 4,p 4,p)
MD5SUM(0cff62463b61d17d6010ef369a11a61f cpm.d64)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -M7 cpm.d71 -n 4,p 4,p)
MD5SUM(d4c62a9dc77f0ddeeb44aa6502bf7066 cpm.d71)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -M8 cpm.d81 -n 4,p 4,p)
MD5SUM(c59ea7c35ad2f8888a9717af30665170 cpm.d81)
CBMCONVERT(-o2 -L cpm.lnx -m cpm.d64)
MD5SUM(31036a537e19832da30b21e630867606 cpm.lnx)
CBMCONVERT(-o2 -L cpm.lnx -m cpm.d71)
MD5SUM(31036a537e19832da30b21e630867606 cpm.lnx)
CBMCONVERT(-o2 -L cpm.lnx -m cpm.d81)
MD5SUM(31036a537e19832da30b21e630867606 cpm.lnx)
CBMCONVERT(-M4o cpm.d64 4,p)
CBMCONVERT(-M7o cpm.d71 4,p)
CBMCONVERT(-M8o cpm.d81 4,p)
MD5SUM(0cff62463b61d17d6010ef369a11a61f cpm.d64)
MD5SUM(d4c62a9dc77f0ddeeb44aa6502bf7066 cpm.d71)
MD5SUM(c59ea7c35ad2f8888a9717af30665170 cpm.d81)
CBMCONVERT(-L cpm.lnx -m cpm.d64)
MD5SUM(31036a537e19832da30b21e630867606 cpm.lnx)
CBMCONVERT(-L cpm.lnx -m cpm.d71)
MD5SUM(31036a537e19832da30b21e630867606 cpm.lnx)
CBMCONVERT(-L cpm.lnx -m cpm.d81)
MD5SUM(31036a537e19832da30b21e630867606 cpm.lnx)
CBMCONVERT(-M4d cpm.d64 4,p)
CBMCONVERT(-M7d cpm.d71 4,p)
CBMCONVERT(-M8d cpm.d81 4,p)
MD5SUM(7f8de666043044faed36f3eb8cc1a7d2 cpm.d64)
MD5SUM(14e270f181f0dd8522963083400bb9ca cpm.d71)
MD5SUM(ef66779668781691a6d642acf89345b6 cpm.d81)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -L cpm.lnx -m cpm.d64)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -L cpm.lnx -m cpm.d71)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -L cpm.lnx -m cpm.d81)
CBMCONVERT(-o2 -L cpm.lnx -m cpm.d64)
MD5SUM(8727feeb73ad976f26c11641121d82b6 cpm.lnx)
CBMCONVERT(-o2 -L cpm.lnx -m cpm.d71)
MD5SUM(8727feeb73ad976f26c11641121d82b6 cpm.lnx)
CBMCONVERT(-o2 -L cpm.lnx -m cpm.d81)
MD5SUM(8727feeb73ad976f26c11641121d82b6 cpm.lnx)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D7 123.d71 -m cpm.d64)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D7 123.d71 -m cpm.d71)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D7 123.d71 -m cpm.d81)
MD5SUM(e68e3ec85b137a27135e1fcf6642b6e6 123.d71)
CBMCONVERT(-D7o 123.d71 -m cpm.d64)
CBMCONVERT(-D7o 123.d71 -m cpm.d71)
CBMCONVERT(-D7o 123.d71 -m cpm.d81)
MD5SUM(e68e3ec85b137a27135e1fcf6642b6e6 123.d71)
FILE(REMOVE cpm.lnx cpm.d64 cpm.d71 cpm.d81 123.d71 123.d81)
EXECUTE_PROGRAM(${CMAKE_COMMAND} -E compare_files 123.d64 4.d64)

FILE(RENAME 5.l7f 5.lFF)
CBMCONVERT(-P 5.lFF)
MD5SUM(3823542b7a4411bd21e4a67129f065e5 5_lff.p00)
FILE(RENAME 5.lFF 5,f)
CBMCONVERT(-P 5,f)
MD5SUM(770b3146cd9821b9a89159a972afb539 5_f.p00)
FILE(RENAME 5,f 5.rel)
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D4 123.d64 5.rel)

FILE(REMOVE 1,s 2,u 3,d 4,p 5_lff.p00 5_f.p00
  1.s00 2.u00 3.d00 4.p00 5.r00 123.lnx 123.d64 4.d64
  1!123 2!123 3!123 4!123)

FILE(RENAME 5.rel 5.l7f)

# Write a maximum-size REL file for 1541,
# consisting of 1,316 records of 127 bytes each.
# 167,132 bytes total = (664-6) * 254 bytes.
SET(b9 "${b}${b}${b}${b}${b}${b}${b}${b}${b}")
SET(c "${b9}${b9}${b9}${b9}${b9}${b9}${b9}${b9}")
SET(d "${c}${c}${c}${c}${c}${c}${c}${c}${c}")
SET(e "abcdefghijklmnopqrstuvwxyz12345")
SET(e "${e}${e}${e}${e}${e}${e}${e}")
FILE(WRITE 5.l7f "${d}${b9}${b}${b}${b}${b}${b}${b}${b}${b}${e}")
CBMCONVERT(-L 5.lnx 5.l7f)
MD5SUM(514088379f416a1d8fa7edbd1612a137 5.lnx)
CBMCONVERT(-D4 5.d64 5.l7f)
MD5SUM(a5e8db2aa88194eda97e00f99fe72151 5.d64)
FILE(RENAME 5.l7f 5.rel)
CBMCONVERT(-I -l 5.lnx)
EXECUTE_PROGRAM(${CMAKE_COMMAND} -E compare_files 5.l7F 5.rel)
FILE(REMOVE 5.l7F)
CBMCONVERT(-d 5.d64)
EXECUTE_PROGRAM(${CMAKE_COMMAND} -E compare_files 5.l7F 5.rel)
FILE(REMOVE 5.l7F 5.rel 5.lnx 5.d64)

STRING(ASCII 1 1 1 1 129 1 t)
FILE(WRITE 1!foo "${t}")
EXECUTE_PROGRAM_EXPECT(4 ${ZIP2DISK} foo foo.d64)
FILE(APPEND 1!foo "1")
EXECUTE_PROGRAM_EXPECT(4 ${ZIP2DISK} foo foo.d64)
FILE(APPEND 1!foo "2")
EXECUTE_PROGRAM_EXPECT(4 ${ZIP2DISK} foo foo.d64)
FILE(APPEND 1!foo "2")
EXECUTE_PROGRAM_EXPECT(4 ${ZIP2DISK} foo foo.d64)
STRING(ASCII 1 1 1 1 129 1 1 1 1 t)
FILE(WRITE 1!foo "${t}")
EXECUTE_PROGRAM_EXPECT(4 ${ZIP2DISK} foo foo.d64)
STRING(ASCII 1 1 1 1 129 1 100 1 1 255 2 3 4 t)
FILE(WRITE 1!foo "${t}")
EXECUTE_PROGRAM_EXPECT(4 ${ZIP2DISK} foo foo.d64)
STRING(ASCII 1 1 1 1 129 1 2 3 4 5 t)
FILE(WRITE 1!foo "${t}")
EXECUTE_PROGRAM_EXPECT(4 ${ZIP2DISK} foo foo.d64)
STRING(ASCII 1 1 1 1 65 1 t)
FILE(WRITE 1!foo "${t}")
EXECUTE_PROGRAM_EXPECT(4 ${ZIP2DISK} foo foo.d64)
STRING(ASCII 1 1 1 1 1 1 t)
FILE(WRITE 1!foo "${t}")
EXECUTE_PROGRAM_EXPECT(4 ${ZIP2DISK} foo foo.d64)
FILE(REMOVE 1!foo foo.d64)

STRING(ASCII 1 2 1 2 1 r)
STRING(RANDOM LENGTH 187 r2)
FILE(WRITE foo.c2n "${r}${r2}")
STRING(ASCII 4 2 1 2 1 s)
FILE(APPEND foo.c2n "${s}${r2}")
STRING(ASCII 2 t)
STRING(ASCII 5 v)
STRING(ASCII 6 w)
STRING(RANDOM LENGTH 191 u)
FILE(APPEND foo.c2n "${t}${u}${v}${u}${w}${u}")
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D4 foo.d64 -c foo.c2n)
FILE(WRITE foo.c2n "${u}")
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D4 foo.d64 -c foo.c2n)
FILE(WRITE foo.c2n "${s}${s2}")
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D4 foo.d64 -c foo.c2n)
FILE(APPEND foo.c2n "${t}truncated")
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D4 foo.d64 -c foo.c2n)
STRING(ASCII 1 1 4 2 4 x)
FILE(WRITE foo.c2n "${x}${r2}")
EXECUTE_PROGRAM_EXPECT(4 ${CBMCONVERT} -D4 foo.d64 -c foo.c2n)
FILE(REMOVE foo.c2n foo.d64)
