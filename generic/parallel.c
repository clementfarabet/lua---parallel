#ifndef TH_GENERIC_FILE
#define TH_GENERIC_FILE "generic/parallel.c"
#else

#ifndef _PARALLEL_GLOBALS_
#define _PARALLEL_GLOBALS_
static key_t shmem_key[MAX_NB_PROCESSES*2];
static int shmem_id[MAX_NB_PROCESSES*2];
static void *shmem_data[MAX_NB_PROCESSES*2];

enum {Char, Byte, Short, Int, Long, Float, Double};

#define getbuffer(pid) (parallel_(Buffer) *)shmem_data[(pid)];
#define wrbuffer(pid) (parallel_(Buffer) *)shmem_data[(pid)*2];
#define rdbuffer(pid) (parallel_(Buffer) *)shmem_data[(pid)*2+1];

#define parallel_wait(L)  if (getppid() == 1) { parallel_(disconnect)(L); }
#endif

typedef struct {
  char beingread;
  char valid;
  char type;
  long int size;
  real data[];
} parallel_(Buffer);

static int parallel_(create)(lua_State *L) {
  // args
  int requested_size = lua_tonumber(L, 1);
  int pid = lua_tonumber(L, 2);
  const char *paths[2];
  paths[0] = lua_tostring(L, 3);
  paths[1] = lua_tostring(L, 4);

  // initialize 2 shared buffers: one for RDs, one for WRs
  int i;
  for (i=0; i<2; i++) {
    // generate unique key
    if ((shmem_key[pid*2+i] = ftok(paths[i], 0)) == -1) {
      perror("<parallel> ftok couldnt get the shared mem descriptor");
      lua_pushnil(L);
      return 1;
    }

    // create shared buffer
    if((shmem_id[pid*2+i] = shmget(shmem_key[pid*2+i], requested_size, 0644 | IPC_CREAT)) == -1) {
      perror("<parallel> shmget couldnt sync the shared mem segment");
      lua_pushnil(L);
      return 1;
    }

    // and link data to the segment
    shmem_data[pid*2+i] = shmat(shmem_id[pid*2+i], (void *)0, 0);

    // and initialize it
    parallel_(Buffer) *buf = getbuffer(pid*2+i);
    buf->beingread = 0;
    buf->valid = 0;
    buf->type = Real;
    buf->size = 0;
  }

  // no arg returned
  return 0;
}

static int parallel_(connect)(lua_State *L) {
  // args
  int requested_size = lua_tonumber(L, 1);
  int pid = lua_tonumber(L, 2);
  const char *paths[2];
  paths[0] = lua_tostring(L, 3);
  paths[1] = lua_tostring(L, 4);

  // connect to 2 shared buffers: one for RDs, one for WRs
  int i;
  for (i=0; i<2; i++) {
    // generate unique key
    if ((shmem_key[pid*2+i] = ftok(paths[i], 0)) == -1) {
      perror("<parallel> ftok couldnt get the shared mem descriptor");
      lua_pushnil(L);
      return 1;
    }

    // create shared buffer
    if((shmem_id[pid*2+i] = shmget(shmem_key[pid*2+i], requested_size, 0644 | IPC_CREAT)) == -1) {
      perror("<parallel> shmget couldnt sync the shared mem segment");
      lua_pushnil(L);
      return 1;
    }

    // and link data to the segment
    shmem_data[pid*2+i] = shmat(shmem_id[pid*2+i], (void *)0, 0);
  }

  // no arg returned
  return 0;
}

static int parallel_(disconnect)(lua_State *L) {
  // args
  int pid = lua_tonumber(L, 2);

  // who to disconnect ?
  int i;
  if (pid == -1) {
    for (i=0; i<MAX_NB_PROCESSES*2; i++) {
      if (shmem_data[i]) {
        shmdt(shmem_data[i]);
        shmctl(shmem_id[i], IPC_RMID, NULL);
        shmem_data[i] = NULL;
      }
    }
  } else {
    for (i=0; i<2; i++) {
      if (shmem_data[pid*2+i]) {
        shmdt(shmem_data[pid*2+i]);
        shmctl(shmem_id[pid*2+i], IPC_RMID, NULL);
        shmem_data[pid*2+i] = NULL;
      }
    }
  }

  return 0;
}

static int parallel_(sendStorage)(lua_State *L) {
  THStorage *storage = luaT_checkudata(L, 1, torch_(Storage_id));
  int pid = lua_tonumber(L, 2);
  parallel_(Buffer) *buf = wrbuffer(pid);
  while (buf->beingread) { parallel_wait(L); }
  while (buf->valid) { parallel_wait(L); }
  buf->size = storage->size;
  buf->type = Real;
  memcpy(buf->data, storage->data, storage->size * sizeof(real));
  buf->valid = 1;
  return 0;
}

static int parallel_(receiveStorage)(lua_State *L) {
  THStorage *storage = luaT_checkudata(L, 1, torch_(Storage_id));
  int pid = lua_tonumber(L, 2);
  parallel_(Buffer) *buf = rdbuffer(pid);
  while (!buf->valid) { parallel_wait(L); }
  buf->beingread = 1;
  if (buf->type != Real) {
    perror("<parallel> receiving data of incorrect type");
  }
  THStorage_(resize)(storage, buf->size);
  memcpy(storage->data, buf->data, storage->size * sizeof(real));
  buf->beingread = 0;
  buf->valid = 0;
  return 0;
}

static const struct luaL_reg parallel_(methods__) [] = {
  {"create", parallel_(create)},
  {"connect", parallel_(connect)},
  {"disconnect", parallel_(disconnect)},
  {"sendStorage", parallel_(sendStorage)},
  {"receiveStorage", parallel_(receiveStorage)},
  {NULL, NULL}
};

static void parallel_(Init)(lua_State *L)
{
  // reg functions into lua space
  luaT_pushmetaclass(L, torch_(Storage_id));
  luaT_registeratname(L, parallel_(methods__), "parallel");

  // init shared mem tables
  int i;
  for (i=0; i<MAX_NB_PROCESSES*2; i++) {
    shmem_data[i] = NULL;
  }
}

#endif
