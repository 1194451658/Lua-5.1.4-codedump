/*
** $Id: lzio.c,v 1.31.1.1 2007/12/27 13:02:25 roberto Exp $
** a generic input stream interface
** See Copyright Notice in lua.h
*/


#include <string.h>

#define lzio_c
#define LUA_CORE

#include "lua.h"

#include "llimits.h"
#include "lmem.h"
#include "lstate.h"
#include "lzio.h"


// 调用ZIO中的lua_Reader，读取一段数据，缓存到buffer p
// 还是用lua_unlock/lua_lock，来处理多线程并发等问题
// 并返回，读取的第一个字节
// Q:为什么要返回第一字节？
//  * 需要看后面的使用
//  * 是为了预览！判断是否读到了文件末尾！
//  * 卧槽！能额外的返回给true/false不行吗？
int luaZ_fill (ZIO *z) {
  size_t size;

  // 获取lua_State
  // 只是为了加锁
  lua_State *L = z->L;
  const char *buff;
  
  // Q: 为什么是unlock?
  lua_unlock(L);

    // lua_Reader函数
    // 读取size个数据的大小
    // Q: 这里是不用自己制定size，而只是返回size
  buff = z->reader(L, z->data, &size);
  lua_lock(L);

  if (buff == NULL || size == 0) 
    return EOZ;

    // size-1，
    // 是因为后面，返回了一个字节
  z->n = size - 1;
  z->p = buff;

  // Q: 返回读取的第一个字节？
  return char2int(*(z->p++));
}

// 向前预读一个数据
// 缓存中没有内容的时候，调用luaZ_fill读取并缓存一段数据
// 返回：
//  * peek缓冲区的第一个字符; 末尾的时候返回EOZ
int luaZ_lookahead (ZIO *z) {
  if (z->n == 0) {
	// 如果当前缓冲区没有元素可读,则继续从输入中读取数据
    if (luaZ_fill(z) == EOZ)
      // 如果没有数据可读了
      return EOZ;
    else {
      // 否则,读到了数据,则将指针和计数器加1
      z->n++;  /* luaZ_fill removed first byte; put back it */
      z->p--;
    }
  }
  // OK了,将它作为字符返回
  return char2int(*z->p);
}

// 初始化ZIO
// 基本上是赋值L, reader, data
// n =0, p = null;
void luaZ_init (lua_State *L, ZIO *z, lua_Reader reader, void *data) {
  z->L = L;
  z->reader = reader;
  z->data = data;
  z->n = 0;
  z->p = NULL;
}


/* --------------------------------------------------------------- read --- */

// 从ZIO中，读取n个字符，到b
// b指针，会随着读取，跟着推进
// 返回：还有多少字符，没有读取
size_t luaZ_read (ZIO *z, void *b, size_t n) {
  while (n) {
    size_t m;

    // 读取和缓存数据
    // 并判断，是否达到文件末尾
    if (luaZ_lookahead(z) == EOZ)
      return n;  /* return number of missing bytes */

    // m实际要读取多少个
    // n <= 已经缓存的个数，读取n
    // n > 缓存的个数，（缓存不够），只读取缓存的内容
    m = (n <= z->n) ? n : z->n;  /* min. between n and z->n */
    memcpy(b, z->p, m);

    // 缓存中减去，被读取走的个数
    z->n -= m;
    z->p += m;

    // 移动b指针，到新位置
    b = (char *)b + m;
    n -= m;
  }
  return 0;
}

/* ------------------------------------------------------------------------ */

// 打开Mbuffer的buffer
// 确保空间>=n
// 最小空间LUA_MINBUFFER
char *luaZ_openspace (lua_State *L, Mbuffer *buff, size_t n) {
  if (n > buff->buffsize) {

  // LUA_MINBUFFER: string buffer的最小容量32
    if (n < LUA_MINBUFFER) n = LUA_MINBUFFER;
    luaZ_resizebuffer(L, buff, n);
  }
  return buff->buffer;
}


