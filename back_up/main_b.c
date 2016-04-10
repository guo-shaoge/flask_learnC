/*
* 最开始的状态，能正确执行，没有数据库
*/
#include "web_backfront.h"

char *work_dir = "/home/shaughn/707/bishe/";

void error(const char *msg) {
	perror(msg);
	exit(1);
}

int compile(const char *src_name) {
	pid_t pid;
	int status;
	struct rlimit lmt;
	
	/*some limits
	* cpu time limit: 60sec
	* file size limit: 100 mb
	*/
	lmt.rlim_cur = COMPILE_CPU_LMT;
	lmt.rlim_max = COMPILE_CPU_LMT;
	setrlimit(RLIMIT_CPU, &lmt);

	lmt.rlim_cur = COMPILED_FS_LMT;
	lmt.rlim_max = COMPILED_FS_LMT;
	setrlimit(RLIMIT_FSIZE, &lmt);

	pid = fork();
	if (pid < 0) {
		//error 
		error("fork error");
	} else if (pid == 0) {
		//因为chroot需要root权限，所以到现在都是root
		//这里必须切换
		while (setresuid(USER_ID, USER_ID, USER_ID) < 0);
		if (execl(GCC_PATH, "gcc","-static", "-o", COMPILED_FILE, src_name , (char*)0) < 0) {
			return -1;
		}
	} else {
		waitpid(pid, &status, 0);
		return status;
	}
}

int run_solution(const char *work_dir) {
	struct rlimit lmt;
	char path[1024] = {0};

	//sleep(2);
	nice(19);
	if (chdir(work_dir) < 0) {
		error("chdir error");
	}
	//freopen("data.in", "r", stdin);
	//freopen("data.out", "w", stdout);
	//freopen("err.out", "w", stderr);

	ptrace(PTRACE_TRACEME, 0, NULL, NULL);

	if (chroot(work_dir) < 0) {
		error("chdir error");
	}
	//因为chroot需要root权限，所以到现在都是roo
	//这里必须切换
	while (setresuid(USER_ID, USER_ID, USER_ID) < 0);

	//cpu time: 60sec
	lmt.rlim_cur = RUN_CPU_LMT;
	lmt.rlim_max = RUN_CPU_LMT;
	setrlimit(RLIMIT_CPU, &lmt);

	//the maximum number of processs
	lmt.rlim_cur = NPROC_LMT;
	lmt.rlim_max = NPROC_LMT;
	setrlimit(RLIMIT_NPROC, &lmt);

	//file size
	lmt.rlim_cur = RUN_FS_LMT;
	lmt.rlim_max = RUN_FS_LMT;
	setrlimit(RLIMIT_FSIZE, &lmt);

	//stack size
	lmt.rlim_cur = STACK_LMT;
	lmt.rlim_cur = STACK_LMT;
	setrlimit(RLIMIT_STACK, &lmt);

	//address space(virtual mem)
	lmt.rlim_cur = AS_LMT;
	lmt.rlim_max = AS_LMT;
	setrlimit(RLIMIT_AS, &lmt);

	//may never return
	char pathname[512];
	strcpy(pathname, "/");
	strcat(pathname, COMPILED_FILE);
	if(execl(pathname, COMPILED_FILE, (char*)0) < 0) {
		perror("in run_solution, execl error");
	}
	return -1;
}

int get_file_size(const char *pathname) {
	struct stat buf;

	if (stat(pathname, &buf) < 0) {
		return 0;
	}
	return buf.st_size;
}

int get_proc_status(pid_t pid, const char *id) {
	char buf[512];
	FILE *fp;
	int n, ret;

	sprintf(buf, "/proc/%d/status", pid);
	if ((fp = fopen(buf, "r")) == NULL) {
		return -1;
	}
	n = strlen(id);	
	ret = 0;
	while (fgets(buf, 512, fp) != NULL) {
		if (strncmp(id, buf, n) == 0) {
			sscanf((char*)buf+n+1, "%d", &ret);
		}
	}
	return ret;
}

int watch_solution(pid_t pid, int *flag) {
	struct rusage	usage;
	struct user_regs_struct regs;
	long orig_eax, eax;
	int status, vm, signo, insyscall;

	insyscall = 1;
	while (1) {
		wait4(-1, &status, 0, &usage);

		vm = get_proc_status(pid, "VmPeak");
		if (vm > AS_LMT) {
			*flag = ML;
			ptrace(PTRACE_KILL, pid, NULL, NULL);
			break;
		}
		if (WIFEXITED(status)) {
			break;
		}
		if (get_file_size("error.out")) {
			*flag = RE;
			ptrace(PTRACE_KILL, pid, NULL, NULL);
		}
		if (WIFSIGNALED(status)) {
			signo = WTERMSIG(status);
			switch (signo) {
				case SIGCHLD:
				case SIGALRM:
				case SIGKILL:
				case SIGXCPU:
					*flag = TL;
					break;
				case SIGXFSZ:
					*flag = OL;
					break;
				default:
					*flag = RE;
					break;
			}
			ptrace(PTRACE_KILL, pid, NULL, NULL);
			break;
		}
		
		orig_eax = ptrace(PTRACE_PEEKUSER, pid, 4*ORIG_EAX, NULL);
		if (insyscall && orig_eax>0) {
			if (call_counter[orig_eax] > syscall_limit[orig_eax]) {
				*flag = RE;
				fprintf(stderr, "process %d is calling %s which is not allowed\n", pid,\
					syscall_list[orig_eax]);
				ptrace(PTRACE_KILL, pid, NULL, NULL);
				break;
			} else {
				printf("process %d is calling %s\n", pid, syscall_list[orig_eax]);
			}
		}
		insyscall = 1-insyscall;
		ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
	}//while
	return 0;
}

int main(int argc, char *argv[]) {
	int compiled_ok, status;
	pid_t pid;
	int flag;

	compiled_ok = compile("./tmp.c");
	if (compiled_ok != 0) {
		fprintf(stderr, "compile error\n");
		exit(1);
	} else {
		printf("compile ok: %d\n", compiled_ok);
	}

	flag = AC;
	pid = fork();
	if (pid < 0) {
		perror("fork error");
		exit(1);
	} else if (pid == 0) {
		run_solution(work_dir);
	} else {
		watch_solution(pid, &flag);
		if (flag == AC) {
			printf("ac\n");
		} else if (flag == WA) {
			printf("wa\n");
		} else if (flag == TL) {
			printf("tl\n");
		} else if (flag == OL) {
			printf("ol\n");
		} else if (flag == RE) {
			printf("re\n");
		} else if (flag == CE) {
			printf("ce\n");
		} else if (flag == ML) {
			printf("ml\n");
		}
		return 0;
	}
}
