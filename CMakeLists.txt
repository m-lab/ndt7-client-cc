cmake_minimum_required(VERSION 3.12.0)
project("libndt7")

include(CheckCXXCompilerFlag)
include(CheckFunctionExists)
include(CheckIncludeFileCXX)
include(CheckLibraryExists)
include(CheckSymbolExists)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
list(APPEND CMAKE_REQUIRED_LIBRARIES Threads::Threads)
if(("${WIN32}"))
  list(APPEND CMAKE_REQUIRED_LIBRARIES ws2_32 crypt32)
endif()

if((${APPLE}))
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/usr/local/include")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L/usr/local/lib")
endif()

check_function_exists(strtonum LIBNDT7_HAVE_STRTONUM)
if(${LIBNDT7_HAVE_STRTONUM})
  add_definitions(-DLIBNDT7_HAVE_STRTONUM)
endif()

CHECK_INCLUDE_FILE_CXX("curl/curl.h" MK_HAVE_CURL_CURL_H)
if(NOT ("${MK_HAVE_CURL_CURL_H}"))
  message(FATAL_ERROR "cannot find: curl/curl.h")
endif()
CHECK_LIBRARY_EXISTS("curl" "curl_easy_init" "" MK_HAVE_LIBCURL)
if(NOT ("${MK_HAVE_LIBCURL}"))
  message(FATAL_ERROR "cannot find: curl")
endif()
LIST(APPEND CMAKE_REQUIRED_LIBRARIES "curl")

if(("${APPLE}"))
  if(EXISTS "/usr/local/opt/openssl@1.1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/usr/local/opt/openssl@1.1/include")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/usr/local/opt/openssl@1.1/include")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/opt/openssl@1.1/lib")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L/usr/local/opt/openssl@1.1/lib")
  endif()
endif()
CHECK_INCLUDE_FILE_CXX("openssl/rsa.h" MK_HAVE_OPENSSL_RSA_H)
if(NOT ("${MK_HAVE_OPENSSL_RSA_H}"))
  message(FATAL_ERROR "cannot find: openssl/rsa.h")
endif()
CHECK_LIBRARY_EXISTS("crypto" "RSA_new" "" MK_HAVE_LIBCRYPTO)
if(NOT ("${MK_HAVE_LIBCRYPTO}"))
  message(FATAL_ERROR "cannot find: crypto")
endif()
LIST(APPEND CMAKE_REQUIRED_LIBRARIES "crypto")
CHECK_INCLUDE_FILE_CXX("openssl/ssl.h" MK_HAVE_OPENSSL_SSL_H)
if(NOT ("${MK_HAVE_OPENSSL_SSL_H}"))
  message(FATAL_ERROR "cannot find: openssl/ssl.h")
endif()
CHECK_LIBRARY_EXISTS("ssl" "SSL_read" "" MK_HAVE_LIBSSL)
if(NOT ("${MK_HAVE_LIBSSL}"))
  message(FATAL_ERROR "cannot find: ssl")
endif()
LIST(APPEND CMAKE_REQUIRED_LIBRARIES "ssl")

