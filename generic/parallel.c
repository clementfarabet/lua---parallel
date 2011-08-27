#ifndef TH_GENERIC_FILE
#define TH_GENERIC_FILE "generic/parallel.c"
#else

static int parallel_(sendStorage)(lua_State *L) {
  THStorage *storage = luaT_checkudata(L, 1, torch_(Storage_id));
  int pid = lua_tonumber(L, 2);
  return 0;
}

static int parallel_(receiveStorage)(lua_State *L) {
  THStorage *storage = luaT_checkudata(L, 1, torch_(Storage_id));
  int pid = lua_tonumber(L, 2);
  return 0;
}

static const struct luaL_reg parallel_(methods__) [] = {
  {"sendStorage", parallel_(sendStorage)},
  {"receiveStorage", parallel_(receiveStorage)},
  {NULL, NULL}
};

static void parallel_(Init)(lua_State *L)
{
  luaT_pushmetaclass(L, torch_(Storage_id));
  luaT_registeratname(L, parallel_(methods__), "parallel");
}

#endif
