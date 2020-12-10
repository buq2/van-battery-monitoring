from flask import Flask,request
from flask_httpauth import HTTPBasicAuth
from werkzeug.security import generate_password_hash, check_password_hash

app = Flask(__name__)
auth = HTTPBasicAuth()

users = {
    "battery_status": generate_password_hash("password"),
    "view": generate_password_hash("view")
}

@auth.verify_password
def verify_password(username, password):
    if username in users and \
            check_password_hash(users.get(username), password):
        return username

@app.route('/')
@auth.login_required
def index():
    return "Hello, {}!".format(auth.current_user())

@app.route('/api/update_battery_status', methods=['GET', 'POST'])
@auth.login_required
def update_battery_status():
    content = request.get_json(silent=True, force=False)
    print(content) # Do your processing
    return "thanks"

if __name__ == '__main__':
    app.run(port=8000, host='0.0.0.0')