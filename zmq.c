/*
 * Copyright (c) 2010 Aleksey Yeschenko <aleksey@yeschenko.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define LUA_LIB

#include "TH.h"
#include "luaT.h"
//#include "lua.h"
#include "lauxlib.h"

#include <zmq.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

#ifdef min
#undef min
#endif
#define min( a, b ) ( ((a) < (b)) ? (a) : (b) )

#ifdef max
#undef max
#endif
#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )

#define torch_(NAME) TH_CONCAT_3(torch_, Real, NAME)
#define torch_Storage TH_CONCAT_STRING_3(torch., Real, Storage)
#define Lzmq_(NAME) TH_CONCAT_3(Lzmq_, Real, NAME)

/* detect zmq version >= 2.1.0 */
#define VERSION_2_1 0
#if defined(ZMQ_VERSION)
#if (ZMQ_VERSION >= ZMQ_MAKE_VERSION(2,1,0))
#undef VERSION_2_1
#define VERSION_2_1 1
#endif
#endif

#define MT_ZMQ_CONTEXT "MT_ZMQ_CONTEXT"
#define MT_ZMQ_SOCKET  "MT_ZMQ_SOCKET"

#ifndef ZMQ_DONTWAIT
#   define ZMQ_DONTWAIT   ZMQ_NOBLOCK
#endif
#ifndef ZMQ_RCVHWM
#   define ZMQ_RCVHWM     ZMQ_HWM
#endif
#ifndef ZMQ_SNDHWM
#   define ZMQ_SNDHWM     ZMQ_HWM
#endif
#if ZMQ_VERSION_MAJOR == 2
#   define zmq_ctx_destroy(context) zmq_term(context)
#   define zmq_msg_send(msg,sock,opt) zmq_send (sock, msg, opt)
#   define zmq_msg_recv(msg,sock,opt) zmq_recv (sock, msg, opt)
#endif

typedef struct {
    void *ptr;
    int should_free;
} zmq_ptr;

static int Lzmq_version(lua_State *L)
{
    int major, minor, patch;

    zmq_version(&major, &minor, &patch);

    lua_createtable(L, 3, 0);

    lua_pushinteger(L, 1);
    lua_pushinteger(L, major);
    lua_settable(L, -3);

    lua_pushinteger(L, 2);
    lua_pushinteger(L, minor);
    lua_settable(L, -3);

    lua_pushinteger(L, 3);
    lua_pushinteger(L, patch);
    lua_settable(L, -3);

    return 1;
}

static int Lzmq_push_error(lua_State *L)
{
    const char *error;
    lua_pushnil(L);
    switch(zmq_errno()) {
    case EAGAIN:
        lua_pushliteral(L, "timeout");
        break;
    case ETERM:
        lua_pushliteral(L, "closed");
        break;
    default:
        error = zmq_strerror(zmq_errno());
        lua_pushlstring(L, error, strlen(error));
        break;
    }
    return 2;
}

static int Lzmq_init(lua_State *L)
{
    zmq_ptr *ctx = lua_newuserdata(L, sizeof(zmq_ptr));
    luaL_getmetatable(L, MT_ZMQ_CONTEXT);
    lua_setmetatable(L, -2);

    if (lua_islightuserdata(L, 1)) {
        // Treat a light userdata as a raw ZMQ context object, which
        // we'll silently wrap.  (And we won't automatically call term
        // on it.)

        ctx->ptr = lua_touserdata(L, 1);
        ctx->should_free = 0;
        return 1;
    }

    int io_threads = luaL_checkint(L, 1);

    ctx->ptr = zmq_init(io_threads);

    if (!ctx->ptr) {
        return Lzmq_push_error(L);
    }

    // toboolean defaults to false, but we want a missing param #2
    // to mean true
    if (lua_isnil(L, 2)) {
        ctx->should_free = 1;
    } else {
        ctx->should_free = lua_toboolean(L, 2);
    }

    return 1;
}

static int Lzmq_term(lua_State *L)
{
    zmq_ptr *ctx = luaL_checkudata(L, 1, MT_ZMQ_CONTEXT);

    if(ctx->ptr != NULL) {
        if(zmq_ctx_destroy(ctx->ptr) == 0) {
            ctx->ptr = NULL;
        } else {
            return Lzmq_push_error(L);
        }
    }

    lua_pushboolean(L, 1);

    return 1;
}

