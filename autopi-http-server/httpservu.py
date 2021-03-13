from flask import Flask, request, jsonify
from flask_httpauth import HTTPBasicAuth
from werkzeug.security import generate_password_hash, check_password_hash
from werkzeug.exceptions import HTTPException
import shelve
import json
import time
import logging

app = Flask(__name__)
auth = HTTPBasicAuth()
log = logging.getLogger(__name__)

users = {
    "battery_status": generate_password_hash("password"),
    "view": generate_password_hash("view")
}

flask_settings = {
    "host": "0.0.0.0",
    "port": 8000,
    "threaded": False,
}


def get_current_status():
    with shelve.open('/tmp/battery_status.shv') as db:
        return json.dumps(db['status'])


@auth.verify_password
def verify_password(username, password):
    if username in users and \
            check_password_hash(users.get(username), password):
        return username


@app.errorhandler(Exception)
def handle_error(e):
    log.error('exception occurred: {:}'.format(e))
    code = 500
    if isinstance(e, HTTPException):
        code = e.code
    return jsonify(error=str(e)), code


@app.route('/')
@auth.login_required
def index():
    return get_current_status()


@app.route('/api/update_battery_status', methods=['GET', 'POST'])
@auth.login_required
def update_battery_status():
    content = request.get_json(silent=True, force=False)

    if content is not None:
        content['timestamp'] = time.time()
        with shelve.open('/tmp/battery_status.shv') as db:
            db['status'] = content

    print(content)
    return "Updated"


def start_server():
    log.debug("Starting flask with settings: {:}".format(flask_settings))
    app.run(**flask_settings)


def dump_handler():
    return get_current_status()


if __name__ == '__main__':
    start_server()


def start(**settings):
    global flask_settings
    flask_settings.update(settings["flask"])
    log.debug(
        "Starting battery monitor service with settings: {:}".format(settings))
    start_server()
