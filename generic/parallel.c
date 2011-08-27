#ifndef TH_GENERIC_FILE
#define TH_GENERIC_FILE "generic/parallel.c"
#else

#ifndef _PARALLEL_GLOBALS_
#define _PARALLEL_GLOBALS_
static key_t shmem_key[MAX_NB_PROCESSES*2];
static int shmem_id[MAX_NB_PROCESSES*2];
static void *shmem_data[MAX_NB_PROCESSES*2];
static int shmem_size[MAX_NB_PROCESSES*2];

enum {Char, Byte, Short, Int, Long, Float, Double};

#define getbuffer(pid) (parallel_(Buffer) *)shmem_data[(pid)]
#define wrbuffer(pid) (parallel_(Buffer) *)shmem_data[(pid)*2]
#define rdbuffer(pid) (parallel_(Buffer) *)shmem_data[(pid)*2+1]
#define wrsize(pid) shmem_size[(pid)*2]
#define rdsize(pid) shmem_size[(pid)*2+1]

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
      lua_pushnil(L);
      return 1;
    }

    // register available size (excluding struct header)
    shmem_size[pid*2+i] = requested_size - sizeof(parallel_(Buffer));

    // and link data to the segment
    shmem_data[pid*2+i] = shmat(shmem_id[pid*2+i], (void *)0, 0);

    // and initialize it
    parallel_(Buffer) *buf = getbuffer(pid*2+i);
    buf->beingread = 0;
    buf->valid = 0;
    buf->type = Real;
    buf->size = 0;
  }

  // success
  lua_pushboolean(L, 1);
  return 1;
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

  // success
  lua_pushboolean(L, 1);
  return 1;
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
  // get args
  THStorage *storage = luaT_checkudata(L, 1, torch_(Storage_id));
  int pid = lua_tonumber(L, 2);

  // get handle on write buffer
  parallel_(Buffer) *buf = wrbuffer(pid);
  int bufsize = wrsize(pid) / (int)sizeof(real);

  // wait for write buffer to be available
  // this implies two things: it is not being read, and the data
  // in it is not valid anymore (i.e. it has been read already)
  while (buf->beingread) { parallel_wait(L); }
  while (buf->valid) { parallel_wait(L); }

  // set the size and type of the data about to be written
  buf->size = storage->size;
  buf->type = Real;

  // if the data to be sent fits in the shared mem, it is
  // sent in one shot, else, it is multiplexed
  if (storage->size < bufsize) {

    // transfer data in a single shot, as it fits in shared buffer
    memcpy(buf->data, storage->data, storage->size * sizeof(real));

    // data is now valid (to be read)
    buf->valid = 1;

  } else {

    // transfer data in multiple shots
    real *datap = storage->data;
    int remaining = storage->size;
    int subsize;
    while (remaining) {
      // we repeat the procedure above, and wait for the data
      // to be fully read by the child before writing the next chunk
      while (buf->beingread) { parallel_wait(L); }
      while (buf->valid) { parallel_wait(L); }

      // for each sub chunk of data, make a transfer
      subsize = min(buf->size, remaining);
      remaining -= subsize;
      memcpy(buf->data, datap, subsize * sizeof(real));
      datap += subsize;

      // the chunk is now valid
      buf->valid = 1;
    }

  }

  // done
  return 0;
}

static int parallel_(receiveStorage)(lua_State *L) {
  // get args
  THStorage *storage = luaT_checkudata(L, 1, torch_(Storage_id));
  int pid = lua_tonumber(L, 2);

  // get handle on read buffer
  parallel_(Buffer) *buf = rdbuffer(pid);
  int bufsize = rdsize(pid) / (int)sizeof(real);

  // wait for data to become valid in buffer
  while (!buf->valid) { parallel_wait(L); }

  // as soon as it is valid, lock the buffer
  buf->beingread = 1;

  // the type of data being read should match the expected one
  if (buf->type != Real) {
    perror("<parallel> receiving data of incorrect type");
  }

  // resize destination storage
  THStorage_(resize)(storage, buf->size);

  // if the data being read fits in the shared mem, it was
  // sent in one shot, else, it is being multiplexed
  if (storage->size < bufsize) {

    // copy data from buffer
    memcpy(storage->data, buf->data, storage->size * sizeof(real));

    // data has now been fully read, and is thefore not valid
    // anymore
    buf->beingread = 0;
    buf->valid = 0;

  } else {

    // read data in multiple shots
    real *datap = storage->data;
    int remaining = storage->size;
    int subsize;
    while (remaining) {
      // repeat the lock/unlock procedure
      while (!buf->valid) { parallel_wait(L); }
      buf->beingread = 1;

      // for each sub chunk of data, make a transfer
      subsize = min(buf->size, remaining);
      remaining -= subsize;
      memcpy(storage->data, datap, subsize * sizeof(real));
      datap += subsize;      

      // done reading
      buf->beingread = 0;
      buf->valid = 0;
    }

  }

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
