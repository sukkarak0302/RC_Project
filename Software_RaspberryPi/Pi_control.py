# 3.3v                          --- 5v
# GPIO 2 (I2C)                  --- 5v
# GPIO 3 (I2C)                  --- GND
# GPIO 4                        --- GPIO 14 (TX)
# GPIO 17(SPI1)                 --- GPIO 18(PWM) : ENA/ENB (MOTOR)
# GPIO 27                       --- GND
# GPIO 22                       --- GPIO 23 : IN1/IN2 (MOTOR)
# 3.3V                          --- GPIO 24 : IN3/IN4 (MOTOR)
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

import RPi.GPIO as GPIO

PIN_MOTOR_PWM = 18
PIN_MOTOR_DIR1 = 23
PIN_MOTOR_DIR2 = 24
PIN_SERVO_STR = 12
PIN_SERVO_DIR = 13

FREQUENCY_MOTOR_PWM = 1000
FREQUENCY_MOTOR_STR = 50
FREQUENCY_MOTOR_DIR = 50

class ControlCar:
    def __init__(self) :
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(PIN_MOTOR_PWM, GPIO_OUT)
        GPIO.setup(PIN_MOTOR_DIR1, GPIO_OUT)
        GPIO.setup(PIN_MOTOR_DIR2, GPIO_OUT)
        GPIO.setup(PIN_SERVO_STR, GPIO_OUT)
        GPIO.setup(PIN_SERVO_DIR, GPIO_OUT)
        self.motor_pwm = GPIO.PWM(PIN_MOTOR_PWM, FREQUENCY_MOTOR_PWM)
        self.steering_pwm = GPIO.PWM(PIN_SERVO_STR, FREQUENCY_MOTOR_STR)
        self.rotation_pwm = GPIO.PWM(PIN_SERVO_DIR, FREQUENCY_MOTOR_DIR)

    def control(self, motor, steering, rotation) :
        if motor == 5 :
            self.output(PIN_MOTOR_DIR1, False)
            self.output(PIN_MOTOR_DIR2, False)
            self.motor_pwm.ChangeDutyCycle(0)
        elif motor < 5 :
            self.output(PIN_MOTOR_DIR1, False)
            self.output(PIN_MOTOR_DIR2, True)
            self.motor_pwm.ChangeDutyCycle(((motor-5) / 4.0)*100)
        else :
            self.output(PIN_MOTOR_DIR1, True)
            self.output(PIN_MOTOR_DIR2, False)
            self.motor_pwm.ChangeDutyCycle(((motor-5) / 4.0)*100)
            
        self.steering_pwm.ChangeDutyCycle(((9-steering)/10.0 + 1.0)*1024.0/20.0)
        self.rotation_pwm.ChangeDutyCycle(((9-rotation)/10.0 + 1.0)*1024.0/20.0)