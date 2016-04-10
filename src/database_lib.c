#include "web_backfront.h"

sqlite3 *get_db() {
	sqlite3 *db;
	int ret;

	ret = sqlite3_open(DB_PATH, &db);
	if (ret != SQLITE_OK) { 
		//open db failed
		//write log and exit
		write_log(INT_RE, 0, "open database error %s", sqlite3_errmsg(db));
		exit(-1);
	}
	return db;
}

void database_error(char *errmsg, const char *sql) {
	char temp[MAXSIZE];

	strcpy(temp, errmsg);
	sqlite3_free(errmsg);
	write_log(INT_RE ,0,  "sqlite3_exec error, sql: %s, %s", sql, errmsg);
	sqlite3_free(errmsg);
}
/*
*	从数据库中读取用户提交的程序，并读入USER_CODE中
*/
static int 
get_code_text_cb(void *arg, int count, char **col_content, char **col_name) {
	FILE *fp = arg;

	fputs(col_content[0], fp);
	return 0;
}

void get_code_text(char *code_id) {
	char sql[SQL_LEN] = {0};
	char *errmsg;
	int ret;
	sqlite3 *db;

	FILE *fp = fopen(USER_CODE, "w");
	if (fp == NULL) {
		write_log(INT_RE, 1, "fopen error(in get_code_text)");
		exit(-1);
	}

	sprintf(sql, "select code_text from code where code_id='%s'", code_id);
	db = get_db();
	ret = sqlite3_exec(db, (const char*)sql, get_code_text_cb,\
		(void*)fp, &errmsg);
	fclose(fp);
	if (ret != SQLITE_OK) {
		database_error(errmsg, sql);
	}
}

/*
* 从数据库中读取用户的输入（数据库表字段为data_input），放入DATA_IN文件中
*/
static int 
get_data_input_cb(void *arg, int count, char **col_content, char **col_name) {
	FILE *fp = arg;

	fputs(col_content[0], fp);
	return 0;
}


/*
* 从DATA_IN中读取用户的输入，比如scanf 需要的输入就从这里得到
*/
void get_data_input(char *code_id) {
	char sql[SQL_LEN] = {0};
	char *errmsg;
	int ret;
	sqlite3 *db;

	FILE *fp = fopen(DATA_IN, "w");
	if (fp == NULL) {
		write_log(INT_RE, 1, "fopen error(in get_data_input)");
		exit(-1);
	}

	sprintf(sql, "select data_input from code where code_id='%s'", code_id);
	db = get_db();
	ret = sqlite3_exec(db, (const char*)sql, get_data_input_cb,\
		(void*)fp, &errmsg);
	fclose(fp);
	if (ret != SQLITE_OK) {
		database_error(errmsg, sql);
	}
}

void update_db(const char *sql) {
	sqlite3 *db;
	int ret;
	char *errmsg;

	db = get_db();
	ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		database_error(errmsg, sql);
	}
}


/*
*	用于将编译运行后的结果反馈到数据库中，注意从相应文件中读取反馈时，
*   文件内容中不能有单引号
*/
void update_ret_to_db() {
	char *buf;
	char sql[LONG_SQL_LEN] = {0};
	int ret, fz;

	buf = readfile(CE_OUT, &fz);
	if (fz) {
		snprintf(sql,LONG_SQL_LEN, "update code set ce_result='%s' \
			where code_id='%s'", buf, code_id);
		free(buf);
		update_db(sql);
	} else {
		snprintf(sql,LONG_SQL_LEN, "update code set ce_result='%s' \
			where code_id='%s'", "", code_id);
		update_db(sql);
	}

	buf = readfile(RE_OUT, &fz);
	if (fz) {
		snprintf(sql,LONG_SQL_LEN, "update code set re_result='%s' \
			where code_id='%s'", buf, code_id);
		free(buf);
		update_db(sql);
	} else {
		snprintf(sql,LONG_SQL_LEN, "update code set re_result='%s' \
			where code_id='%s'", "", code_id);
		update_db(sql);
	}

	buf = readfile(DATA_OUT, &fz);
	if (fz) {
		snprintf(sql,LONG_SQL_LEN, "update code set ok_result='%s' \
			where code_id='%s'", buf, code_id);
		free(buf);
		update_db(sql);
	} else { 
		snprintf(sql,LONG_SQL_LEN, "update code set ok_result='%s' \
			where code_id='%s'", "", code_id);
		update_db(sql);
	}
}
