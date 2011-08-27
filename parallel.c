#include "TH.h"
#include "luaT.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define torch_(NAME) TH_CONCAT_3(torch_, Real, NAME)
#define torch_string_(NAME) TH_CONCAT_STRING_3(torch., Real, NAME)
#define parallel_(NAME) TH_CONCAT_3(parallel_, Real, NAME)

static const void* torch_CharStorage_id = NULL;
static const void* torch_ByteStorage_id = NULL;
static const void* torch_ShortStorage_id = NULL;
static const void* torch_IntStorage_id = NULL;
static const void* torch_LongStorage_id = NULL;
static const void* torch_FloatStorage_id = NULL;
static const void* torch_DoubleStorage_id = NULL;

#include "generic/parallel.c"
#include "THGenerateAllTypes.h"

DLL_EXPORT int luaopen_libparallel(lua_State *L)
{
  torch_CharStorage_id = luaT_checktypename2id(L, "torch.CharStorage");
  torch_ByteStorage_id = luaT_checktypename2id(L, "torch.ByteStorage");
  torch_ShortStorage_id = luaT_checktypename2id(L, "torch.ShortStorage");
  torch_IntStorage_id = luaT_checktypename2id(L, "torch.IntStorage");
  torch_LongStorage_id = luaT_checktypename2id(L, "torch.LongStorage");
  torch_FloatStorage_id = luaT_checktypename2id(L, "torch.FloatStorage");
  torch_DoubleStorage_id = luaT_checktypename2id(L, "torch.DoubleStorage");

  parallel_CharInit(L);
  parallel_ByteInit(L);
  parallel_ShortInit(L);
  parallel_IntInit(L);
  parallel_LongInit(L);
  parallel_FloatInit(L);
  parallel_DoubleInit(L);
  return 1;
}
