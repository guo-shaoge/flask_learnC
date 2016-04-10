#include "web_backfront.h"

static int readn(int fd, char *buf, size_t len) {
	int nleft, n;
	char * ptr;

	ptr = buf;
	nleft = len;
	while (nleft > 0) {
		n = read(fd, ptr, len);
		if (n < 0) {
			if (errno == EINTR) continue;
			else return -1;
		} else if (n == 0) {
			return len;
		} else {
			nleft -= n;
			ptr = ptr+n;
		}
	}
}


/*
 * read a file, return the buf ptr
 * 由于这个函数是用于往sql语句添加的，所以需要将所有newline 换为空格
 */
char *readfile(const char *fn, size_t *fz) {
	int fd, n;
	struct stat statbuf;
	char *ret, *ptr, *nl;
	
	if (stat(fn, &statbuf) < 0) {
		if (fz != NULL)
			*fz = 0;
		return NULL;
	}
	if (fz != NULL)
		*fz = statbuf.st_size;
	fd = open(fn, O_RDONLY);
	if (fd < 0) {
		if (fz != NULL)
			*fz = 0;
		return NULL;
	}
	ret = calloc(statbuf.st_size+1, sizeof(char));
	if ((n = readn(fd, ret, statbuf.st_size)) < 0) {
		return NULL;
	}
	/*
	*  sprintf(sql, "update tbl_name set col_name='%s' where condition", \
	*   buf);
	* 上面的sql语句中， buf使用单引号包围，所以buf中不能有单引号
	*/
	char *del;
	while ((del = strchr(ret, '\'')) != NULL) {
		*del = ' ';
	}
	return ret;
}
