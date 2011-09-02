#ifndef TH_GENERIC_FILE
#define TH_GENERIC_FILE "generic/zmq.c"
#else

static int Lzmq_(sendStorage)(lua_State *L)
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

  int rc = zmq_send(s->ptr, &msg, flags);

  if(zmq_msg_close(&msg) != 0) {
    return Lzmq_push_error(L);
  }

  if (rc != 0) {
    return Lzmq_push_error(L);
  }

  lua_pushboolean(L, 1);

  return 1;
}

static int Lzmq_(recvStorage)(lua_State *L)
{
  zmq_ptr *s = luaL_checkudata(L, 1, MT_ZMQ_SOCKET);
  int flags = luaL_optint(L, 2, 0);

  zmq_msg_t msg;
  if(zmq_msg_init(&msg) != 0) {
    return Lzmq_push_error(L);
  }

  if(zmq_recv(s->ptr, &msg, flags) != 0) {
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

static const struct luaL_reg Lzmq_(methods)[] = {
  {"sendStorage",    Lzmq_(sendStorage)},
  {"recvStorage",    Lzmq_(recvStorage)},
  {NULL, NULL}
};

static void Lzmq_(Init)(lua_State *L)
{
  luaT_pushmetaclass(L, torch_(Storage_id));
  luaT_registeratname(L, Lzmq_(methods), "zmq");
}

#endif
