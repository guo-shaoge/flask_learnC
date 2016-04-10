# coding: utf-8

from flask import render_template, Flask, request, redirect, url_for, \
	config, _app_ctx_stack, current_app, flash, g, session, jsonify
import sqlite3
import time
import sys
import subprocess
from werkzeug.security import check_password_hash, generate_password_hash

#config
DATABASE_PATH='/home/shaughn/707/bishe/web_main.db'
DATABASE_SCRIPT='web_main.sql'
SECRET_KEY='test secret key'
MAIN_PATH='/home/shaughn/707/bishe/main'

app = Flask(__name__)
app.config.from_object(__name__)

def get_db():
	"""return the database connection."""
	top_ctx = _app_ctx_stack.top
	if not hasattr(top_ctx, 'sqlite_db'):
		top_ctx.sqlite_db = sqlite3.connect(app.config['DATABASE_PATH'])
	return top_ctx.sqlite_db

def init_db():
	"""initialize the database from the script."""
	db = get_db()
	if db:
		with open(app.config['DATABASE_SCRIPT'], 'r') as f:
			db.executescript(f.read())
		db.commit()
		db.close()

@app.cli.command('init_db')
def initdb_command():
	"""initialize the db from command line."""
	init_db()
	print 'Initialized the database successfully'


@app.before_request
def before_request():
	"""init g.user."""
	g.user = None
	if 'user_id' in session:
		g.user = get_db().execute('select * from user where user_id=?',\
			[session['user_id']]).fetchone()

@app.teardown_appcontext
def close_database(e):
	db = get_db()
	db.execute('delete from code where is_saved=0')
	db.commit()
	top = _app_ctx_stack.top
	if hasattr(top, 'sqlite_db'):
		top.sqlite_db.close()

@app.route('/')
@app.route('/login', methods=['GET', 'POST'])
def login():
        if g.user:
            flash('you have alreay logged in', category='warnning')
            return redirect(url_for('run_code', code_id='new'));
	if request.method == 'POST':
		if request.form['username'] == '':
			flash('please input your name', category='warnning')
		elif request.form['password'] == '':
			flash('please input password', category='warnning')
		else:
			db_username = get_db().execute('select username from user where username=?',
				[request.form['username']]).fetchone()
			db_pw_hash = get_db().execute('select pw_hash from user where username=?',
				[request.form['username']]).fetchone()
			if db_username is None:
				flash('no such user, please sign up first', category='warnning')
			elif check_password_hash(db_pw_hash[0], request.form['password']):
				flash('welcome %s' % request.form['username'],category='success')
				session['username'] = request.form['username']
				session['user_id'] = get_db().execute('select user_id from user where username=?', 
					[request.form['username']]).fetchone()[0]
				return redirect(url_for('run_code', code_id='new'))
			else:
				flash('wrong password', category='warnning')
	return render_template('login.html')

@app.route('/logout')
def logout():
	if session['username']:
		del(session['username'])
		del(session['user_id'])
	return redirect(url_for('login'))

@app.route('/sign_up', methods=['GET', 'POST'])
def sign_up():
	if request.method == 'POST':
		username = request.form['username']
		pw = request.form['password']
		email = request.form['email']
		user = get_db().execute('select * from user where username=?',
		[username]).fetchone()
		if '@' not in email:
			flash('invalid email address', category='warnning')
		elif pw == '':
			flash('invalid password', category='warnning')
		elif user:
			flash('username has already been used!', category='warnning')
		elif request.form['confirm_password'] != request.form['password']:
			flash('please confirm password')
		else:
			db = get_db()
			db.execute('insert into user (username, email,\
			pw_hash) values(?, ?, ? )', [username, email, generate_password_hash(pw)])
			db.commit()
			return redirect(url_for('login'))
	return render_template('signup.html')


@app.route('/profile')
def profile():
	if g.user is None:
		return redirect(url_for('login'))
	else:
		all = get_db().execute('select * from saved_code where author_id=?',\
		[session['user_id']]).fetchall()
		return render_template('profile.html', entries=all, username=session['username'])

@app.route('/delete_entry', methods=['POST', 'GET'])
def delete_entry():
	if request.method=='GET':
		return redirect(url_for('profile'))
	else:
		author_id = session['user_id']
		saved_time = request.form['saved_time']
		db = get_db()
		code_id_t = db.execute('select code_id from saved_code where saved_time=? and author_id=?', [saved_time, author_id]).fetchone()
		if code_id_t != None:
			code_id = code_id_t[0]
			db.execute('delete from code where code_id=?', [code_id])
			db.execute('delete from saved_code where saved_time=? and author_id=?', [saved_time, author_id])
			db.commit()
			flash('delete entry success!', category='success')
		else:
			flash('delete entry failed', category='warnning')
		return render_template('profile.html')

@app.route('/run_code/<code_id>', methods=['GET', 'POST'])
def run_code(code_id):
	if g.user is None:
		return redirect('login')
	if request.method == 'POST':
		code_text = request.form['code_text']
		if code_text != '':
			code_name = request.form['code_name']
			code_info = request.form['code_info']
			author_id = session['user_id']
			data_input = request.form['data_input']
			code_time = int(time.time())
			if code_name != '':
				is_saved = 1
			else:
				is_saved = 0
			db = get_db()
			db.execute('insert into code (code_text, code_time, author_id, data_input,\
			is_saved) values(?, ?, ?, ?, ?)', [code_text, code_time, author_id, data_input,\
			is_saved])
			db.commit()
			code_id = get_db().execute('select code_id from code where author_id=? and\
				code_time=?', [author_id, code_time]).fetchone()[0]
			db = get_db()
			#sys.stderr.write('\ncode_text is:')
			#sys.stderr.write(code_text)
			#sys.stderr.write('\ncode_name is:')
			#sys.stderr.write(code_name)
			#sys.stderr.write('\n')
			if code_name != '':
				db.execute('insert into saved_code (code_id, author_id, code_name, saved_time, info)\
				values (?, ?, ?, ?, ?)', [code_id, author_id, code_name, code_time, code_info])
				db.commit()
			subprocess.call([MAIN_PATH, session['username'], str(code_id)])
			db_ok_result = get_db().execute('select ok_result from code where code_id=?',\
				[code_id]).fetchone()
			db_re_result = get_db().execute('select re_result from code where code_id=?',\
				[code_id]).fetchone()
			db_ce_result = get_db().execute('select ce_result from code where code_id=?',\
				[code_id]).fetchone()
			#return 'ok_result: %s' % db_ok_result[0]
			#return jsonify(ok_result='what?')
			return jsonify(ok_result=db_ok_result[0], re_result=db_re_result[0], ce_result=db_ce_result[0])
		else:
			flash('please input your code!', category='warnning')
			return 'in test'
	else:
		if code_id != 'new':
			code_text = get_db().execute('select code_text from code where code_id=?',\
				[code_id]).fetchone()[0]
		else:
			code_text = """
#include <stdio.h>
int main() {

}"""
		return render_template('index_ajax.html', code_text=code_text)
		#return render_template('index_form.html')

if __name__ == '__main__':
	app.run(host="0.0.0.0", port=8080, debug=True)
