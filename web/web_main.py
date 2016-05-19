# coding: utf-8

from flask import render_template, Flask, request, redirect, url_for, \
    config, _app_ctx_stack, current_app, flash, g, session, jsonify, json
import sqlite3
import time
import sys
import subprocess
from werkzeug.security import check_password_hash, generate_password_hash
import gdbmi
import os
import socket

#config
DATABASE_PATH='/home/shaughn/707/bishe/web_main.db'
DATABASE_SCRIPT='web_main.sql'
SECRET_KEY='test secret key'
MAIN_PATH='/home/shaughn/707/bishe/main'
CODE_PATH='/home/shaughn/707/bishe/code_data'
MOBWRITE_DAEMON_PORT=3017

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

@app.route('/test')
def test():
    return render_template('test.html')
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
                session['code_path'] = os.path.join(CODE_PATH, session['username'])
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

def temp_compile_code(code_text):
    if code_text != '':
        fn = os.path.join(session['code_path'], 'temp_compile_file.c')
        outfn = os.path.join(session['code_path'], 'a.out')
        f = open(fn, 'w')
        f.write(code_text)
        f.close()
        ret = subprocess.call(['/usr/bin/gcc', '-g', fn, '-o', outfn])
        return ret
    return -1

@app.route('/debug', methods=['GET', 'POST'])
def debug():
    if request.method == 'GET':
        return redirect(url_for('run_code', code_id='new'))
    # post
    cmd = request.form['cmd']
    if cmd == '':
        return redirect(url_for('run_code', code_id='new'))
    if getattr(current_app, 'gdb', None) is None:
        setattr(current_app, 'gdb', {})
    if cmd == 'start': 
        ret = temp_compile_code(request.form['code_text'])
        if ret != 0:
            return jsonify(debug='compile failed, maybe you should run the code first \
                    to check the error message<br>')
        p = gdbmi.Session(os.path.join(session['code_path'], 'a.out')).start()
        if current_app.gdb.has_key(session['user_id']): 
            current_app.gdb[session['user_id']].process.kill()
            sys.stderr.write(str(current_app.gdb[session['user_id']].process.returncode))
            del current_app.gdb[session['user_id']]
        current_app.gdb[session['user_id']] = p
        cur_gdb = p
        sys.stderr.write(str(cur_gdb.process.pid)+'\n')
        #token = cur_gdb.send('-break-insert main')
        #while not cur_gdb.wait_for(token):
        #    pass
        token = cur_gdb.send('-exec-run')
        while not cur_gdb.wait_for(token):
            pass
        debug_ret = cur_gdb.ret_str.strip()
        if cur_gdb.line_no != "":
            tempstr = 'line(%s): ' % cur_gdb.func.strip()
            debug_ret = tempstr+cur_gdb.line_no.strip() \
                    +' '+debug_ret
        return jsonify(debug=debug_ret+'<br>')

    cur_gdb = current_app.gdb[session['user_id']]
    if cmd == 'update_breakpoints':
        token = cur_gdb.send('-break-delete')
        while not cur_gdb.wait_for(token):
            pass
        bps = request.form['arg']
        for x in bps.split(','):
            if x != '':
                token = cur_gdb.send('-break-insert '+x)
                while not cur_gdb.wait_for(token):
                    pass
    if cmd == 'next':
        token = cur_gdb.send('-exec-next')
        while not cur_gdb.wait_for(token):
            pass
    if cmd == 'continue':
        token = cur_gdb.send('-exec-continue')
        while not cur_gdb.wait_for(token):
            pass
    if cmd == 'step_in':
        token = cur_gdb.send('-exec-step')
        while not cur_gdb.wait_for(token):
            pass
    if cmd == 'backtrace':
        token = cur_gdb.send('-data-evaluate-expression main')
        while not cur_gdb.wait_for(token):
            pass
    if cmd == 'add_bp':
        line_num = request.form['arg'];
        token = cur_gdb.send('-break-insert'+line_num)
        while not cur_gdb.wait_for(token):
            pass
    if cmd == 'del_bp':
        bp_no = request.form['arg']
        token = cur_gdb.send('-break-delete'+bp_no)
        while not cur_gdb.wait_for(token):
            pass
    if cmd == 'list_bp':
        token = cur_gdb.send('-break-list')
        while not cur_gdb.wait_for(token):
            pass
    if cmd == 'print_var':
        var_name = request.form['arg']
        token = cur_gdb.send('-data-evaluate-expression '+var_name)
        while not cur_gdb.wait_for(token):
            pass
    if cmd == 'stop':
        cur_gdb.process.kill()
        sys.stderr.write(cur_gdb.process.process.poll())
    sys.stderr.write(cur_gdb.ret_str.strip())
    debug_ret = cur_gdb.ret_str.strip()
    if cur_gdb.line_no != "":
        tempstr = 'line(%s): ' % cur_gdb.func.strip()
        debug_ret = tempstr+cur_gdb.line_no.strip() \
                +' '+debug_ret
    return jsonify(debug=debug_ret+'<br>')

@app.route('/run_code/<code_id>', methods=['GET', 'POST'])
def run_code(code_id):
    if g.user is None:
        return redirect('login')
    if request.method == 'POST':
        code_text = request.form['code_text']
        if code_text != '':
            code_name = request.form['code_name']
            code_info = request.form['code_info']
            #sys.stderr.write('debug!!!: '+code_name+ ' '+code_text+'\n')
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
            sys.stderr.write(code_text)
            sys.stderr.write(db_ok_result[0])
            sys.stderr.write(db_re_result[0])
            sys.stderr.write('debug over!!')
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
        return render_template('index_ajax.html', code_text=code_text, username=session['username'])
        #return render_template('index_form.html')

@app.route('/gateway', methods=['POST'])
def gateway():
    outStr = '\n'
    if request.form.has_key('q'):
        outStr = request.form['q']
    elif request.form.has_key('p'):
        outStr = request.form['p']

    inStr = ''
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect(('localhost', MOBWRITE_DAEMON_PORT))
    except socket.error, msg:
        s = None
    if not s:
        inStr = '\n'
    else:
        s.settimeout(10.0)
        s.send(outStr)
    while 1:
        line = s.recv(1024)
        if not line:
            break
        inStr += line
    s.close()
    
    return inStr+'\n'

    
if __name__ == '__main__':
    app.run(host="0.0.0.0", port=8080, debug=True)
    #socketio.run(app, host="0.0.0.0", port=8080, debug=True)
