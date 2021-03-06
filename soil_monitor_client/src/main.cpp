#include <Arduino.h>
#include "math.h"
/**
 * A BLE client example that is rich in capabilities.
 * There is a lot new capabilities implemented.
 * author unknown
 * updated by chegewaraAAA
 */

#include "BLEDevice.h"
//#include "BLEScan.h"

#include <HardwareSerial.h>
#include <esp_task_wdt.h>
#include <string>  
#include <stdint.h>

#define FRDM_SERIAL_TX_PIN ( 17 )
#define FRDM_SERIAL_RX_PIN ( 16 )
#define FRDM_SERIAL_MODE ( SERIAL_8N1 )
#define FRDM_SERIAL_BAUD ( 4800 )
#define MAX_NUMBER_OF_SENSORS ( 2 )

#define SENSOR_TIMEOUT_S ( 2 )
#define WDT_TIMEOUT 6


//#define PRINT_ALL_DEVICES

typedef enum eSensorValueType
{
  eSENSOR_VALUE_CAPACITANCE,
  eSENSOR_VALUE_TEMPERATURE,

  // new sensor value types above this line
  eSENSOR_VALUE_COUNT,
}eSENSOR_VALUE_TYPE;

typedef struct xSensorData
{
  const std::string strDevName;
  uint16_t u16Capacitance;
  bool boIsNewData;
}xSENSOR_DATA;

static xSENSOR_DATA xSensorData[MAX_NUMBER_OF_SENSORS] =
{
  {
    "Soil Sensor 1",
    0,
    false,
  },
    {
    "Soil Sensor 2",
    0,
    false,
  }
};


static HardwareSerial FRDMSerialComm(1); // use UART 2, 

static boolean doScan = false;
static uint8_t u8SensorIdx;

static BLEAdvertisedDevice* myDevice;
static BLEScan* pBLEScan;
static const char * pCharAdvData;

//static xSENSOR_READING_PACKET

static void scanCompleteCB(BLEScanResults scanResults);

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.println((char*)pData);
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */

  void onResult(BLEAdvertisedDevice advertisedDevice) {
    int i = 0;
    uint16_t u16CapVal = 0;

#ifdef PRINT_ALL_DEVICES
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
#endif

    // We have found a device, let's see if it's the device name we're looking for
    if( xSensorData[ u8SensorIdx % MAX_NUMBER_OF_SENSORS ].strDevName == advertisedDevice.getName() )
    {
      xSensorData[u8SensorIdx % MAX_NUMBER_OF_SENSORS].u16Capacitance = 0;
      pCharAdvData = advertisedDevice.getManufacturerData().data(); // why is our data prepended with other data?

      Serial.print("Sensor ");
      Serial.print( u8SensorIdx % MAX_NUMBER_OF_SENSORS ); 
      Serial.print( ": " ); 

      pCharAdvData += 2;

      while( *pCharAdvData != 0x00 )
      {
        // loop until finding null character
        // Serial.print("raw byte: ");
        // Serial.println( *pCharAdvData, HEX ); 
        //Serial.println( *pCharAdvData - 0x30, HEX);
        
        u16CapVal = (u16CapVal * 10) + (*pCharAdvData - 0x30);
        // Serial.print("converted so far: ");
        // Serial.println(u16CapVal);
        pCharAdvData++;
      }

      xSensorData[u8SensorIdx % MAX_NUMBER_OF_SENSORS].u16Capacitance = u16CapVal;
      xSensorData[u8SensorIdx % MAX_NUMBER_OF_SENSORS].boIsNewData = true;

      Serial.println(xSensorData[u8SensorIdx % MAX_NUMBER_OF_SENSORS].u16Capacitance);
      pBLEScan->stop();
      doScan = true;

      xSensorData[u8SensorIdx % MAX_NUMBER_OF_SENSORS].boIsNewData = true;
    }
  }
};

void scanCompleteCB(BLEScanResults scanResults) 
{
    Serial.print("Timed out looking for sensor ");
    Serial.println( ( u8SensorIdx % MAX_NUMBER_OF_SENSORS ), DEC );
    doScan = true;
}

void setup() {
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  Serial.begin( 115200 );
  FRDMSerialComm.begin( FRDM_SERIAL_BAUD, FRDM_SERIAL_MODE, FRDM_SERIAL_RX_PIN, FRDM_SERIAL_TX_PIN );

  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(SENSOR_TIMEOUT_S, &scanCompleteCB, false);

  u8SensorIdx = 1;
} // End of setup.


// This is the Arduino main loop function.
void loop() {
  uint8_t u8TxBuf[8];
  int i;

  for( i = 0; i < MAX_NUMBER_OF_SENSORS; i++ )
  {
    if( xSensorData[i].boIsNewData == true )
    {
      u8TxBuf[0] = ( u8SensorIdx % MAX_NUMBER_OF_SENSORS );
      u8TxBuf[1] = eSENSOR_VALUE_CAPACITANCE;
      u8TxBuf[2] = ( ( xSensorData[i].u16Capacitance & 0xFF00 ) >> 8 );
      u8TxBuf[3] = ( uint8_t( xSensorData[i].u16Capacitance & 0x00FF ) );

      FRDMSerialComm.write( u8TxBuf, 8 );

      xSensorData[i].boIsNewData = false;
    }
  }

  if( doScan == true )
  {
    u8SensorIdx++;
    esp_task_wdt_reset();
    pBLEScan->start(SENSOR_TIMEOUT_S, &scanCompleteCB, false);
  }

  esp_task_wdt_reset();
  Serial.print(".");
  delay(100); // Delay a half second between loops.
} // End of loop