static int Lzmq_ctx_gc(lua_State *L)
{
    zmq_ptr *ctx = luaL_checkudata(L, 1, MT_ZMQ_CONTEXT);
    if (ctx->should_free) {
        return Lzmq_term(L);
    } else {
        return 0;
    }
}

static int Lzmq_ctx_lightuserdata(lua_State *L)
{
    zmq_ptr *ctx = luaL_checkudata(L, 1, MT_ZMQ_CONTEXT);
    lua_pushlightuserdata(L, ctx->ptr);
    return 1;
}

static int Lzmq_socket(lua_State *L)
{
    zmq_ptr *ctx = luaL_checkudata(L, 1, MT_ZMQ_CONTEXT);
    int type = luaL_checkint(L, 2);

    zmq_ptr *s = lua_newuserdata(L, sizeof(zmq_ptr));

    s->ptr = zmq_socket(ctx->ptr, type);

    if (!s->ptr) {
        return Lzmq_push_error(L);
    }

    luaL_getmetatable(L, MT_ZMQ_SOCKET);
    lua_setmetatable(L, -2);

    return 1;
}

static int Lzmq_close(lua_State *L)
{
    zmq_ptr *s = luaL_checkudata(L, 1, MT_ZMQ_SOCKET);

    if(s->ptr != NULL) {
        if(zmq_close(s->ptr) == 0) {
            s->ptr = NULL;
        } else {
            return Lzmq_push_error(L);
        }
    }

    lua_pushboolean(L, 1);

    return 1;
}

#if VERSION_2_1
#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif
#endif

static int Lzmq_setsockopt(lua_State *L)
{
    zmq_ptr *s = luaL_checkudata(L, 1, MT_ZMQ_SOCKET);
    int option = luaL_checkint(L, 2);

    int rc = 0;

    switch (option) {
#if VERSION_2_1
    case ZMQ_FD:
        {
            socket_t optval = (socket_t) luaL_checklong(L, 3);
            rc = zmq_setsockopt(s->ptr, option, (void *) &optval, sizeof(socket_t));
        }
        break;
    case ZMQ_EVENTS:
        {
            int32_t optval = (int32_t) luaL_checklong(L, 3);
            rc = zmq_setsockopt(s->ptr, option, (void *) &optval, sizeof(int32_t));
        }
        break;
    case ZMQ_TYPE:
    case ZMQ_LINGER:
    case ZMQ_RECONNECT_IVL:
    case ZMQ_BACKLOG:
        {
            int optval = (int) luaL_checklong(L, 3);
            rc = zmq_setsockopt(s->ptr, option, (void *) &optval, sizeof(int));
        }
        break;
#endif
#if ZMQ_SWAP
    case ZMQ_SWAP:
#endif
#if ZMQ_MCAST_LOOP
    case ZMQ_MCAST_LOOP:
#endif
    case ZMQ_RATE:
    case ZMQ_RECOVERY_IVL:
        {
            int64_t optval = (int64_t) luaL_checklong(L, 3);
            rc = zmq_setsockopt(s->ptr, option, (void *) &optval, sizeof(int64_t));
        }
        break;
    case ZMQ_IDENTITY:
    case ZMQ_SUBSCRIBE:
    case ZMQ_UNSUBSCRIBE:
        {
            size_t optvallen;
            const char *optval = luaL_checklstring(L, 3, &optvallen);
            rc = zmq_setsockopt(s->ptr, option, (void *) optval, optvallen);
        }
        break;
#ifdef ZMQ_HWM
    case ZMQ_HWM:
#endif
    case ZMQ_SNDHWM:
    case ZMQ_RCVHWM:
    case ZMQ_AFFINITY:
    case ZMQ_SNDBUF:
    case ZMQ_RCVBUF:
        {
            uint64_t optval = (uint64_t) luaL_checklong(L, 3);
            rc = zmq_setsockopt(s->ptr, option, (void *) &optval, sizeof(uint64_t));
        }
        break;
    default:
        rc = -1;
        errno = EINVAL;
    }

    if (rc != 0) {
        return Lzmq_push_error(L);
    }

    lua_pushboolean(L, 1);

    return 1;
}

