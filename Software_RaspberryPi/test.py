from flask import Flask, render_template, Response, request
import json
import cv2
import logging

app = Flask(__name__)
camera = cv2.VideoCapture(0)

if __name__ == '__main__':
    app.run()

@app.route('/')
def index():
    acc_val = request.args.get('ACC', type=int)
    str_val = request.args.get('GET', type=int)
    fla_val = request.args.get('FLA', type=int)
    roten_val = request.args.get('ROT', type=int)
    logging.warning(acc_val)
    return render_template('phone_control.html')

@app.route('/streaming')
def streaming_cam():
    return Response(gen_frame(), mimetype='multipart/x-mixed-replace; boundary=frame')

def gen_frame():
    while True:
        success, frame = camera.read()
        if not success:
            break
        else:
            ret, buffer = cv2.imencode('.jpg',frame)
            frame = buffer.tobytes()
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')  # concat frame one by one and show result


    



