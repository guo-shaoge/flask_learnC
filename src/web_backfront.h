#ifndef WEB_BACKFRONT
#define	WEB_BACKFRONT

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <fcntl.h>
#include <sys/user.h>
#include <sys/reg.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <signal.h>

#include <sqlite3.h>

//database
#define DB_PATH			"/home/shaughn/707/bishe/web_main.db"

//filename and pathname
#define APP_ROOT_DIR	"/home/shaughn/707/bishe"
#define GCC_PATH	"/usr/bin/gcc"
#define	COMPILED_FILE	"compiled_file" //编译后的可执行文件
#define	CE_OUT	"compiled_err.out"
#define	INT_RE_OUT "internal_rt_err.out"//存放main程序运行时错误
#define RE_OUT	"rt_err.out"//存放用户程序运行时错误，如time limited等
#define	LOG_OUT	"log.out"		//存放日志文件
#define APP_LOG_OUT	"log.app.out"
#define	DATA_OUT	"data.out"	//存放输入程序的正确的输出
#define USER_CODE	"user_code.c" //用来存放用户输入的源程序
#define CODE_DATA	"code_data" //用来存放用户程序以及各种响应输出文件的目录
#define	DATA_IN	"data.in"	//user input

#define	ONE_MB	(1<<20)
#define	USER_ID	1000
#define COMPILED_OK	0
#define RWRWRW (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
#define RWXRWXRWX (S_IRWXU | S_IRWXG | S_IRWXO)

//error level
#define INT_RE	0	//main程序出错，比如fork() < 0
#define RE 1	//运行客户提交的程序时发生错误，比如time limited
#define	LOG	2		
#define APP_LOG 3
#define CE	4

//strlen limits
#define WORK_DIR_LEN	512
#define	SQL_LEN			512
#define LONG_SQL_LEN	1<<19
#define MAXSIZE			1024
//runtime limits
#define	COMPILE_CPU_LMT	30
#define COMPILED_FS_LMT	(100*ONE_MB)

#define RUN_FS_LMT	4096
#define RUN_FN_LMT	1024	
#define	RUN_CPU_LMT	3
#define STACK_LMT	(8*ONE_MB)
#define AS_LMT	(128*ONE_MB)
#define	NPROC_LMT	1	

enum { CODE_AC, CODE_TL, CODE_RE, CODE_ML, CODE_OL};
extern int call_counter[512];
extern const char* const syscall_list[];
extern const int syscall_limit[];
extern char code_id[128];

void write_log(int level, int iserrno, const char *msg, ...);
char *readfile(const char *fn, size_t *fz);
#endif