static int Lzmq_getsockopt(lua_State *L)
{
    zmq_ptr *s = luaL_checkudata(L, 1, MT_ZMQ_SOCKET);
    int option = luaL_checkint(L, 2);

    size_t optvallen;

    int rc = 0;

    switch (option) {
#if VERSION_2_1
    case ZMQ_FD:
        {
            socket_t optval;
            optvallen = sizeof(socket_t);
            rc = zmq_getsockopt(s->ptr, option, (void *) &optval, &optvallen);
            if (rc == 0) {
                lua_pushinteger(L, (lua_Integer) optval);
                return 1;
            }
        }
        break;
    case ZMQ_EVENTS:
        {
            int32_t optval;
            optvallen = sizeof(int32_t);
            rc = zmq_getsockopt(s->ptr, option, (void *) &optval, &optvallen);
            if (rc == 0) {
                lua_pushinteger(L, (lua_Integer) optval);
                return 1;
            }
        }
        break;
    case ZMQ_TYPE:
    case ZMQ_LINGER:
    case ZMQ_RECONNECT_IVL:
    case ZMQ_BACKLOG:
        {
            int optval;
            optvallen = sizeof(int);
            rc = zmq_getsockopt(s->ptr, option, (void *) &optval, &optvallen);
            if (rc == 0) {
                lua_pushinteger(L, (lua_Integer) optval);
                return 1;
            }
        }
        break;
#endif
#if ZMQ_SWAP
    case ZMQ_SWAP:
#endif
#if ZMQ_MCAST_LOOP
    case ZMQ_MCAST_LOOP:
#endif
    case ZMQ_RATE:
    case ZMQ_RECOVERY_IVL:
    case ZMQ_RCVMORE:
        {
            int64_t optval;
            optvallen = sizeof(int64_t);
            rc = zmq_getsockopt(s->ptr, option, (void *) &optval, &optvallen);
            if (rc == 0) {
                lua_pushinteger(L, (lua_Integer) optval);
                return 1;
            }
        }
        break;
    case ZMQ_IDENTITY:
        {
            char id[256];
            memset((void *)id, '\0', 256);
            optvallen = 256;
            rc = zmq_getsockopt(s->ptr, option, (void *)id, &optvallen);
            id[255] = '\0';
            if (rc == 0) {
                lua_pushstring(L, id);
                return 1;
            }
        }
        break;
#ifdef ZMQ_HWM
    case ZMQ_HWM:
#endif
    case ZMQ_SNDHWM:
    case ZMQ_RCVHWM:
    case ZMQ_AFFINITY:
    case ZMQ_SNDBUF:
    case ZMQ_RCVBUF:
        {
            uint64_t optval;
            optvallen = sizeof(uint64_t);
            rc = zmq_getsockopt(s->ptr, option, (void *) &optval, &optvallen);
            if (rc == 0) {
                lua_pushinteger(L, (lua_Integer) optval);
                return 1;
            }
        }
        break;
    default:
        rc = -1;
        errno = EINVAL;
    }

    if (rc != 0) {
        return Lzmq_push_error(L);
    }

    lua_pushboolean(L, 1);

    return 1;
}

static int Lzmq_bind(lua_State *L)
{
    zmq_ptr *s = luaL_checkudata(L, 1, MT_ZMQ_SOCKET);
    const char *addr = luaL_checkstring(L, 2);

    if (zmq_bind(s->ptr, addr) != 0) {
        return Lzmq_push_error(L);
    }

    lua_pushboolean(L, 1);

    return 1;
}

static int Lzmq_connect(lua_State *L)
{
    zmq_ptr *s = luaL_checkudata(L, 1, MT_ZMQ_SOCKET);
    const char *addr = luaL_checkstring(L, 2);

    if (zmq_connect(s->ptr, addr) != 0) {
        return Lzmq_push_error(L);
    }

    lua_pushboolean(L, 1);

    return 1;
}

static int Lzmq_send(lua_State *L)
{
    zmq_ptr *s = luaL_checkudata(L, 1, MT_ZMQ_SOCKET);
    size_t msg_size;
    const char *data = luaL_checklstring(L, 2, &msg_size);
    int flags = luaL_optint(L, 3, 0);

    zmq_msg_t msg;
    if(zmq_msg_init_size(&msg, msg_size) != 0) {
        return Lzmq_push_error(L);
    }
    memcpy(zmq_msg_data(&msg), data, msg_size);

    int rc = zmq_msg_send(&msg, s->ptr, flags);

    if(zmq_msg_close(&msg) != 0) {
        return Lzmq_push_error(L);
    }

    if (rc == -1) {
        return Lzmq_push_error(L);
    }

    lua_pushboolean(L, 1);

    return 1;
}

