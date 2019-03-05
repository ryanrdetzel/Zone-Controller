#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>

#include "Adafruit_MCP23017.h"

#define NUM_ZONES 1

Adafruit_MCP23017 mcp;
byte lastInputUpdate = 0;

unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 500;           // interval at which to blink (milliseconds)
byte blinkState = LOW;

bool overrideActive = false;
unsigned long last_output = 0;

struct Zone {
  bool zoneOn;
  bool zoneEnabled;
  byte ledRedPin;
  byte ledGreenPin;
  byte relayPin;
  byte buttonPin;
} zone1;

Zone zones[NUM_ZONES];

void setupZone(struct Zone zone);

void setup() {
  Serial.begin(9600);
  Serial.write("Startup\n");

  zone1 = {
    zoneOn: false,
    zoneEnabled: false,
    ledRedPin: 15, // not real pin #
    ledGreenPin: 14, // not real pin #
    relayPin: 14, // on esp
    buttonPin: 0  // not real pin #
  };

  zones[0] = zone1;

  // Reset mcp
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW);
  delay(10);
  digitalWrite(0, HIGH);

  Wire.begin(4, 5); //i2c

  mcp.begin();
  
  for (int zIndex=0;zIndex<NUM_ZONES;zIndex++){
    Zone zone = zones[zIndex];
    setupZone(zone);
  }

  // Override switch
  mcp.pinMode(4, INPUT);
  mcp.pullUp(4, HIGH); 

  lastDebounceTime = millis();
}

void setupZone(struct Zone zone){
  /* Setup LEDs */
  mcp.pinMode(zone.ledRedPin, OUTPUT);
  mcp.digitalWrite(zone.ledRedPin, HIGH);

  mcp.pinMode(zone.ledGreenPin, OUTPUT);
  mcp.digitalWrite(zone.ledGreenPin, HIGH);

  /* Setup relay */
  digitalWrite(zone.relayPin, LOW);
  pinMode(zone.relayPin, OUTPUT);

  // mcp.pinMode(zone.relayPin, OUTPUT);
  // mcp.digitalWrite(zone.relayPin, HIGH);

  /* Setup button */
  mcp.pinMode(zone.buttonPin, INPUT);
  mcp.pullUp(zone.buttonPin, HIGH); 
}

void processZone(struct Zone zone){
  byte ledRed = HIGH;
  byte ledGreen = HIGH;

  if (zone.zoneEnabled == false){
    zone.zoneOn = false;
  } else{
    if (overrideActive){
      zone.zoneOn = true; // force zone to "on" if we're in override mode and not disabled
    }
  }

  /* Update LED to reflect state */
  switch(zone.zoneEnabled){
    case false: ledRed = LOW; ledGreen = HIGH; break;
    case true: 
      ledRed = HIGH; 
      ledGreen = LOW;
      if (zone.zoneOn){
        ledGreen = blinkState;
      }
    break;
  }
  mcp.digitalWrite(zone.ledRedPin, ledRed);
  mcp.digitalWrite(zone.ledGreenPin, ledGreen);

  // Update relay
  if (zone.zoneOn){
    digitalWrite(zone.relayPin, HIGH);
  } else {
    digitalWrite(zone.relayPin, LOW);
  }
}

void toggleZoneState(struct Zone zone){
  // Toggle if the zone is on/off
  if (overrideActive) return;  // Only manual overrides
}

byte buttonToBit(struct Zone zone){
  switch(zone.buttonPin){
    case 0: return 1; break;
    case 1: return 2; break;
    case 2: return 4; break;
    case 3: return 8; break;
  }
  return 0;
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    blinkState = !blinkState;
  }

  //Read inputs
  byte allInputs = mcp.readGPIO(0);

  // Do simple check vs the current
  overrideActive = ((allInputs >> 4 & 1) == 0);

  if (allInputs != lastInputUpdate){
    if ((millis() - lastDebounceTime) > debounceDelay) {
      // Check if the pin that changed was a zone pine
      for (int zIndex=0;zIndex<NUM_ZONES;zIndex++){
        Zone zone = zones[zIndex];
        byte buttonPinBit = buttonToBit(zone); // inputs start on pin 21
        if (~allInputs & buttonPinBit){
          zone.zoneEnabled = !zone.zoneEnabled;
          Serial.write("Change\n");
        }
        zones[zIndex] = zone;
      }

      lastInputUpdate = allInputs;
      lastDebounceTime = millis();
    }
  }

  for (int zIndex=0;zIndex<NUM_ZONES;zIndex++){
    Zone zone = zones[zIndex];
    processZone(zone);
  }

  if (millis() - last_output > 1000) // compare diff of now vs. timer 
  {    
    uint32_t free = system_get_free_heap_size(); // get free ram   
    Serial.print(last_output);
    Serial.println(free); // output ram to serial   
    last_output = millis(); // reset timer
  }
}
