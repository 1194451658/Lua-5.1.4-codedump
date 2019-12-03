/*
** $Id: luac.c,v 1.54 2006/06/02 17:37:11 lhf Exp $
** Lua compiler (saves bytecodes to files; also list bytecodes)
** See Copyright Notice in lua.h
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define luac_c
#define LUA_CORE

#include "lua.h"
#include "lauxlib.h"

#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"

#define PROGNAME	"luac"		/* default program name */
#define	OUTPUT		PROGNAME ".out"	/* default output file */

static int listing=0;			/* list bytecodes? */
static int dumping=1;			/* dump bytecodes? */
static int stripping=0;			/* strip debug information? */
static char Output[]={ OUTPUT };	/* default output file name */
static const char* output=Output;	/* actual output file name */

// 命令的名称argv[0]
static const char* progname=PROGNAME;	/* actual program name */

static void fatal(const char* message)
{
 fprintf(stderr,"%s: %s\n",progname,message);
 exit(EXIT_FAILURE);
}

static void cannot(const char* what)
{
 fprintf(stderr,"%s: cannot %s %s: %s\n",progname,what,output,strerror(errno));
 exit(EXIT_FAILURE);
}

static void usage(const char* message)
{
    // 如果
 if (*message=='-')
  fprintf(stderr,"%s: unrecognized option " LUA_QS "\n",progname,message);
 else
  fprintf(stderr,"%s: %s\n",progname,message);
 fprintf(stderr,
 "usage: %s [options] [filenames].\n"
 "Available options are:\n"
 "  -        process stdin\n"
 "  -l       list\n"
 "  -o name  output to file " LUA_QL("name") " (default is \"%s\")\n"
 "  -p       parse only\n"
 "  -s       strip debug information\n"
 "  -v       show version information\n"
 "  --       stop handling options\n",
 progname,Output);
 exit(EXIT_FAILURE);
}

// 封装C语言，字符串比较
#define	IS(s)	(strcmp(argv[i],s)==0)

// Q: 怎么解析到，只有一个文件参数的？！
static int doargs(int argc, char* argv[])
{
 int i;
 int version=0;

 // argv[0]是命令名称
 if (argv[0]!=NULL && *argv[0]!=0) 
    progname=argv[0];

    // 遍历所有命令行参数
 for (i=1; i<argc; i++)
 {
    // - 是命令行参数的结束标志
  if (*argv[i]!='-')			/* end of options; keep it */
   break;

    // argv[i]字符串比较
  else if (IS("--"))			/* end of options; skip it */
  {
    // 直接++，
    // 退出参数解析了
    // i直接指向，下一个lua文件的参数
   ++i;
   // 如果解析过了-v选项
   if (version) 
    ++version;
   break;
  }

  // -，是标记参数结束
  else if (IS("-"))			/* end of options; use stdin */
   break;
  else if (IS("-l"))			/* list */
  // 解析到-l选项
   ++listing;
  else if (IS("-o"))			/* output file */
  {
    // 紧挨着的
    // 下一个参数
   output=argv[++i];

   // 如果参数为空字符串
   // 打印-o的提示
   if (output==NULL || *output==0) 

    // LUA_QL: 添加单引号包围
    usage(LUA_QL("-o") " needs argument");

    // 如果参数是-
    // 则是stdout
   if (IS("-")) 
    output=NULL;
  }
  else if (IS("-p"))			/* parse only */
   dumping=0;
  else if (IS("-s"))			/* strip debug information */
   stripping=1;
  else if (IS("-v"))			/* show version */
   ++version;
  else					/* unknown option */
   usage(argv[i]);
 }

 // i会等于argc???
 if (i==argc && (listing || !dumping))
 {
  dumping=0;
  argv[--i]=Output;
 }
 if (version)
 {
  printf("%s  %s\n",LUA_RELEASE,LUA_COPYRIGHT);

  // 如果只有version选项！则直接退出程序
  // 这里用++:
  //    * 是同时标记了，有此选项
  //    * 和此选项出现的个数
  if (version==argc-1) 
    exit(EXIT_SUCCESS);
 }
 return i;
}