static int Lzmq_recv(lua_State *L)
{
    zmq_ptr *s = luaL_checkudata(L, 1, MT_ZMQ_SOCKET);
    int flags = luaL_optint(L, 2, 0);

    zmq_msg_t msg;
    if(zmq_msg_init(&msg) != 0) {
        return Lzmq_push_error(L);
    }

    if(zmq_msg_recv(&msg, s->ptr, flags) == -1) {
        // Best we can do in this case is try to close and hope for the best.
        zmq_msg_close(&msg);
        return Lzmq_push_error(L);
    }

    lua_pushlstring(L, zmq_msg_data(&msg), zmq_msg_size(&msg));

    if(zmq_msg_close(&msg) != 0) {
        // Above string will be poped from the stack by the normalising code
        // upon sucessful return.
        return Lzmq_push_error(L);
    }

    return 1;
}



static const luaL_reg zmqlib[] = {
    {"version",    Lzmq_version},
    {"init",       Lzmq_init},
    {NULL,         NULL}
};

static const luaL_reg ctxmethods[] = {
    {"__gc",       Lzmq_ctx_gc},
    {"lightuserdata", Lzmq_ctx_lightuserdata},
    {"term",       Lzmq_term}, 
    {"socket",     Lzmq_socket},
    {NULL,         NULL}
};

static const luaL_reg sockmethods[] = {
    {"__gc",    Lzmq_close},
    {"close",   Lzmq_close},
    {"setopt",  Lzmq_setsockopt},
    {"getopt",  Lzmq_getsockopt},
    {"bind",    Lzmq_bind},
    {"connect", Lzmq_connect},
    {"send",    Lzmq_send},
    {"recv",    Lzmq_recv},
    {NULL,      NULL}
};

#define set_zmq_const(s) lua_pushinteger(L,ZMQ_##s); lua_setfield(L, -2, #s);

#include "generic/zmq.c"
#include "THGenerateAllTypes.h"

DLL_EXPORT int luaopen_libluazmq(lua_State *L)
{
    /* context metatable. */
    luaL_newmetatable(L, MT_ZMQ_CONTEXT);
    luaL_register(L, NULL, ctxmethods);
    lua_pushvalue(L, -1);
    lua_setfield(L, -1, "__index");

    /* socket metatable. */
    luaL_newmetatable(L, MT_ZMQ_SOCKET);
    luaL_register(L, NULL, sockmethods);
    lua_pushvalue(L, -1);
    lua_setfield(L, -1, "__index");

    luaL_register(L, "zmq", zmqlib);

    /* Socket types. */
    set_zmq_const(PAIR);
    set_zmq_const(PUB);
    set_zmq_const(SUB);
    set_zmq_const(REQ);
    set_zmq_const(REP);
    set_zmq_const(XREQ);
    set_zmq_const(XREP);
    set_zmq_const(PULL);
    set_zmq_const(PUSH);

    /* Socket options. */
#ifdef ZMQ_HWM
    set_zmq_const(HWM);
#else
    set_zmq_const(SNDHWM);
    set_zmq_const(RCVHWM);
#endif
#ifdef ZMQ_SWAP
    set_zmq_const(SWAP);
#endif
    set_zmq_const(AFFINITY);
    set_zmq_const(IDENTITY);
    set_zmq_const(SUBSCRIBE);
    set_zmq_const(UNSUBSCRIBE);
    set_zmq_const(RATE);
    set_zmq_const(RECOVERY_IVL);
#ifdef ZMQ_MCAST_LOOP
    set_zmq_const(MCAST_LOOP);
#endif
    set_zmq_const(SNDBUF);
    set_zmq_const(RCVBUF);
    set_zmq_const(RCVMORE);
#if VERSION_2_1
    set_zmq_const(FD);
    set_zmq_const(EVENTS);
    set_zmq_const(TYPE);
    set_zmq_const(LINGER);
    set_zmq_const(RECONNECT_IVL);
    set_zmq_const(BACKLOG);

    /* POLL events */
    set_zmq_const(POLLIN);
    set_zmq_const(POLLOUT);
    set_zmq_const(POLLERR);
#endif

    /* Send/recv options. */
    set_zmq_const(NOBLOCK);
    set_zmq_const(DONTWAIT);
    set_zmq_const(SNDMORE);

    Lzmq_CharInit(L);
    Lzmq_ByteInit(L);
    Lzmq_ShortInit(L);
    Lzmq_IntInit(L);
    Lzmq_LongInit(L);
    Lzmq_FloatInit(L);
    Lzmq_DoubleInit(L);

    return 1;
}
