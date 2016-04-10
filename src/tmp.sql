create table user (
	user_id integer primary key autoincrement,
	username text not null,
	email text not null,
	pw_hash text not null
);

create table code (
	code_id	integer primary key autoincrement,
	code_text text not null,
	code_time text not null,
	author_id references user(user_id) on update cascade,
	ok_result text,
	re_result text,
	ce_result text
);
