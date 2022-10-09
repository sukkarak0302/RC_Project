#include <WiFi.h>
#include <HTTPClient.h>

//Sensor Attachment
#define PIN_BRK 34
#define PIN_ACC 39
#define PIN_STR 36
#define PIN_GER_UP 33
#define PIN_GER_DN 32

#define MAX_BRK 900        
#define MAX_ACC 2300
#define MAX_STR 3600  

#define MIN_BRK 500
#define MIN_ACC 1900  
#define MIN_STR 0


#define NUM_SENSOR 3

#define WIFI_SSID "GO_PROFI"
#define WIFI_PASS "12345678"

int PIN_CONFIG[NUM_SENSOR] = { PIN_ACC, PIN_BRK, PIN_STR };
int MAX_CONFIG[NUM_SENSOR] = { MAX_ACC, MAX_BRK, MAX_STR };
int MIN_CONFIG[NUM_SENSOR] = { MIN_ACC, MIN_BRK, MIN_STR };
int STP_CONFIG[NUM_SENSOR] = { 3, 1, 9 };

HTTPClient http;
int Read_Potentiometer(int idx);
void Send_Data(int * State);

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  delay(100);

  Serial.println();

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println(".");
  }
  Serial.println("Connected");

  pinMode(PIN_GER_UP, INPUT_PULLDOWN);
  pinMode(PIN_GER_DN, INPUT_PULLDOWN);

}

void loop() {
  // put your main code here, to run repeatedly:
  int sensor_val[NUM_SENSOR];
  int gear_val;
  //Serial.println("Sensor Val : ");
  for(int i = 0; i< NUM_SENSOR; i++)
  {
    sensor_val[i] = Read_Potentiometer(i);
    if (i == 2)
    {
      sensor_val[i] = Str_Comp(sensor_val[i]);
    }
    //Serial.println(sensor_val[i]);
  }
  gear_val = Read_GearValue();
  Send_Data(sensor_val, gear_val);
  //
  delay(50);
}

int Read_Potentiometer(int idx)
{
  int val = analogRead(PIN_CONFIG[idx]);
  int stp = ( MAX_CONFIG[idx] - MIN_CONFIG[idx] ) / ( STP_CONFIG[idx] + 1 ) ;

  if ( val >= MAX_CONFIG[idx] )
  {
    val = MAX_CONFIG[idx] - 1;
  }
  else if ( val <= MIN_CONFIG[idx] )
  {
    val = MIN_CONFIG[idx];
  }
  else
  {
    //None
  }
  int output = ( val - MIN_CONFIG[idx] ) / stp;
  //Serial.println(val);
  //Serial.println(output);
  return output;
}

int Str_Comp(int inp)
{
  return STP_CONFIG[2] - inp;
}

void Send_Data(int * State, int Gear_val)
{
  if ( State[1] == 1 )
  {
    State[0] = 0;
  }
  String query = "?ACC="+String(State[0])+"&BRK="+String(State[1])+"&STR="+String(State[2])+"&GER="+String(Gear_val);
  Serial.println(query);
  String addr = "http://192.168.4.1/joyControl"+query; //?ACC=1&BRK=1&STR=1&GER=1";//+State[0]+"&BRK="+State[1]+"&STR="+State[2]+"&GER="+Gear_val;  
  //Serial.println(addr);

  http.begin(addr);
  http.GET();
  http.end();
}

int Read_GearValue()
{
  int ger_up = digitalRead(PIN_GER_UP);
  int ger_dn = digitalRead(PIN_GER_DN);
  //Serial.println(ger_up);
  //Serial.println(ger_dn);
  if(ger_up)
  {
    return 1;
  }
  else if(ger_dn)
  {
    return 2;
  }
  else
  {
    return 0;
  }
}
