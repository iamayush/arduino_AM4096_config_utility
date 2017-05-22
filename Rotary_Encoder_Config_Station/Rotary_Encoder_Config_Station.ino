#include <Wire.h>
#include <avr/wdt.h>
#include "Definitions.h"



void setup() {
  
  Wire.begin();
  Wire.setClock(400000);
  Serial.begin(115200);

  // Reset the Arduino if it hangs for more than 2s
  wdt_enable(WDTO_2S);
  
  Serial.println();
  Serial.println("Reboot, please reconnect any devices (I2C might be blocked)");
  Serial.println();
  Serial.println("AM4096 Rotary Encoder Config Utility");
  Serial.println();
}


byte r[2];
bool checkDeviceAvailable(int addr){
    return readMemory(addr, 32, r); // Read a register (fast)
}

uint8_t searchAddressSpace(){
  byte r[2];
  for(uint8_t i=0; i < 128; i++){ // search all possible addresses
    if(checkDeviceAvailable(i)){
      Serial.print("Device found at addr: ");
      Serial.println(i);
      return i;
    }
  }
  return 255; // invalid address (valid: 0 - 127)
}




void loop() {
  wdt_reset();

  // No device connected
  if(device_addr > 127){
    delay(1000);
    Serial.println("Searching");
    device_addr = searchAddressSpace();
    device_settings_read = false;
  }else{
    if(checkDeviceAvailable(device_addr)){
      // Device connected

      if(!device_settings_read){
        readDeviceSettings(device_addr);
        device_settings_read = true;
        displayDeviceProperties();
        Serial.println("Waiting for input... (h for help)");
      }

      parseCommand();

      delay(1);
      
    }else{
      device_addr = 255;
      Serial.println("Connection lost");
      Serial.println();
    }
  }
}


String commandIn = "";
bool line = false;
void parseCommand(){
  while (Serial.available()) {
      char c = Serial.read();
      if(c == '\n'){
        line = true;
        break;
      }
      commandIn += c;
  }
  if(line){
    line = false;
    Serial.println();
    Serial.print(">  ");
    Serial.println(commandIn);

    // Display status
    if(commandIn == "" || commandIn == "t"){
      if(!readDeviceStatus(device_addr)){
        Serial.println("Error reading device status");
        device_settings_read = false;
      }else{
        displayDeviceStatus();
      }
    }

    // help
    else if(commandIn == "h"){
      Serial.println("Commands available: (line end with \\n)");
      Serial.println("h       -- Show this help");
      Serial.println("s       -- Display current device settings");
      Serial.println("reg (-i)-- Display current raw register value (with comparison to default)");
      Serial.println("def     -- Apply the default settings");
      Serial.println("a <val> -- Change the Address");
      Serial.println("z <val> -- Set a zero-value");
      Serial.println("r       -- Flip the Rotation sign");
      Serial.println("t       -- read the current state for testing");
      Serial.println("(or blank)");
    }
    
    // trigger reload of device settings
    else if(commandIn == "s"){
      device_settings_read = false;
    }

    // flip rotation & write setting
    else if(commandIn == "r"){
      device_settings[1][0] ^= 0b10000;
      if(!writeMemoryCheck(device_addr, 1, device_settings[1])){
        Serial.println("Error flipping rotation");
        device_settings_read = false;
      }
      else{
        Serial.print("Success: Rotation now ");
        Serial.println((device_settings[1][0] & 0b10000) ? "Counter-Clockwise" : "Clockwise");
      }
    }

    // display raw register
    else if(commandIn.startsWith("reg")){
      char buf[30];
      // display raw register with additional info
      if(commandIn == "reg -i"){
        Serial.println("Current register values, default values, default mask:");
        for(uint8_t i=0; i < 4; i++){
          sprintf(buf, "%02i: %02x %02x   %02x %02x   %02x %02x", i,
            device_settings[i][0], device_settings[i][1],
            default_settings[i][0], default_settings[i][1],
            default_settings_mask[i][0], default_settings_mask[i][1]);
          Serial.println(buf);
        }
      }else{
        Serial.println("Current register values:");
        for(uint8_t i=0; i < 4; i++){
          sprintf(buf, "%02i: %02x %02x", i, device_settings[i][0], device_settings[i][1]);
          Serial.println(buf);
        }
      }
    }

    // write default settings
    else if(commandIn == "def"){
      if(!writeDefaultSettings(device_addr)){
        Serial.println("Error writing default settings");
      }
      else{
        Serial.println("Success: Default settings written");
      }
      device_settings_read = false;
    }

    // write zero value
    else if(commandIn.startsWith("z ")){
      uint16_t zero = commandIn.substring(2).toInt() & 0xfff; // 12 bit value max
      
      device_settings[1][0] = (device_settings[1][0] & 0xf0) | ((zero >> 8) & 0xf);
      device_settings[1][1] = zero & 0xff;
      if(!writeMemoryCheck(device_addr, 1, device_settings[1])){
        Serial.println("Error writing Zero Value rotation");
        device_settings_read = false;
      }
      else{
        Serial.print("Success: Zero value now ");
        Serial.println(zero);
      }
    }

    // write address
    else if(commandIn.startsWith("a ")){
      uint8_t addr = commandIn.substring(2).toInt() & 0b01111111; // 7 bit value max
      
      device_settings[0][1] = (device_settings[0][1] & 0b10000000) | addr;
      if(!writeMemory(device_addr, 0, device_settings[0])){
        // No writeMemoryCheck here since address changes
        Serial.println("Error giving new address");
        device_settings_read = false;
      }
      else{
        Serial.print("Success: Device address now ");
        Serial.println(addr);
        device_addr = addr;
      }
      delay(30);
    }

    
    else{
      Serial.println("Unknown command");
      Serial.println("type h for help");
    }
    
    commandIn = "";
  }
}


