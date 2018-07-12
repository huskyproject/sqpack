cmake_minimum_required(VERSION 2.8.11)
PROJECT(sqpack C)
set(CMAKE_INCLUDE_CURRENT_DIR ON)


if(CMAKE_BUILD_TYPE MATCHES Debug)
  ADD_DEFINITIONS(-g -DDEBUG -D_DEBUG)
  set(CMAKE_CONFIGURATION_TYPES "Debug" CACHE STRING "My multi config types" FORCE)
else(CMAKE_BUILD_TYPE MATCHES Debug)
  ADD_DEFINITIONS(-DNDEBUG)
  set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "My multi config types" FORCE)
endif(CMAKE_BUILD_TYPE MATCHES Debug)

if (MSVC)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../out/lib")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../out/bin")
  ADD_DEFINITIONS(-DWIN32 -D_WINDOWS -D_DLL -D_CONSOLE -D_MAKE_DLL  -D_CRT_SECURE_NO_WARNINGS)
  include_directories("${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_SOURCE_DIR}/../huskylib;${CMAKE_CURRENT_SOURCE_DIR}/../fidoconf;${CMAKE_CURRENT_SOURCE_DIR}/../smapi;")
  if(CMAKE_BUILD_TYPE MATCHES Debug)
    list( APPEND CMAKE_FIND_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../out/lib/Debug" )
  else(CMAKE_BUILD_TYPE MATCHES Debug)
    list( APPEND CMAKE_FIND_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../out/lib/Release" )
  endif(CMAKE_BUILD_TYPE MATCHES Debug)
else ()
  ADD_DEFINITIONS(-Wall -DUNIX)
endif (MSVC)

find_library(husky_LIB NAMES husky PATHS "${CMAKE_FIND_ROOT_PATH}")
find_library(fidoconfig_LIB NAMES fidoconfig PATHS "${CMAKE_FIND_ROOT_PATH}")
find_library(smapi_LIB NAMES smapi PATHS "${CMAKE_FIND_ROOT_PATH}")

message("-------------------------")
message("System            : ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}")
message("Compiler          : ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message("Prefix            : ${CMAKE_INSTALL_PREFIX} (run \"cmake . -DCMAKE_INSTALL_PREFIX:PATH=/other\" for other prefix)")
message("libhusky          : ${husky_LIB}")
message("libfidoconfig     : ${fidoconfig_LIB}")
message("libsmapi          : ${smapi_LIB}")
message("Build type        : ${CMAKE_CONFIGURATION_TYPES}")
message("-------------------------")

ADD_EXECUTABLE(sqpack
  sqpack.c
)
target_link_libraries(sqpack ${husky_LIB} ${fidoconfig_LIB} ${smapi_LIB})

if (NOT MSVC)
  INSTALL(TARGETS sqpack
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
  )
  set(CPACK_GENERATOR ${CPACK_GENERATOR} TGZ)
endif (NOT MSVC)

set (CPACK_PACKAGE_VERSION_MAJOR "1")
set (CPACK_PACKAGE_VERSION_MINOR "9")
file(READ cvsdate.h CPACK_PACKAGE_VERSION_PATCH LIMIT 10 OFFSET 17)
string(SUBSTRING ${CPACK_PACKAGE_VERSION_PATCH} 0 10 CPACK_PACKAGE_VERSION_PATCH)
string(REGEX REPLACE "-" "" CPACK_PACKAGE_VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH})
INCLUDE(CPack)
