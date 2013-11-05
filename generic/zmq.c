#ifndef TH_GENERIC_FILE
#define TH_GENERIC_FILE "generic/zmq.c"
#else

static int Lzmq_(send)(lua_State *L)
{
  THStorage *storage = luaT_checkudata(L, 1, torch_Storage);

  zmq_ptr *s = luaL_checkudata(L, 2, MT_ZMQ_SOCKET);


  size_t msg_size = storage->size * sizeof(real);

  int flags = luaL_optint(L, 3, 0);

  zmq_msg_t msg;

  if(zmq_msg_init_size(&msg, msg_size) != 0) {
    return Lzmq_push_error(L);
  }

  memcpy(zmq_msg_data(&msg), storage->data, msg_size);

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

static int Lzmq_(recv)(lua_State *L)
{

  THStorage *storage = luaT_checkudata(L, 1, torch_Storage);

  zmq_ptr *s = luaL_checkudata(L, 2, MT_ZMQ_SOCKET);


  int flags = luaL_optint(L, 3, 0);

  zmq_msg_t msg;
  if(zmq_msg_init(&msg) != 0) {
    return Lzmq_push_error(L);
  }

  if(zmq_msg_recv(&msg, s->ptr, flags) == -1) {
    if (flags == 1) {
      lua_pushboolean(L, 0);
      return 1;
    }
    // Best we can do in this case is try to close and hope for the best.
    zmq_msg_close(&msg);
    return Lzmq_push_error(L);
  }

  size_t msg_size = zmq_msg_size(&msg);

  // resize destination storage
  THStorage_(resize)(storage, msg_size);

  // copy data from buffer
  memcpy(storage->data, zmq_msg_data(&msg), msg_size);

  if(zmq_msg_close(&msg) != 0) {
    // Above string will be poped from the stack by the normalising code
    // upon sucessful return.
    return Lzmq_push_error(L);
  }

  lua_pushboolean(L, 1);

  return 1;
}

static const struct luaL_reg Lzmq_(methods)[] = {
  {"send",    Lzmq_(send)},
  {"recv",    Lzmq_(recv)},
  {NULL, NULL}
};

static void Lzmq_(Init)(lua_State *L)
{
  luaT_pushmetatable(L, torch_Storage);
  luaT_registeratname(L, Lzmq_(methods), "zmq");
}

#endif
