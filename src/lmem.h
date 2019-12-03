/*
** $Id: lmem.h,v 1.31.1.1 2007/12/27 13:02:25 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "lua.h"

// Memeory Error Message
#define MEMERRMSG	"not enough memory"


// L: lua_State* L
// b: void* block
// on: 旧的个数
// n:  新的个数
// e: 一个元素的大小
#define luaM_reallocv(L,b,on,n,e) \
    // MAX_SIZET/(e): 最大个数
    // 检查n是否超过最大个数
	((cast(size_t, (n)+1) <= MAX_SIZET/(e)) ?  /* +1 to avoid warnings */ \
        // 重新调配内存
		luaM_realloc_(L, (b), (on)*(e), (n)*(e)) : \
		luaM_toobig(L))

//  ----------------------------------
//          释放内存函数
//  ----------------------------------

// 释放内存
// b: Type* b
// s: b的大小
#define luaM_freemem(L, b, s)	luaM_realloc_(L, (b), (s), 0)

// 释放内存
// b: Type* b
#define luaM_free(L, b)		luaM_realloc_(L, (b), sizeof(*(b)), 0)

// 释放内存
#define luaM_freearray(L, b, n, t)   luaM_reallocv(L, (b), n, 0, sizeof(t))

//  ----------------------------------
//          分配内存函数
//  ----------------------------------

// 分配内存函数
// t: 分配多大
#define luaM_malloc(L,t)	luaM_realloc_(L, NULL, 0, (t))

// 分配内存函数
// t: 类型
#define luaM_new(L,t)		cast(t *, luaM_malloc(L, sizeof(t)))

// 分配内存
// n: t的个数
// t: 类型
#define luaM_newvector(L,n,t) \
		cast(t *, luaM_reallocv(L, NULL, 0, n, sizeof(t)))

// 数组个数满的时候，立刻2倍扩容
// v: void* block
// nelems: 当前v中的元素个数
// size: 当期v最大空间元素个数
// t: 数组内，元素类型
// limit: 个数限制
// e: 错误消息
#define luaM_growvector(L,v,nelems,size,t,limit,e) \
          if ((nelems)+1 > (size)) \
            ((v)=cast(t *, luaM_growaux_(L,v,&(size),sizeof(t),limit,e)))

// v: 数组
// oldn: 旧的元素个数
// n: 新的元素个数
// t: 数组内元素类型
#define luaM_reallocvector(L, v,oldn,n,t) \
   ((v)=cast(t *, luaM_reallocv(L, v, oldn, n, sizeof(t))))


// 封装了lua_newState传入进来的，
// 内存分配函数
// ud:存储在了lua_State中
LUAI_FUNC void *luaM_realloc_ (lua_State *L, void *block, size_t oldsize,
                                                          size_t size);
// 简单报错block too big
LUAI_FUNC void *luaM_toobig (lua_State *L);

// 2倍扩容函数
// size: 当前内存个数
// size_elems: 单个的内存大小
// limit: 限制size，最大可以是多少
// errormsg: 超limit的报错信息
LUAI_FUNC void *luaM_growaux_ (lua_State *L, void *block, int *size,
                               size_t size_elem, int limit,
                               const char *errormsg);

#endif

