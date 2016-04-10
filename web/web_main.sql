drop table if exists user;
create table user (
	user_id integer primary key autoincrement,
	username text not null,
	email text not null,
	pw_hash text not null,
	reg_time integer
);

drop table if exists code;
create table code (
	code_id integer primary key autoincrement,
	code_text text not null,
	code_time integer not null,
	data_input	text,
	author_id integer references user(user_id) on update cascade,
	ce_result text,
	re_result text,
	ok_result text,
	is_saved bool
);

drop table if exists saved_code;
create table saved_code (
	saved_code_id integer primary key autoincrement,
	code_id integer references code(code_id) on update cascade,
	author_id integer references user(user_id) on update cascade,
	code_name text not null,
	saved_time integer not null,
	info text
);
