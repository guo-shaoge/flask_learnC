#include "web_backfront.h"

void update_ret_to_db();
char code_id[128];

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

int close_fd() {
	int f = -1, i;

	for (i = 100; i<500; i++) {
		if (close(i) == 0) 
			f = 0;
	}
	return f;
}


void write_file(const char *fn, const char *msg, const char *mode, size_t len) {
	int left, n;
	FILE *fp;

	again:
	fp = fopen(fn, mode);	
	if (fp == NULL) {
		if (errno == EMFILE) 
			//process has reached the maxinum num of the open files
			if (close_fd() == 0) goto again;
		perror("fopen error (write_file write_log)");
		fprintf(stderr, "%d\n", getpid());
		exit(-1);
	}
	fputs(msg, fp);
	fputs("\n", fp);
	fclose(fp);
}

void write_log(int level, int iserrno, const char *msg, ...) {
	va_list ap;
	char buf[LONG_SQL_LEN];
	va_start(ap, msg);
	vsnprintf((char*)buf, sizeof(buf), msg, ap);
	strcat((char*)buf, "\n");
	va_end(ap);

	if (level == RE) {
		write_file(RE_OUT, buf, "w", sizeof(buf));
		if (iserrno) {
			write_file(RE_OUT, strerror(errno), "w", sizeof(buf));
		}
	} else if (level == INT_RE) {
		write_file(INT_RE_OUT, buf, "a", sizeof(buf));
		if (iserrno) {
			write_file(INT_RE_OUT, strerror(errno), "a", sizeof(buf));
		}
		write_file(RE_OUT, "runtime error(server internal error)", "a", sizeof(buf));
		write_file(INT_RE_OUT, "##################", "a", sizeof(buf));
	} else if (level == CE) {
		write_file(CE_OUT, buf, "w", sizeof(buf));
		if (iserrno) {
			write_file(CE_OUT, strerror(errno), "w", sizeof(buf));
		}
	} else if (level == LOG) {
		write_file(LOG_OUT, buf, "a", sizeof(buf));
		if (iserrno) {
			write_file(LOG_OUT, strerror(errno), "a", sizeof(buf));
		}
	} else if (level == APP_LOG) {
		char temp[WORK_DIR_LEN];
		int f = 0;

		getcwd(temp, WORK_DIR_LEN);
		if (strcmp(APP_ROOT_DIR, temp) != 0) {
			f = 1;
			chdir(APP_ROOT_DIR);
		}
		write_file(APP_LOG_OUT, buf, "a", sizeof(buf));
		if (iserrno) {
			write_file(APP_LOG_OUT, strerror(errno), "a", sizeof(buf));
		}
		if (f) {
			chdir(temp);
		}
	}
	return ;
}

int Setrlimit(int resource, const struct rlimit *rlim) {
	int ret;

	if ((ret = setrlimit(resource, rlim)) < 0) {
		write_log(INT_RE, 1, "setrlimit error");
		exit(-1);
	}
	return ret;
}

int compile(const char *src_name) {
	pid_t pid;
	int status;
	struct rlimit lmt;
	
	pid = fork();
	if (pid < 0) {
		write_log(INT_RE ,1,  "fork error when compile file");
		return -1;
	} else if (pid == 0) {
		//因为chroot需要root权限，所以到现在都是root
		//这里须切换
		if (setreuid(USER_ID, USER_ID) < 0) {
			write_log(INT_RE, 1, "setreuid error(in compile)");
			exit(-1);
		}
		/*some limits
		* cpu time limit: 60sec
		* file size limit: 100 mb
		*/
		lmt.rlim_cur = COMPILE_CPU_LMT;
		lmt.rlim_max = COMPILE_CPU_LMT;
		Setrlimit(RLIMIT_CPU, &lmt);

		lmt.rlim_cur = COMPILED_FS_LMT;
		lmt.rlim_max = COMPILED_FS_LMT;
		Setrlimit(RLIMIT_FSIZE, &lmt);

		if (freopen(CE_OUT, "w",  stdout) == NULL) {
			write_log(INT_RE, 1, "freopen error (in compile)");
			printf("fope err");
		}
		if (freopen(CE_OUT, "w",  stderr) == NULL) {
			write_log(INT_RE, 1, "freopen error (in compile)");
		}

		if (execl(GCC_PATH, "gcc","-static", "-o", COMPILED_FILE, src_name , (char*)0) < 0) {
			write_log(INT_RE,1,  "execl error when compile file");
			exit(-1);
		}
	} else {
		waitpid(pid, &status, 0);
		return status;
	}
}

