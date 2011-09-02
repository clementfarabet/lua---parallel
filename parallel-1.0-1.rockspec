
package = "parallel"
version = "1.0-1"

source = {
   url = "parallel-1.0-1.tgz"
}

description = {
   summary = "a (simple) parallel computing framework for Lua",
   detailed = [[
         A package that provides a simple mechanism to
         dispatch Lua scripts as independant processes
            and communicate via a super raw shared memory
         buffer                                       
   ]],
   homepage = "https://github.com/clementfarabet/lua---parallel/wiki",
   license = "MIT/X11"
}

dependencies = {
   "lua >= 5.1",
   'torch',
   'sys'
}

build = {
   type = "cmake",

   cmake = [[
         cmake_minimum_required(VERSION 2.8)

         set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

         # infer path for Torch7
         string (REGEX REPLACE "(.*)lib/luarocks/rocks.*" "\\1" TORCH_PREFIX "${CMAKE_INSTALL_PREFIX}" )
         message (STATUS "Found Torch7, installed in: " ${TORCH_PREFIX})

         find_package (Torch REQUIRED)

         set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

         include_directories (${TORCH_INCLUDE_DIR} ${PROJECT_SOURCE_DIR})
         link_directories    (${TORCH_LIBRARY_DIR})

         # not working right now
         add_library (luazmq SHARED zmq.c)
         target_link_libraries (luazmq zmq ${TORCH_LIBRARIES})

         add_library (parallel SHARED parallel.c)
         target_link_libraries (parallel ${TORCH_LIBRARIES})
         # parallel target specific cflags
         set_property(TARGET parallel APPEND PROPERTY COMPILE_FLAGS "-fopenmp")
         

         install_targets(/lib luazmq)
         install_files(/lua zmq.lua)

         install_files(/lua/parallel init.lua)
         install_targets(/lib parallel)
         
   ]],

   variables = {
      CMAKE_INSTALL_PREFIX = "$(PREFIX)"
   }
}
