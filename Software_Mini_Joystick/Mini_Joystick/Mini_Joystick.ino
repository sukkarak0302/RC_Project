/*
            + 3v3 --------------- GND
            + EN  --------------- 23
PIN_POT0    + 36(ADC1_0) -------- 22(I2C_SCL)   + PIN_LCD_SCL
PIN_LEFT    + 39 ---------------- 1
PIN_RIGHT   + 34 ---------------- 3
PIN_GEAR_0  + 35 ---------------- 21(I2C_SDA)   + PIN_LCD_SDA
PIN_GEAR_1  + 32 ---------------- GND
PIN_MODE_0  + 33 ---------------- 19(VSPI_MISO)
PIN_MODE_1  + 25 ---------------- 18(VSPI_CLK)
PIN_SWMODE  + 26 ---------------- 5(VSPI_CS)
            + 27 ---------------- 17(TX2)
PIN_RAD_CLK + 14(SPI_CLK) ------- 16(RX2)
PIN_RAD_MISO+ 12(SPI_MISO) ------ 4(ADC2_0)      + PIN_POT1
            + GND --------------- 0              + PIN_RAD_CE
PIN_RAD_MOSI+ 13(SPI_MOSI) ------ 2              + PIN_RAD_CSN
            + 9  ---------------- 15(SPI_CS)
            + 10 ---------------- 8
            + 11 ---------------- 7
            + 5V ---------------- 6
*/

// Tutorial : https://howtomechatronics.com/tutorials/arduino/arduino-wireless-communication-nrf24l01-tutorial/

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define PIN_GEAR_0 35
#define PIN_GEAR_1 32
#define PIN_MODE_0 33
#define PIN_MODE_1 25
#define PIN_SWMODE 26
#define PIN_LEFT   39
#define PIN_RIGHT  34
#define PIN_POT0   36
#define PIN_POT1    4

#define PIN_RAD_CLK 14
#define PIN_RAD_MISO 12
#define PIN_RAD_MOSI 13
#define PIN_RAD_CE 0
#define PIN_RAD_CSN 2

#define PIN_LCD_SCL 22
#define PIN_LCD_SDA 21

#define NUM_SENS 9

#define MAX_POT_VAL 1000
#define MIN_POT_VAL 0

#define TRANSMIT_INTERVAL 100

#define TRANS_MAX 7

RF24 radio(PIN_RAD_CE,PIN_RAD_CSN); // CE, CSN
const byte address[6] = "00001";

// [Pot0, Pot1, Gear, Mode, Ind/Rot-Mode, Left, Right]
int switch_status[NUM_SENS] = {};
int switch_pin[NUM_SENS] = {PIN_POT0, PIN_POT1, PIN_GEAR_0, PIN_GEAR_1, PIN_MODE_0, PIN_MODE_1, PIN_SWMODE, PIN_LEFT, PIN_RIGHT};
int potentometer_range[2][2] = { {1500, 3200}, {1200, 3150}}; 

uint status[7] = {};
uint status_pre[7] = {};
uint transmit_st[7] = {};

//int status_transmit[7] = {};
int status_transmit{};

void setup() {
  Serial.begin(115200);

  // put your setup code here, to run once:
  for(int ind = 2; ind < NUM_SENS; ind++) {
    pinMode(switch_pin[ind], INPUT);
  }
  // radio related
  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();
  
}

void loop() {
  // put your main code here, to run repeatedly:
  read_value();
  transmit_value_set();
  value_test();
  value_set();
  radio_transmit();
  delay(100);
}

void value_test() {
  for(int i = 0; i < 7; i++) {Serial.print(status[i]); Serial.print(" / ");}
  //Serial.println();
}

void transmit_value_set() {
  for(int idx = 0; idx < 7; idx++) {
    transmit_st[idx] = 0;
    if(idx >= 5) {
      if(status_pre[idx] != status[idx]) transmit_st[idx] = 1;
      else transmit_st[idx] = 0;
    }
    else {
      transmit_st[idx] = status[idx];
    }
  }
}

void read_value() {
  for(int ind = 0; ind < NUM_SENS; ind++) {
    if(ind < 2) {
      switch_status[ind] = analogRead(switch_pin[ind]);  
    }
    else {
      switch_status[ind] = digitalRead(switch_pin[ind]);
    }
  }

  //Potentiometer
  //status[0] = static_cast<int>(switch_status[0] / (MAX_POT_VAL - MIN_POT_VAL) / 9) ;
  //status[1] = static_cast<int>(switch_status[1] / (MAX_POT_VAL - MIN_POT_VAL) / 9) ;
  for(int pot_idx = 0; pot_idx < 2; pot_idx++) {
    status[pot_idx] = int(9.0 - float( switch_status[pot_idx] - potentometer_range[pot_idx][0] ) / float( potentometer_range[pot_idx][1] - potentometer_range[pot_idx][0] ) * 9.0);
    if(status[pot_idx] > 9) status[pot_idx] = 9;
    else if(status[pot_idx] < 0) status[pot_idx] = 0;
  }
  //status[0] = switch_status[0];
  //status[1] = switch_status[1];

  //Gear
  if(switch_status[2] + switch_status[3] < 2) {
    if(switch_status[2] + switch_status[3] == 0) status[2] = 0; // Neutral 
    else if(switch_status[2] == 1) status[2] = 2; // Forward
    else  status[2] = 1; // Reward
  } 

  // Mode
  if(switch_status[4] + switch_status[5] == 1) {
    if(switch_status[4] == 1) status[3] = 1; // MODE1 - automatic
    else status[3] = 0; // MODE2 - Manual
  }

  // Switch Mode
  if(switch_status[6] == 0) status[4] = 1; // Turn Indicator
  else status[4] = 0; // Camera turning

  // Left button
  if(switch_status[7] == 1) status[5] = 1; // Pressed
  else status[5] = 0;

  // Right button
  if(switch_status[8] == 1) status[6] = 1; // Pressed
  else status[6] = 0;
}


void value_set() {
  status_transmit = 0;
  for(int idx = 0; idx < 7; idx++) {
    status_transmit = status_transmit | (( transmit_st[idx] & 0xF ) << ( idx * 4 ));
  }
}

void radio_transmit() {
  Serial.println(status_transmit);
  radio.write(&status_transmit, sizeof(status_transmit));
  /*
  for (int idx = 0; idx < TRANS_MAX; idx++) {
    radio.write(status_transmit[idx], sizeof(status_transmit[idx]));
  }
  */
}




