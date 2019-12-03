/*
** $Id: lmem.c,v 1.70.1.1 2007/12/27 13:02:25 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/


#include <stddef.h>

#define lmem_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



/*
** About the realloc function:
** void * frealloc (void *ud, void *ptr, size_t osize, size_t nsize);
** (`osize' is the old size, `nsize' is the new size)
**
** Lua ensures that (ptr == NULL) iff (osize == 0).
**
** * frealloc(ud, NULL, 0, x) creates a new block of size `x'
**
** * frealloc(ud, p, x, 0) frees the block `p'
** (in this specific case, frealloc must return NULL).
** particularly, frealloc(ud, NULL, 0, 0) does nothing
** (which is equivalent to free(NULL) in ANSI C)
**
** frealloc returns NULL if it cannot create or reallocate the area
** (any reallocation to an equal or smaller size cannot fail!)
*/



// newsize的最小值
#define MINSIZEARRAY	4

// 2倍扩容函数
// size: 当前内存个数
// size_elems: 单个的内存大小
// limit: 限制size，最大可以是多少
// errormsg: 超limit的报错信息
void *luaM_growaux_ (lua_State *L, void *block, int *size, size_t size_elems,
                     int limit, const char *errormsg) {
  void *newblock;
  int newsize;

  // 大于上限的1/2
  if (*size >= limit/2) {  /* cannot double it? */
    if (*size >= limit)  /* cannot grow even a little? */
      luaG_runerror(L, errormsg);

    // size >= limit/2
    // size < limit
    // 限制智能增大到上限limit
    newsize = limit;  /* still have at least one free place */
  }
  else {
    // 直接扩大2倍容量
    newsize = (*size)*2;

    // 限制newsize的最小值
    if (newsize < MINSIZEARRAY)
      newsize = MINSIZEARRAY;  /* minimum size */
  }
  newblock = luaM_reallocv(L, block, *size, newsize, size_elems);
  *size = newsize;  /* update only when everything else is OK */
  return newblock;
}


// 简单报错block too big
void *luaM_toobig (lua_State *L) {
  luaG_runerror(L, "memory allocation error: block too big");
  return NULL;  /* to avoid warnings */
}



/*
** generic allocation routine.
*/

// 封装了lua_newState传入进来的，
// 内存分配函数
// ud:存储在了lua_State中
void *luaM_realloc_ (lua_State *L, void *block, size_t osize, size_t nsize) {
    // G(L): 快速访问lua_State中的global_State
  global_State *g = G(L);

  // block不是空，osize不能是0
  // block是空，osize是0
  lua_assert((osize == 0) == (block == NULL));

    // 调用内存分配函数
    // 重新分配内存
  block = (*g->frealloc)(g->ud, block, osize, nsize);

    // 如果没有分配成功
    // 报错
  if (block == NULL && nsize > 0)
    luaD_throw(L, LUA_ERRMEM);

  lua_assert((nsize == 0) == (block == NULL));

  // 更新
  // 分配的内存总量
  g->totalbytes = (g->totalbytes - osize) + nsize;

  return block;
}

