/*
** $Id: lzio.h,v 1.21.1.1 2007/12/27 13:02:25 roberto Exp $
** Buffered streams
** See Copyright Notice in lua.h
*/


#ifndef lzio_h
#define lzio_h

#include "lua.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define char2int(c)	cast(int, cast(unsigned char, (c)))

#define zgetc(z)  (((z)->n--)>0 ?  char2int(*(z)->p++) : luaZ_fill(z))

typedef struct Mbuffer {
  char *buffer;

  // Q: bufflen??
  // reset的时候，会变0
  size_t n;

  // 指定buffer的大小
  size_t buffsize;
} Mbuffer;

// 初始化Mbuffer函数
// buff: Mbuffer类型
#define luaZ_initbuffer(L, buff) 
    ((buff)->buffer = NULL, (buff)->buffsize = 0)

// 快速获取Mbuffer成员
#define luaZ_buffer(buff)	((buff)->buffer)
#define luaZ_sizebuffer(buff)	((buff)->buffsize)
#define luaZ_bufflen(buff)	((buff)->n)

// 重置函数
#define luaZ_resetbuffer(buff) ((buff)->n = 0)

// 重新分配buffer的大小
// 变成size大
// buffer: Mbuffer
#define luaZ_resizebuffer(L, buff, size) \
	(luaM_reallocvector(L, (buff)->buffer, (buff)->buffsize, size, char), \
	(buff)->buffsize = size)

// 释放buffer
#define luaZ_freebuffer(L, buff)	luaZ_resizebuffer(L, buff, 0)


// 打开Mbuffer的buffer
// 确保空间>=n
// 最小空间LUA_MINBUFFER
LUAI_FUNC char *luaZ_openspace (lua_State *L, Mbuffer *buff, size_t n);




// 初始化ZIO
// 基本上是赋值L, reader, data
// n =0, 
// p = null;
LUAI_FUNC void luaZ_init (lua_State *L, ZIO *z, lua_Reader reader,
                                        void *data);
LUAI_FUNC size_t luaZ_read (ZIO* z, void* b, size_t n);	/* read next n bytes */
LUAI_FUNC int luaZ_lookahead (ZIO *z);



/* --------- Private Part ------------------ */

struct Zio {
    // 当前data中的数据大小
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  lua_Reader reader;

  // 最近使用lua_Reader读取缓存下来的数据
  void* data;			/* additional data */
  lua_State *L;			/* Lua state (for reader) */
};


LUAI_FUNC int luaZ_fill (ZIO *z);

#endif
