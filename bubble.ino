#include "Particle.h"

//Declare the variables
const size_t SCAN_RESULT_MAX = 30; //Set a constant to 30, which is the max number of devices detected in the general scan
const uint8_t NAME = 0xCC; //Create the constant that sets the name of each device (this is changed for each particle flashed, protoype will be either AA, BB or CC)

int speakerPin = A4; //The pinused on the Particle
unsigned long interval = 1000;
unsigned long previousMillis = 0;

#define NOTE  716 //Note that will be used by the speaker
#define NOTEDURATION 1000 //How long the buzzer will sound for
#define NOTEDURATIONDELAY 1000 //How long the buzzer will pause for

//Define the structure that will be broadcast by the particle devices
struct companyDevice {
   uint8_t  name; //Name of device
   int   rssi; //Distance of device when detected
}; 

struct companyDevice devicesInBubble[10]; //Empty structure to fill with devices currently in close proximity
struct companyDevice temporaryDetectedDevices[10]; //Empty structure list that will be filled with devices that need to be checked with the devicesInBubble list

BleScanResult scanResult; //Define the variable scanResult from the pre-defined bluetooth result variable type
BleScanResult scanResults[SCAN_RESULT_MAX]; //Make scanResults a empty list of 30

void setup() {
    Serial.begin(9600); //Initialise serial output
    BLE.on(); //Turn Bluetooth on
    setAdvertisingData(); //Run the advertising data broadcasting function
}

void setAdvertisingData() { //Function to set up particle for broadcasting of each particle 
    uint8_t buf[BLE_MAX_ADV_DATA_LEN]; //the empty string that will be filled with the data that will be broadcast
    BleAdvertisingData advData; //Declare the advertising data variable
    
    buf[0] = 0xFE; //Setting the company ID (All devices will use these values for general identification)
    buf[1] = 0x13;  //Setting the company ID (All devices will use these values for general identification)
    buf[2] = NAME;//Setting the unique name for each device
    advData.appendCustomData(buf, 3); //add the custom data to the advertising data variable
    
    BLE.setAdvertisingInterval(50); //Set the frequency that the data will be advertised 
    BLE.advertise(&advData); //Set what will be advertised
}

void scanForDevices() { //Function for the Bluetooth scan
    //Clear the temporary detected devices list, ready for a new scan
    for (int i = 0; i < 10; i++) {
        temporaryDetectedDevices[i].name = 0x00;
        temporaryDetectedDevices[i].rssi = 0;
    }
   
    BLE.setScanTimeout(100);  // Scan for devices for 100 milliseconds
   
    int count = BLE.scan(scanResults, SCAN_RESULT_MAX);  //Populate the ScanResults string with the scanned results up to a maximum of 30, and set this value equal to count. 
    int companyDeviceCount = 0; //Set the counter to 0
    
    for (int i = 0; i < count; i++) { //Check each item in the general scanned list if it is a device of interest (with the id of 0xFE and 0x13)
        
        scanResult = scanResults[i]; //set the scanResult to be the i iteration in the list
        
        uint8_t buf[BLE_MAX_ADV_DATA_LEN]; //Make empty list with max number of entries that will be filled with devices of interest
        size_t len; //decalre empty integer variable that will store the length of the new interest list
        //Populating the list with the manufacturing data advertised for each device that will then be filtered
        len = scanResults[i].advertisingData.get(BleAdvertisingDataType::MANUFACTURER_SPECIFIC_DATA, buf, BLE_MAX_ADV_DATA_LEN);
        
        if (len == 3) { //Filter for BLE devices with 3 bytes in their advertising data (specific for bubble devides)
            if (buf[0] == 0xFE && buf[1] == 0x13) { //filter the general list for ones of interest by the "company name"
                if (scanResult.rssi >= -65) { //filter for devices in a certain range and populate the temp list of devices
                    temporaryDetectedDevices[companyDeviceCount].name = buf[2];
                    temporaryDetectedDevices[companyDeviceCount].rssi = scanResult.rssi;
                    //Serial.println(temporaryDetectedDevices[companyDeviceCount].name);
                    //Serial.println(temporaryDetectedDevices[companyDeviceCount].rssi);
                    companyDeviceCount++;
                }
            }
        }
    }
}

void checkForNewDevices() { //Updating the main bubble list with new devicves detected in the temp device list
    for (int i = 0; i < 10; i++) { //Looking through the bubble list which is a maximum of 10 here (would need increased if more particles added)
        for (int j = 0; j < 10; j++) { 
            if (temporaryDetectedDevices[i].name == devicesInBubble[j].name) { //check the name is the same, if it is breack and go next
               break; //return to next for loop iteration 
            }
            if (j == 9) { //checking the old list has been fully checked before accepting the name not being in the list
                
                char detectedDeviceID[10];
                sprintf(detectedDeviceID, "%X", temporaryDetectedDevices[i].name);
                char publishResults[30];
                sprintf(publishResults, "%X,contact,%X", NAME, temporaryDetectedDevices[i].name);
                //print additions to the list to the command line
                Serial.print("Adding entry - ");
                Serial.print(detectedDeviceID);
                Serial.print(" at ");
                Serial.println(Time.timeStr().c_str());
                //Serial.println(detectedDeviceID);
                //Serial.println(publishResults);
                //publish the results
                Particle.publish("BubbleIntegration", publishResults, PRIVATE);
                for (int k = 0; k < 10; k++) { //Check the old list for a space and add in new entry when found (fills left to right)
                    if (devicesInBubble[k].name == 0x00) {
                        devicesInBubble[k].name = temporaryDetectedDevices[i].name; //populate main list with temp list entry
                        break;
                    }
                    
                }
            }
        }
    }
}

void removeOldDevices() { //Updating the main bubble list by removing any that are no longer in the temp device list
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            if (devicesInBubble[i].name == temporaryDetectedDevices[j].name) {
               break;
            }
            if (j == 9) {
                //Publish the stop time *********
                char detectedDeviceID[10];
                sprintf(detectedDeviceID, "%X", devicesInBubble[i].name);
                char publishResults[30];
                sprintf(publishResults, "%X,separate,%X", NAME, devicesInBubble[i].name);
                //print removals to the command line
                Serial.print("Deleting entry - ");
                Serial.print(detectedDeviceID);
                Serial.print(" at ");
                Serial.println(Time.timeStr().c_str());;
                //publish the results
                Particle.publish("BubbleIntegration", publishResults, PRIVATE);
                devicesInBubble[i].name = 0x00; //remove device from the list
            }
        }
    }
}

void printDetectedDevices() { //funciton to print the devices that are within 2m (from the main list)
    for (int i =0; i < 10; i++) {
        Serial.print(devicesInBubble[i].name);
        Serial.print(",");
    }
    Serial.println();
}

void soundTheAlarm () {
    for (int i = 0; i < 10; i++) {
        if (devicesInBubble[i].name != 0x00) {
            tone(speakerPin, NOTE, NOTEDURATION); 
            delay(NOTEDURATIONDELAY);
            break;
        }
    }
}

void loop() { 
    unsigned long currentMillis = millis();
    scanForDevices();
    checkForNewDevices();
    removeOldDevices();
    soundTheAlarm();
    printDetectedDevices();
    delay(500);
}