#define toproto(L,i) (clvalue(L->top+(i))->l.p)

// Q: 这个是？
// n: 几个lua文件
static const Proto* combine(lua_State* L, int n)
{
 if (n==1)
  return toproto(L,-1);
 else
 {
  int i,pc;
  Proto* f=luaF_newproto(L);
  setptvalue2s(L,L->top,f); incr_top(L);
  f->source=luaS_newliteral(L,"=(" PROGNAME ")");
  f->maxstacksize=1;
  pc=2*n+1;
  f->code=luaM_newvector(L,pc,Instruction);
  f->sizecode=pc;
  f->p=luaM_newvector(L,n,Proto*);
  f->sizep=n;
  pc=0;
  for (i=0; i<n; i++)
  {
   f->p[i]=toproto(L,i-n-1);
   f->code[pc++]=CREATE_ABx(OP_CLOSURE,0,i);
   f->code[pc++]=CREATE_ABC(OP_CALL,0,1,1);
  }
  f->code[pc++]=CREATE_ABC(OP_RETURN,0,1,0);
  return f;
 }
}

static int writer(lua_State* L, const void* p, size_t size, void* u)
{
 UNUSED(L);
 return (fwrite(p,size,1,(FILE*)u)!=1) && (size!=0);
}

// 命令行参数
struct Smain {
 int argc;
 char** argv;
};

static int pmain(lua_State* L)
{
// 传入的参数
 struct Smain* s = (struct Smain*)lua_touserdata(L, 1);
 int argc=s->argc;
 char** argv=s->argv;

 const Proto* f;
 int i;

// lua_checkstack:
// Ensures that there are at least extra free stack slots in the stack.
// It returns false if it cannot grow the stack to that size.
// This function never shrinks the stack;
// if the stack is already larger than the new size,
// it is left unchanged.

// stack将装不下，传入的文件名
 if (!lua_checkstack(L,argc)) 
    fatal("too many input files");

 for (i=0; i<argc; i++)
 {
    // 检查是否是-
    // 默认stdin
  const char* filename=IS("-") ? NULL : argv[i];

  // 加载lua文件
  if (luaL_loadfile(L,filename)!=0) 
    fatal(lua_tostring(L,-1));
 }

 f=combine(L,argc);

    // 汇编语言显示bytecode
 if (listing) 
    luaU_print(f,listing>1);

    // 输出bytecode
 if (dumping)
 {
    // 打开输出文件
  FILE* D= (output==NULL) ? stdout : fopen(output,"wb");
  if (D==NULL) 
    cannot("open");

  lua_lock(L);
  luaU_dump(L,f,writer,D,stripping);
  lua_unlock(L);
  if (ferror(D)) cannot("write");
  if (fclose(D)) cannot("close");
 }
 return 0;
}


//
// luac文档：
// https://www.lua.org/manual/5.1/luac.html
//

int main(int argc, char* argv[])
{
 lua_State* L;
    // argc, argv
    // 命令行参数
 struct Smain s;

 // 处理了usage()中
 // 所描述的选项
 int i=doargs(argc,argv);

 // argc和argv变成
 // 剩余的，没有解析的参数
 // Q: 传给lua的参数？
 argc-=i; argv+=i;

 // Q: 最少的有个lua源文件？
 if (argc<=0) 
    usage("no input files given");

 L=lua_open();
 if (L==NULL) 
    fatal("not enough memory for state");
 s.argc=argc;
 s.argv=argv;

    // Calls the C function func in protected mode.
    // func starts with only one element in its stack,
    // a light userdata containing ud.
    // In case of errors,
    // lua_cpcall returns the same error codes as lua_pcall,
    // plus the error object on the top of the stack;
    // otherwise, it returns zero,
    // and does not change the stack.
    // All values returned by func are discarded.
 if (lua_cpcall(L, pmain, &s)!=0) 
    fatal(lua_tostring(L,-1));
 lua_close(L);
 return EXIT_SUCCESS;
}