int run_solution(const char *work_dir) {

//fprintf(stderr, "%d\n", getpid());
	struct rlimit lmt;

	nice(19);
	if (chroot(work_dir) < 0) {
		write_log(INT_RE, 1, "chroot error");
		exit(-1);
	}
	if (chdir("/") < 0) {
		write_log(INT_RE, 1, "chdir error (in run_solution)");
		exit(-1);
	}

	//因为chroot需要root权限，所以到现在都是roo
	//这里必须切换
	if (setreuid(USER_ID, USER_ID) < 0) {
		write_log(INT_RE, 1, "setreuid error(in run_solution)");
		exit(-1);
	}
//getcwd(temp, 512);
//printf("%s\n", temp);
//fflush(stdout);

	ptrace(PTRACE_TRACEME, 0, NULL, NULL);

	//cpu time: 60sec
	lmt.rlim_cur = RUN_CPU_LMT;
	lmt.rlim_max = RUN_CPU_LMT;
	Setrlimit(RLIMIT_CPU, &lmt);

	//the maximum number of processs
	lmt.rlim_cur = NPROC_LMT;
	lmt.rlim_max = NPROC_LMT;
	Setrlimit(RLIMIT_NPROC, &lmt);

	//file size
	lmt.rlim_cur = RUN_FS_LMT;
	lmt.rlim_max = RUN_FS_LMT;
	Setrlimit(RLIMIT_FSIZE, &lmt);

	//maximum file descriptor number
	lmt.rlim_cur = RUN_FN_LMT;
	lmt.rlim_max = RUN_FN_LMT;
	Setrlimit(RLIMIT_NOFILE, &lmt);

	lmt.rlim_cur = STACK_LMT;
	lmt.rlim_max = STACK_LMT;
	Setrlimit(RLIMIT_STACK, &lmt);
	//address space(virtual mem)
	lmt.rlim_cur = AS_LMT;
	lmt.rlim_max = AS_LMT;
	Setrlimit(RLIMIT_AS, &lmt);

	freopen(DATA_IN, "r", stdin);
	freopen(DATA_OUT, "w", stdout);
	freopen(RE_OUT, "w", stderr);
	if (execl("/compiled_file", "compiled_file", (char *)0) < 0) {
		write_log(INT_RE, 1, "run_solution execl error");
		exit(-1);
	}
}

int get_file_size(const char *pathname) {
	struct stat buf;

	if (stat(pathname, &buf) < 0) {
		return 0;
	}
	return buf.st_size;
}


int watch_solution(pid_t pid, int *flag) {
	struct rusage	usage;
	struct user_regs_struct regs;
	long orig_eax, eax;
	int status, vm, signo, insyscall, ptraceflag;

	insyscall = 1;
	while (1) {
		pid_t pid = wait4(-1, &status, 0, &usage);
		ptraceflag = 0;

		vm = get_proc_status(pid, "VmPeak");
		if (vm > AS_LMT) {
			*flag = CODE_ML;
			ptrace(PTRACE_KILL, pid, NULL, NULL);
			break;
		}
		if (WIFEXITED(status)) {
			break;
		}
		if (get_file_size(RE_OUT)) {
			*flag = CODE_RE;
			ptrace(PTRACE_KILL, pid, NULL, NULL);
		}
		if (WIFSTOPPED(status)) {
			signo = WSTOPSIG(status);
			//psignal(signo, "in stopped");
			switch (signo) {
				case SIGCHLD:
				case SIGALRM:
				case SIGKILL:
				case SIGXCPU:
					*flag = CODE_TL;
					break;
				case SIGXFSZ:
					*flag = CODE_OL;
					break;
				case SIGTRAP:
					ptraceflag = 1;
					break;
				default:
					*flag = CODE_RE;
					break;
			}
			if (ptraceflag == 0) {
				ptrace(PTRACE_KILL, pid, NULL, NULL);
				break;
			}
		}

		if (ptraceflag == 0 && WIFSIGNALED(status)) {
			signo = WTERMSIG(status);
			//psignal(signo, "in signaled");
			switch (signo) {
				case SIGCHLD:
				case SIGALRM:
				case SIGKILL:
				case SIGXCPU:
					*flag = CODE_TL;
					break;
				case SIGXFSZ:
					*flag = CODE_OL;
					break;
				default:
					*flag = CODE_RE;
					break;
			}
			ptrace(PTRACE_KILL, pid, NULL, NULL);
			break;
		}
		
		orig_eax = ptrace(PTRACE_PEEKUSER, pid, 4*ORIG_EAX, NULL);
		if (insyscall && orig_eax>0) {
			if (call_counter[orig_eax] > syscall_limit[orig_eax]) {
				*flag = CODE_RE;
				write_log(LOG, 0, "process %d is calling %s which is not allowed", pid, syscall_list[orig_eax]);
                // debug
                //fprintf(stderr, "calling %s(%d), which is not allowed\n", syscall_list[orig_eax], call_counter[orig_eax]);
				write_log(LOG, 0, "process %d is calling %s", pid, syscall_list[orig_eax]);
				ptrace(PTRACE_KILL, pid, NULL, NULL);
				break;
			} else {
				//write_log(LOG, 0, "process %d is calling %s\n", pid, syscall_list[orig_eax]);
                // debug
                //fprintf(stderr, "calling %s(%d)\n", syscall_list[orig_eax], call_counter[orig_eax]);
			}
		}
		insyscall = 1-insyscall;
		ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
	}//while
	return 0;
}