macro(MKSetRestrictiveCompilerFlags)
  if(("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") OR
     ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang"))
    set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -Werror")
    # https://www.owasp.org/index.php/C-Based_Toolchain_Hardening_Cheat_Sheet
    set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -Wall")
    set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -Wextra")
    set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -Wconversion")
    set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -Wcast-align")
    set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -Wformat=2")
    set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -Wformat-security")
    set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -fno-common")
    # Some options are only supported by GCC when we're compiling C code:
    if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
      set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -Wmissing-prototypes")
      set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -Wstrict-prototypes")
    else()
      set(MK_C_FLAGS "${MK_C_FLAGS} -Wmissing-prototypes")
      set(MK_C_FLAGS "${MK_C_FLAGS} -Wstrict-prototypes")
    endif()
    set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -Wmissing-declarations")
    set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -Wstrict-overflow")
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
      set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -Wtrampolines")
    endif()
    set(MK_CXX_FLAGS "${MK_CXX_FLAGS} -Woverloaded-virtual")
    set(MK_CXX_FLAGS "${MK_CXX_FLAGS} -Wreorder")
    set(MK_CXX_FLAGS "${MK_CXX_FLAGS} -Wsign-promo")
    set(MK_CXX_FLAGS "${MK_CXX_FLAGS} -Wnon-virtual-dtor")
    set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} -fstack-protector-all")
    if(NOT "${APPLE}" AND NOT "${MINGW}")
      set(MK_LD_FLAGS "${MK_LD_FLAGS} -Wl,-z,noexecstack")
      set(MK_LD_FLAGS "${MK_LD_FLAGS} -Wl,-z,now")
      set(MK_LD_FLAGS "${MK_LD_FLAGS} -Wl,-z,relro")
      set(MK_LD_FLAGS "${MK_LD_FLAGS} -Wl,-z,nodlopen")
      set(MK_LD_FLAGS "${MK_LD_FLAGS} -Wl,-z,nodump")
    elseif(("${MINGW}"))
      set(MK_LD_FLAGS "${MK_LD_FLAGS} -static")
    endif()
    add_definitions(-D_FORTIFY_SOURCES=2)
  elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # TODO(bassosimone): add support for /Wall and /analyze
    set(MK_COMMON_FLAGS "${MK_COMMON_FLAGS} /WX /W4 /EHs")
    set(MK_LD_FLAGS "${MK_LD_FLAGS} /WX")
  else()
    message(FATAL_ERROR "Compiler not supported: ${CMAKE_CXX_COMPILER_ID}")
  endif()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MK_COMMON_FLAGS} ${MK_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MK_COMMON_FLAGS} ${MK_CXX_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MK_LD_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${MK_LD_FLAGS}")
  if("${WIN32}")
    add_definitions(-D_WIN32_WINNT=0x0600) # for NI_NUMERICSERV and WSAPoll
  endif()
endmacro()
MKSetRestrictiveCompilerFlags()

set(MK_LIBNDT7_AMALGAMATE_DEST single_include/libndt7.hpp)
file(REMOVE ${MK_LIBNDT7_AMALGAMATE_DEST})
foreach(SOURCE IN ITEMS
        include/libndt7/preamble.hpp
        third_party/github.com/nlohmann/json/json.hpp
        include/libndt7/internal/assert.hpp
        include/libndt7/internal/sys.hpp
        include/libndt7/internal/logger.hpp
        include/libndt7/internal/curlx.hpp
        include/libndt7/internal/err.hpp
        include/libndt7/timeout.hpp
        include/libndt7/libndt7.h
        include/libndt7/libndt7.cpp)
  file(READ ${SOURCE} CONTENT)
  file(APPEND ${MK_LIBNDT7_AMALGAMATE_DEST} "${CONTENT}")
endforeach()

add_definitions(${CMAKE_REQUIRED_DEFINITIONS})
include_directories(${CMAKE_REQUIRED_INCLUDES}
                    ${CMAKE_SOURCE_DIR}
                    ${CMAKE_SOURCE_DIR}/include)

add_executable(curlx_test test/curlx_test.cpp)
target_link_libraries(curlx_test ${CMAKE_REQUIRED_LIBRARIES})

add_executable(libndt7-standalone-builds libndt7-standalone-builds.cpp)
target_link_libraries(libndt7-standalone-builds ${CMAKE_REQUIRED_LIBRARIES})

add_executable(tests-libndt test/libndt7_test.cpp)
target_link_libraries(tests-libndt ${CMAKE_REQUIRED_LIBRARIES})

add_executable(sys_test test/sys_test.cpp)
target_link_libraries(sys_test ${CMAKE_REQUIRED_LIBRARIES})

set(MK_LIBNDT7_AMALGAMATE_DEST2 library/libndt7.cpp)
file(REMOVE ${MK_LIBNDT7_AMALGAMATE_DEST2})
foreach(SOURCE IN ITEMS
  include/libndt7/preamble.hpp
  third_party/github.com/nlohmann/json/json.hpp
  include/libndt7/internal/assert.hpp
  include/libndt7/internal/sys.hpp
  include/libndt7/internal/logger.hpp
  include/libndt7/internal/curlx.hpp
  include/libndt7/internal/err.hpp
  include/libndt7/timeout.hpp
  include/libndt7/libndt7.cpp)
  file(READ ${SOURCE} CONTENT)
  file(APPEND ${MK_LIBNDT7_AMALGAMATE_DEST2} "${CONTENT}")
endforeach()

add_library(ndt7 STATIC ${MK_LIBNDT7_AMALGAMATE_DEST2})
set_target_properties(ndt7 PROPERTIES LINKER_LANGUAGE CXX)

add_executable(ndt7-client-cc ndt7-client-cc.cpp)
target_link_libraries(ndt7-client-cc "ndt7" ${CMAKE_REQUIRED_LIBRARIES})

enable_testing()

add_test(NAME curlx_unit_tests COMMAND curlx_test)
add_test(NAME other_unit_tests COMMAND tests-libndt)
add_test(NAME sys_unit_tests COMMAND sys_test)

add_test(NAME simple_test COMMAND ndt7-client-cc
         -download -upload -verbose -scheme=ws)
add_test(NAME modern_test COMMAND ndt7-client-cc
         -ca-bundle-path ${CMAKE_SOURCE_DIR}/third_party/curl.haxx.se/ca/cacert.pem
         -verbose -download -upload -scheme=wss)

INSTALL(PROGRAMS ndt7-client-cc DESTINATION bin)
INSTALL(FILES ${MK_LIBNDT7_AMALGAMATE_DEST} DESTINATION include/libndt7)
INSTALL(FILES include/libndt7/libndt7.h DESTINATION include/libndt7)
INSTALL(FILES libndt7.a DESTINATION lib)
