from flask import Flask, request

app = Flask(__name__)

@app.route('/post', methods = ["POST"])
def post():

    print(request.data)
    return ''

app.run(host='0.0.0.0', port= 8090)