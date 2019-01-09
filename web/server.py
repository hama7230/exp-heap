from bottle import route, run, template, abort, request
import os
import subprocess

@route("/hello")
def hello():
    return "hoge"

@route('/<app_name>')
def binary(app_name):
    return "Opeing %s..." % app_name 
    

def parse_steps(raw):
    lines = raw[:-1].split('\n')
    res = []
    for line in lines:
        res.append(line.split())
    return res

def parse_chunks(raw):
    res = {}
    buf = []
    
    lines = raw.split('\n')
    for i, l in enumerate(lines):
        if l == 'All Chunks':
            break
    print i
    all_chunks = lines[i+1]
    chunks = all_chunks[:-1].split(' ')
    buf = map(lambda chunk: [int(chunk.split(':')[0], 16)
                           , int(chunk.split(':')[1], 16)
                           , True if chunk.split(':')[2] is '1' else False], chunks) 
    res['all_chunks'] = buf
    
    

    print res



@route('/<app_name>/steps')
def show_steps(app_name):
    
    print 'check the binary exists'
    path_name = '/tmp/trace_heap/%s' % app_name
    if not os.path.isfile(path_name):
        abort(404, 'not exists')

    try:
        print 'try showing steps on %s' % app_name
        result = subprocess.check_output(['../bin/heap', app_name, 'invisible', 'steps'])
    except:
        abort(500, 'failed analysis the binary')

    print repr(result)
    steps = parse_steps(result)
    print steps
    return template('steps', app_name=app_name, steps=steps)



@route('/<app_name>/chunks')
def show_chunks(app_name):
    
    print 'check the binary exists'
    path_name = '/tmp/trace_heap/%s' % app_name
    if not os.path.isfile(path_name):
        abort(404, 'not exists')
    
    index = request.params.get('index')
    print 'index = ', index
    if index is None:
        index = -1

    try:
        print 'try showing chunks on %s' % app_name
        result = subprocess.check_output(['../bin/heap', app_name, 'invisible', 'chunks', index])
    except:
        return "something wrong"
    
    parse_chunks(result)
    return result

run(host='0.0.0.0', port=8080, debug=True)
