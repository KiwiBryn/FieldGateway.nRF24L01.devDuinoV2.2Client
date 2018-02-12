//---------------------------------------------------------------------------------
// Copyright ® January 2018, devMobile Software
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Thanks to the creators and maintainers of the libraries used by this project
//   https://github.com/maniacbug/RF24
//   https://github.com/Seeed-Studio/Grove_Temper_Humidity_TH02
//  https://github.com/sparkfun/SparkFun_ATSHA204_Arduino_Library
//
#include <LowPower.h>
#include <RF24.h>
#include <sha204_library.h>
#include <TH02_dev.h>

// nRF24L01 ISM wireless module setup for devDuino V2,2 https://www.elecrow.com/devicter-sensor-node-v22-atmega-328-with-support-ota-update-p-1485.html
// RF24 radio( ChipeEnable , ChipSelect )
RF24 radio(7,6);
const byte FieldGatewayChannel = 10 ;
const byte FieldGatewayAddress[5] = "Base1";
const rf24_datarate_e RadioDataRate = RF24_250KBPS; 
const rf24_pa_dbm_e RadioPALevel = RF24_PA_MIN;

// Payload configuration
const int PayloadSizeMaximum = 32 ;
char payload[PayloadSizeMaximum] = "";
const byte DeviceIdPlusCsvSensorReadings = 1 ;
const byte SensorReadingSeperator = ',' ;

// ATSHA204 secure authentication, validation with crypto and hashing (initially only used for unique serial number) 
atsha204Class sha204(A2);
const int DeviceSerialNumberLength = 9 ;
uint8_t deviceSerialNumber[DeviceSerialNumberLength];

// SI7005 Indoor temperature and humdity sensor
#include <TH02_dev.h>

const int LoopSleepDelaySeconds = 300 ;


void setup() 
{
  Serial.begin(9600);
  Serial.println("Setup called");

  // Retrieve the serial number then display it nicely
  sha204.getSerialNumber(deviceSerialNumber);

  Serial.print("SNo:");
  for (int i=0; i<sizeof( deviceSerialNumber) ; i++)
  {
    // Add a leading zero
    if ( deviceSerialNumber[i] < 16)
    {
      Serial.print("0");
    }    
    Serial.print(deviceSerialNumber[i], HEX);
    Serial.print(" ");
  }
  Serial.println(); 

  // Configure the Seeedstudio TH02 temperature & humidity sensor
  Serial.println("TH02 setup");
  TH02.begin();
  delay(100);

  // Configure the nRF24 module
  Serial.println("nRF24 setup");
  radio.begin();
  radio.setChannel(FieldGatewayChannel);
  radio.openWritingPipe(FieldGatewayAddress);
  radio.setDataRate(RadioDataRate) ;
  radio.setPALevel(RadioPALevel);
  radio.enableDynamicPayloads();

  Serial.println("Setup done");
}


void loop() 
{
  int payloadLength = 0 ;    
  float temperature ;
  float humidity ;
  float batteryVoltage ;
  
  Serial.println("Loop called");
  memset( payload, 0, sizeof( payload));

  // prepare the payload header with PayloadMessageType (top nibble) and DeviceID length (bottom nibble)
  payload[0] = (DeviceIdPlusCsvSensorReadings << 4) | sizeof(deviceSerialNumber) ; 
  payloadLength += 1;

  // Copy the device serial number into the payload
  memcpy( &payload[payloadLength], deviceSerialNumber, sizeof( deviceSerialNumber));
  payloadLength += sizeof( deviceSerialNumber) ;

  // Read the temperature, humidity & battery voltage values then display nicely
  temperature = TH02.ReadTemperature();
  humidity = TH02.ReadHumidity();
  Serial.print("T:");
  Serial.print( temperature, 1 ) ;
  Serial.print( "C" ) ;

  Serial.print(" H:");
  Serial.print( humidity, 0 ) ;
  Serial.print( "%" ) ;

  batteryVoltage = readVcc() / 1000.0 ;
  Serial.print(" B:");
  Serial.print( batteryVoltage, 2 ) ;
  Serial.println( "V" ) ;

  // Copy the temperature into the payload
  payload[ payloadLength] = 't';
  payloadLength += 1 ;
  dtostrf(temperature, 6, 1, &payload[payloadLength]);  
  payloadLength += 6;
  payload[ payloadLength] = ',';
  payloadLength += 1 ;

  // Copy the humidity into the payload
  payload[ payloadLength] = 'h';
  payloadLength += 1 ;
  dtostrf(humidity, 4, 0, &payload[payloadLength]);  
  payloadLength += 4;
  payload[ payloadLength] = ',';
  payloadLength += 1 ;

  // Copy the battery voltage into the payload
  payload[ payloadLength] = 'v';
  payloadLength += 1 ;
  dtostrf(batteryVoltage, 5, 2, &payload[payloadLength]);  
  payloadLength += 5;

  // Powerup the nRF24 chipset then send the payload to base station
  Serial.print( "Payload length:");
  Serial.println( payloadLength );

  radio.powerUp();
  delay(100);
  
  Serial.println( "nRF24 write" ) ;
  boolean result = radio.write(payload, payloadLength);
  if (result)
    Serial.println("Write Ok...");
  else
    Serial.println("Write failed.");
 
 Serial.println( "nRF24 power down" ) ;
 radio.powerDown();

 delay(100);
 
 for( int i = 0 ; i < ( LoopSleepDelaySeconds / 8 ) ; i++)
  {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
  Serial.println("Loop done");  
}


long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADcdMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  
 
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
 
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
 
  long result = (high<<8) | low;
 
  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}

