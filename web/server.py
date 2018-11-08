from bottle import route, run, template
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

@route('/<app_name>/steps')
def show_steps(app_name):
    try:
        result = subprocess.check_output(['../bin/heap', app_name, 'invisible'])
    except:
        return "something wrong"
    print repr(result)
    steps = parse_steps(result)
    print steps
    return template('steps', app_name=app_name, steps=steps)

run(host='localhost', port=8080, debug=True)
