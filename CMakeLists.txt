
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

CMAKE_MINIMUM_REQUIRED(VERSION 2.6 FATAL_ERROR)
CMAKE_POLICY(VERSION 2.6)
IF(LUAROCKS_PREFIX)
    MESSAGE(STATUS "Installing Torch through Luarocks")
    STRING(REGEX REPLACE "(.*)lib/luarocks/rocks.*" "\\1" CMAKE_INSTALL_PREFIX  "${LUAROCKS_PREFIX}")
    MESSAGE(STATUS "Prefix inferred from Luarocks: ${CMAKE_INSTALL_PREFIX}")
ENDIF()
FIND_PACKAGE(Torch REQUIRED)
FIND_PACKAGE(ZMQ REQUIRED)

SET(src zmq.c)
SET(luasrc zmq.lua)
INCLUDE_DIRECTORIES(${ZMQ_INCLUDE_DIR})
ADD_TORCH_PACKAGE(luazmq "${src}" "${luasrc}" "Image Processing")
TARGET_LINK_LIBRARIES(luazmq luaT TH ${ZMQ_LIBRARY})

SET(src)
SET(luasrc init.lua)
ADD_TORCH_PACKAGE(parallel "${src}" "${luasrc}" "Parallel Computing")