void make_work_dir(char *user_name, char *work_dir) {
	memset(work_dir, 0,	WORK_DIR_LEN); 

	strcpy(work_dir, APP_ROOT_DIR);
	strcat(work_dir, "/");
	strcat(work_dir, CODE_DATA);
	strcat(work_dir, "/");
	strcat(work_dir, user_name);
	strcat(work_dir, "/");
	if (mkdir(work_dir, RWXRWXRWX) < 0) {
		if (errno != EEXIST) {
			write_log(APP_LOG, 0, "mkdir error(in make_work_dir)");
			exit(-1);
		}
	}
}


void remove_file(const char *fn, int log) {
	if (remove(fn) < 0) {
		if (log)
			write_log(APP_LOG, 1, "remove %s error(in remove_file)", fn);
	}
}

void remove_files() {
	remove_file(DATA_IN, 1);
	remove_file(DATA_OUT, 1);
	remove_file(RE_OUT, 1);
	remove_file(CE_OUT, 1);
	remove_file(USER_CODE, 1);
	remove_file(COMPILED_FILE, 1);
	return;
}

void create_file(const char *fn) {
	if (creat(fn, RWRWRW) < 0) {
		write_log(APP_LOG, 1, "init create files error");
	}
}

void init_create_files() {
	create_file(INT_RE_OUT);
	create_file(LOG_OUT);
}

void init_remove_files() {
	remove_file(DATA_IN, 0);
	remove_file(DATA_OUT, 0);
	remove_file(RE_OUT, 0);
	remove_file(CE_OUT, 0);
	remove_file(USER_CODE, 0);
	remove_file(COMPILED_FILE, 0);
	return;
}

int main(int argc, char *argv[]) {
	int ret, status;
	pid_t pid;
	int flag;
	char user_name[128];
	char work_dir[WORK_DIR_LEN];

	if (argc != 3) {
		write_log(APP_LOG, 0, "usgae: ./main username code_id");
		exit(-1);
	}

	if (atexit(remove_files) != 0) {
		write_log(APP_LOG, 1, "atexit error");
		exit(-1);
	}

	if (atexit(update_ret_to_db) != 0) {
		write_log(APP_LOG, 1, "atexit error");
		exit(-1);
	}
	umask(0);
	strcpy(user_name, argv[1]);
	strcpy(code_id, argv[2]);
	make_work_dir(user_name, work_dir);
	if (chdir(work_dir) < 0) {
		write_log(APP_LOG, 0, "chdir to wrok_dir error");
		exit(-1);
	}
	init_remove_files();
	init_create_files();
	//sleep(5);
	get_code_text(code_id);
	get_data_input(code_id);
#ifdef	DEBUG
	//FILE *fp = fopen(DATA_IN, "w");
	//fputs("1", fp);
	//fclose(fp);
#endif

	ret = compile(USER_CODE);
	if (ret != COMPILED_OK) {
		write_log(LOG, 0, "compile error, see the ce.out");
		exit(-1);
	} else {
		write_log(LOG, 0, "compile ok, now start run");
		if (chown("./compiled_file", USER_ID, USER_ID) < 0) {
			write_log(INT_RE, 1, "chown compiled_file error");
			exit(-1);
		}
	}

	pid = fork();
	if (pid < 0) {
		write_log(INT_RE, 1, "fork error (in main)");
		exit(-1);
	} else if (pid == 0) {
		run_solution(work_dir);
	} else {
		//parent
		watch_solution(pid, &flag);
		if (flag == CODE_TL) {
			write_log(RE, 0, "time exceed");
		} else if (flag == CODE_ML) {
			write_log(RE, 0, "mem exceed");
		} else if (flag == CODE_OL) {
			write_log(RE, 0, "output exceed");
		} else if (flag == CODE_RE) {
			write_log(RE, 0, "runtime error");
		}
		return 0;
	}
}
