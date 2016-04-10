
/*
* 最开始的状态，能正确执行，没有数据库
*/
#ifndef WEB_BACKFRONT
#define	WEB_BACKFRONT

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/ptrace.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/reg.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <signal.h>


#define	COMPILED_FILE	"compiled_file"
#define GCC_PATH	"/usr/bin/gcc"

#define	ONE_MB	(1<<20)
#define	USER_ID	1000

//limits
#define	COMPILE_CPU_LMT	60
#define COMPILED_FS_LMT	(100*ONE_MB)

#define RUN_FS_LMT	(100*ONE_MB)
#define	RUN_CPU_LMT		60
#define STACK_LMT	(8*ONE_MB)
#define AS_LMT	(128*ONE_MB)
#define	NPROC_LMT	1	

enum { AC, WA, TL, OL, RE, CE, ML};
extern int call_counter[512];
extern const char* const syscall_list[];
extern const int syscall_limit[];
#endif
