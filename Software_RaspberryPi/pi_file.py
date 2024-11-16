# 3.3v                          --- 5v
# GPIO 2 (I2C)                  --- 5v
# GPIO 3 (I2C)                  --- GND
# GPIO 4                        --- GPIO 14 (TX)
# GPIO 17(SPI1)                 --- GPIO 18(PWM) : ENA/ENB (MOTOR)
# GPIO 27                       --- GND
# GPIO 22                       --- GPIO 23 : IN1/IN2 (MOTOR)
# 3.3V                          --- GPIO 24 
# GPIO 10(MOSI SPI0)            --- GND
# GPIO 9 (MISO SPI0)            --- GPIO 25
# GPIO 11 (SCLK SPI0)           --- GPIO 8 (CE SPI0)
# GND                           --- GPIO 7 (CE1 SPI0)
# GPIO 0 (SDA I2C)              --- GPIO 1 (SCA I2C)
# GPIO 5                        --- GND
# GPIO 6                        --- GPIO 12 (PWM) : STEERING
# GPIO 13 (PWM) : ROT           --- GND
# GPIO 19 (MISO SPI1 / PWM)     --- GPIO 16 (CE2 SPI1)
# GPIO 26                       --- GPIO 20 (MISO SPI1)
# GND                           --- GPIO 21 (SCLK SPI 1)

from flask import Flask, render_template, Response, request
import json
import cv2
import logging

from Pi_control import Control_Car

app = Flask(__name__)
camera = cv2.VideoCapture(0)
control = Control_Car()

if __name__ == '__main__':
    app.run()

@app.route('/')
def index():
    acc_val = request.args.get('ACC', type=int)
    str_val = request.args.get('GET', type=int)
    fla_val = request.args.get('FLA', type=int)
    roten_val = request.args.get('ROT_EN', type=int)
    logging.warning(acc_val)
    control(acc_val,str_val,roten_val)
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

