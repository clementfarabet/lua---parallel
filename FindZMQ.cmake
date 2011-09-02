# - this module looks for libzmq
#
# Defines:
#  ZMQ_INCLUDE_DIR: include path for zmq.h
#  ZMQ_LIBRARy:     required lib: libzmq
#  ZMQ_FOUND if the lib was found
#
# Marco Scoffier Sept. 1, 2011

SET(ZMQ_FOUND 0)
IF(WIN32)
  FIND_LIBRARY(ZMQ_LIBRARY
    libzmq
    ${ZMQ_ROOT}
    )
ELSE( WIN32 )
  IF(NOT ZMQ_ROOT)
    SET(ZMQ_ROOT $ENV{ZMQ_ROOT})
  ENDIF(NOT ZMQ_ROOT)
  IF(NOT ZMQ_ROOT)
    MESSAGE(STATUS "** WARNING no ZMQ_ROOT setting to $HOME/local")
    MESSAGE(STATUS "** you can set the correct ZMQ_ROOT in your environment")
    MESSAGE(STATUS "** eg. bash: export ZMQ_ROOT=<somewhere>/local")
    SET(ZMQ_ROOT $ENV{HOME}/local)
  ENDIF(NOT ZMQ_ROOT)    
  MESSAGE(STATUS "Searching for libzmq in : " ${ZMQ_ROOT} "/lib")
  FIND_LIBRARY(ZMQ_LIBRARY
    NAMES libzmq zmq 
    PATHS 
	${ZMQ_ROOT}/lib  
	/lib
	/usr/lib	
	/usr/local/lib
	/opt/lib
	$ENV{LD_LIBRARY_PATH}
    )	
  FIND_PATH(ZMQ_INCLUDE_DIR
    "zmq.h"
    PATHS
	${ZMQ_ROOT}/include
	/include
	/usr/include
	/usr/local/include
	/opt/include
	$ENV{C_INCLUDE_PATH}
    )
ENDIF(WIN32)

IF(ZMQ_INCLUDE_DIR)
	SET(ZMQ_FOUND 1)
ENDIF(ZMQ_INCLUDE_DIR)