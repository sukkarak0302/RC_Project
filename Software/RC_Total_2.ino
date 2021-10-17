#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <String.h>
#include <esp_camera.h>
#include <HTTPClient.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"

//#include "portmacro.h"

#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

// WIFI SSID, PW part
const char* ssid = "RC_Control";
const char* password = "12345678";

void startCameraServer_Core1();
void startCameraServer_Core0();
void startCameraServer_Both();
void startCameraServer_Sens();
void set_DistVal(int val);
int get_ACCVal();
int get_STRVal();
//static int SEN_distance = 0;

// Task definition
TaskHandle_t CAM_Handle;
TaskHandle_t CTRL_Handle;
TaskHandle_t SENSOR_Handle;
TaskHandle_t LED_Handle;

WiFiServer server(80);

// Traction/Steering Motor control
int AIA = 2;
int AIB = 14;
int STR = 15;
int SEN_TRIG = 12;
int SEN_ECHO = 13;
int LED = 4;

int CurACC=5;
int CurSTR=5;
int PreSTR = 0;
int PreACC = 0;

static int SEN_distance=0;
unsigned long preTime = 0;
unsigned long preTimeCtrl = 0;

int Core0_Setup = 0;

void setup() {
  Serial.begin(115200);

  xTaskCreate (
    CAM_Task,   // task function
    "CAM_Task", // name of task
    10000,   // stack size of task
    NULL,  // parameter of the task
    3,       // priority of the task
    &CAM_Handle);   // task handle to keep track of created task
    delay(1000);

  xTaskCreate (
    Control_Task,   // task function
    "Control_Task", // name of task
    10000,   // stack size of task
    NULL,  // parameter of the task
    1,       // priority of the task
    &CTRL_Handle);   // task handle to keep track of created task
    delay(1000);

  xTaskCreate (
    Sensor_Task,   // task function
    "Sensor_Task", // name of task
    10000,   // stack size of task
    NULL,  // parameter of the task
    2,       // priority of the task
    &SENSOR_Handle);   // task handle to keep track of created task
    
  xTaskCreate (
    LED_Task,   // task function
    "LED_Task", // name of task
    10000,   // stack size of task
    NULL,  // parameter of the task
    4,       // priority of the task
    &LED_Handle);   // task handle to keep track of created task

    
    //esp_task_wdt_delete(CAM_Handle);
    //esp_task_wdt_delete(CTRL_Handle);
    //esp_task_wdt_delete(SENSOR_Handle);
    //esp_task_wdt_delete(LED_Handle);  

}

void loop() 
{
  
}

void Control_Level(int acc, int str, int Change)
{
  int accLevel;
  int strLevel;
  int Driving_dir = 0;
  
  accLevel = abs(254/4*(acc-5));
  //strLevel = -(6554/2)/8*(str-1) + 6544;
  strLevel = -(5800/2)/8*(str-1) + 5800;
  Serial.printf(" ACCLev : %d / STRLev : %d \n", accLevel, strLevel);
  //strLevel = (4000-6554)/8*(str-1) + 6544;
  if ( Change == 2 )
  {
    if( acc>5 )
    {
      ledcWrite(11, 0);
      ledcWrite(13, accLevel);
    }
    else
    {
      ledcWrite(11, accLevel);
      ledcWrite(13, 0);
    }
  }
  if ( Change == 1 )
  {
    ledcWrite(15, strLevel);
  }
}

void CAM_Task(void * pvParameters )
{
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
  // camera init
#endif

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err); 
    vTaskDelete(NULL);
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  Serial.println("CAM config successful");

  /* Wifi started */
  Serial.println();
  Serial.println("Configuring access point...");

  // You can remove the password parameter if you want the AP to be open.
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  startCameraServer_Both();

  server.begin();

  Serial.println("Server started");
  Core0_Setup = 1;
  
  while(1)
  {
    delay(100);
  }

  vTaskDelete(NULL);
}

void Control_Task(void * pvParameters)
{ 
  int initialized = 0;
  int counter = 1;
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  esp_task_wdt_delete(NULL);

  portTickType LastWakeTime;
  const portTickType Frequency = 20 * portTICK_RATE_MS;
  int duration = 0;

  LastWakeTime = xTaskGetTickCount();
  
  while(1)
  {
    if ( Core0_Setup == 1 )
    {
      if (initialized == 0 )
      {
        //startCameraServer_Core1();
        Serial.print("CORE1 Initialized\n");
        initialized = 1;
        
        pinMode(AIA, OUTPUT);
        pinMode(AIB, OUTPUT);
        pinMode(STR, OUTPUT);
        ledcSetup(11, 255,8);
        ledcAttachPin(AIA, 11);
        ledcSetup(13, 255, 8);
        ledcAttachPin(AIB, 13);
      
        //Steering Motor
        ledcSetup(15, 50, 16);
        ledcAttachPin(STR, 15);
      }
      UserInput2();
      //vTaskDelayUntil( &LastWakeTime, Frequency );
    }
    else
    {
      delay(1000);
    }
  }
  vTaskDelete(NULL);
}

void UserInput2()
{
  //unsigned long curTime = millis();
  int ACC_Val = get_ACCVal();
  int STR_Val = get_STRVal();
  int Change = 0;
 
  CurACC = ACC_Val;
  CurSTR = STR_Val;
  if( ( CurSTR != PreSTR )  )
  {
    Control_Level(CurACC,CurSTR,1);
  }
  else if ( ( CurACC != PreACC ) )
  {
    Control_Level(CurACC,CurSTR,2);
  }
  PreACC = CurACC;
  PreSTR = CurSTR;
}

void Sensor_Task(void * pvParameters)
{
  pinMode(SEN_TRIG,OUTPUT);
  pinMode(SEN_ECHO,INPUT);

  portTickType LastWakeTime;
  const portTickType Frequency = 200 * portTICK_RATE_MS;
  int duration = 0;

  LastWakeTime = xTaskGetTickCount();

  while(1)
  {
    digitalWrite(SEN_TRIG,LOW);
    delayMicroseconds(2);
    digitalWrite(SEN_TRIG,HIGH);
    delayMicroseconds(10);
    digitalWrite(SEN_TRIG,LOW);
    
    duration = pulseIn(SEN_ECHO, HIGH);
    SEN_distance = duration / 29 / 2;
    set_DistVal(SEN_distance);
    //Serial.printf("Distance : %d\n", SEN_distance);
    vTaskDelayUntil( &LastWakeTime, Frequency );
  }
  vTaskDelete(NULL);
}

void LED_Task(void * pvParameters)
{
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  vTaskDelete(NULL);
}
