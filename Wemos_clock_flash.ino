

// --- DESCRIPTION ---
// On the hour (NTP via WiFi) it flashes the LED & 
// GPIO pin the commensurate number of times to 
// drive an external clock load of some sort.
// The only bit I wrote is this comment block!
// Written by Gemini - it took to or three attempts 
// and was sorted out within fifteen minutes.
// ---------------------------


#include <ESP8266WiFi.h>  // Correct library for ESP8266
#include <time.h>         // Standard C time library
#include "sntp.h"         // SNTP configuration for ESP8266
#include <secrets.h>      // Your network credentials

// --- HARDWARE CONFIGURATION ---
// D4 corresponds to GPIO 2 on the ESP8266/D1 Mini.
#define PULSE_GPIO D4           // GPIO 2 (D4 on Wemos D1 Mini)
#define ONBOARD_LED PULSE_GPIO  // The onboard LED is also on D4

#define PULSE_DURATION_MS 2000  // 2 seconds high pulse
#define GAP_DURATION_MS 1000    // 1 second gap

// --- DEBUG CONFIGURATION ---
// Set to 'true' to run the pulse pattern every TEST_MODE_INTERVAL_SEC
const bool TEST_MODE = false; 
// The interval (in seconds) between test patterns (30s = 30000ms)
const unsigned long TEST_MODE_INTERVAL_SEC = 30; 
// ---------------------------




// --- TIME CONFIGURATION ---
const char* ntpServer = "pool.ntp.org";

// GMT (London) base offset is 0 hours (0 seconds).
// We use POSIX time rules to handle GMT/BST automatically.
// The "GMT0BST,M3.5.0/1,M10.5.0" string tells the ESP8266 when to switch.
const char* TZ_INFO = "GMT0BST,M3.5.0/1,M10.5.0";

// --- SCHEDULER STATE VARIABLES ---
unsigned long lastStateChangeTime = 0;
int currentPatternHour = -1;
int pulsesToExecute = 0;
int pulseCounter = 0;

// State machine states
enum PulseState
{
  STATE_IDLE,
  STATE_PULSE_HIGH,
  STATE_PULSE_LOW
};

PulseState currentState = STATE_IDLE;


// --- FUNCTION PROTOTYPES ---
void setup();
void loop();
void syncTime();
void handlePulsePattern();
int getCurrentHourIn24HourFormat();


// ---------------------------------------------------------------------
// --- INITIALIZATION AND SETUP ---
// ---------------------------------------------------------------------

void setup()
{
  Serial.begin(115200);

  // Set the pulse pin as output
  pinMode(PULSE_GPIO, OUTPUT);

  // Ensure both pin states are LOW/OFF initially
  digitalWrite(PULSE_GPIO, LOW);

  // --- Initial Wi-Fi connection attempt ---
  Serial.printf("\nConnecting to %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi Connected.");
    syncTime();
  }
  else
  {
    Serial.println("\nWiFi Failed. Running with unsynced time.");
  }
}


// ---------------------------------------------------------------------
// --- TIME AND SCHEDULING LOGIC ---
// ---------------------------------------------------------------------

void syncTime()
{
  // 1. Set the time zone rule (TZ_INFO handles GMT/BST switch)
  //configTzFile(TZ_INFO);
  configTzTime(TZ_INFO, ntpServer);

  // 2. Configure and synchronize SNTP
  sntp_setservername(0, (char*)ntpServer);
  sntp_init();

  Serial.println("Waiting for NTP time synchronization...");

  // Wait for the time to be set (up to 10 seconds)
  time_t now = time(nullptr);
  int attempts = 0;
  while (now < 100000 && attempts < 20)
  {
    delay(500);
    Serial.print("*");
    now = time(nullptr);
    attempts++;
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    Serial.println("\nTime synchronized successfully.");
    Serial.print("Current time (BST/GMT adjusted): ");
    Serial.println(asctime(&timeinfo));
  }
  else
  {
    Serial.println("\nFailed to obtain time from NTP after waiting.");
  }
}

int getCurrentHourIn24HourFormat()
{
  struct tm timeinfo;
  // getLocalTime uses the TZ rule set by configTzFile
  if (!getLocalTime(&timeinfo))
  {
    return -1;  // Time not yet synced
  }
  return timeinfo.tm_hour;  // Returns 0-23 (24-hour clock)
}

// ---------------------------------------------------------------------
// --- NON-BLOCKING PULSE HANDLER (FSM) ---
// ---------------------------------------------------------------------

void setOutputHigh()
{
  // The pulse pin is driven HIGH for the signal.
  digitalWrite(PULSE_GPIO, HIGH);

  // The onboard LED is Active LOW, so we write LOW to turn it ON.
  digitalWrite(ONBOARD_LED, LOW);
}

void setOutputLow()
{
  // The pulse pin is driven LOW for the gap/idle.
  digitalWrite(PULSE_GPIO, LOW);

  // The onboard LED is Active LOW, so we write HIGH to turn it OFF.
  digitalWrite(ONBOARD_LED, HIGH);
}


// void handlePulsePattern()
// {
//   unsigned long currentTime = millis();

//   switch (currentState)
//   {
//     case STATE_IDLE:
//       {
//         int currentHour = getCurrentHourIn24HourFormat();

//         // Check if the hour has changed AND the time is valid
//         if (currentHour != -1 && currentHour != currentPatternHour)
//         {

//           // Calculate the required number of pulses based on the hour (1-12)
//           int hour_12 = (currentHour % 12 == 0) ? 12 : (currentHour % 12);
//           pulsesToExecute = hour_12;

//           currentPatternHour = currentHour;
//           pulseCounter = 0;

//           Serial.printf("New hour detected: %02d:00. Scheduling %d pulses.\n", currentHour, pulsesToExecute);

//           // Transition to the first pulse
//           currentState = STATE_PULSE_HIGH;
//           lastStateChangeTime = currentTime;

//           // Turn pin HIGH and LED ON
//           setOutputHigh();
//           Serial.printf("PULSE #1: HIGH for %dms (LED ON).\n", PULSE_DURATION_MS);
//         }
//         break;
//       }

//     case STATE_PULSE_HIGH:
//       {
//         if (currentTime - lastStateChangeTime >= PULSE_DURATION_MS)
//         {
//           // Pulse is over, count it and transition to gap
//           pulseCounter++;

//           // Turn pin LOW and LED OFF
//           setOutputLow();
//           lastStateChangeTime = currentTime;
//           currentState = STATE_PULSE_LOW;

//           // Log the state change
//           if (pulseCounter < pulsesToExecute)
//           {
//             Serial.printf("PULSE #%d: LOW for %dms (LED OFF).\n", pulseCounter, GAP_DURATION_MS);
//           }
//         }
//         break;
//       }

//     case STATE_PULSE_LOW:
//       {
//         // Check if gap is over
//         if (currentTime - lastStateChangeTime >= GAP_DURATION_MS)
//         {

//           if (pulseCounter < pulsesToExecute)
//           {
//             // Start next pulse
//             setOutputHigh();
//             lastStateChangeTime = currentTime;
//             currentState = STATE_PULSE_HIGH;
//             Serial.printf("PULSE #%d: HIGH for %dms (LED ON).\n", pulseCounter + 1, PULSE_DURATION_MS);
//           }
//           else
//           {
//             // Pattern complete, return to idle
//             Serial.printf("Pattern complete for hour %02d. Total pulses: %d. Idling (LED OFF).\n", currentPatternHour, pulsesToExecute);
//             currentState = STATE_IDLE;
//           }
//         }
//         break;
//       }
//   }
// }

void handlePulsePattern() {
    unsigned long currentTime = millis();
    static unsigned long lastTestTriggerTime = 0; // Tracks the last trigger time in test mode

    switch (currentState) {
        case STATE_IDLE: {
            
            int newPulsesToExecute = 0;
            bool trigger = false;
            
            if (TEST_MODE) {
                // --- A. TEST MODE LOGIC ---
                if (currentTime - lastTestTriggerTime >= (TEST_MODE_INTERVAL_SEC * 1000UL)) {
                    // In Test Mode, we will loop through pulse counts 1 to 12 sequentially.
                    lastTestTriggerTime = currentTime;
                    currentPatternHour = (currentPatternHour + 1) % 12; // Cycle 0 through 11
                    newPulsesToExecute = (currentPatternHour == 0) ? 12 : currentPatternHour;
                    trigger = true;
                    Serial.printf("\n*** TEST MODE TRIGGER ***. Simulating hour: %d.\n", newPulsesToExecute);
                }
            } else {
                // --- B. PRODUCTION MODE LOGIC ---
                int currentHour = getCurrentHourIn24HourFormat();

                if (currentHour != -1 && currentHour != currentPatternHour) {
                    // Calculate the required number of pulses based on the hour (1-12)
                    int hour_12 = (currentHour % 12 == 0) ? 12 : (currentHour % 12);
                    currentPatternHour = currentHour; // Store the actual 24hr clock time
                    newPulsesToExecute = hour_12;
                    trigger = true;
                    Serial.printf("\n*** PRODUCTION TRIGGER ***. New hour detected: %02d:00.\n", currentHour);
                }
            }
            
            // --- C. COMMON TRIGGER ACTION ---
            if (trigger) {
                pulsesToExecute = newPulsesToExecute;
                pulseCounter = 0;
                
                Serial.printf("Scheduling %d pulses.\n", pulsesToExecute);
                
                // Transition to the first pulse
                currentState = STATE_PULSE_HIGH;
                lastStateChangeTime = currentTime;
                
                // Turn pin HIGH and LED ON
                setOutputHigh(); 
                Serial.printf("PULSE #1: HIGH for %dms (LED ON).\n", PULSE_DURATION_MS);
            }
            break;
        }
        
        // --- STATE_PULSE_HIGH and STATE_PULSE_LOW remain unchanged ---
        case STATE_PULSE_HIGH: {
            if (currentTime - lastStateChangeTime >= PULSE_DURATION_MS) {
                pulseCounter++;
                setOutputLow();
                lastStateChangeTime = currentTime;
                currentState = STATE_PULSE_LOW;
                
                if (pulseCounter < pulsesToExecute) {
                    Serial.printf("PULSE #%d: LOW for %dms (LED OFF).\n", pulseCounter, GAP_DURATION_MS);
                }
            }
            break;
        }

        case STATE_PULSE_LOW: {
            if (currentTime - lastStateChangeTime >= GAP_DURATION_MS) {
                
                if (pulseCounter < pulsesToExecute) {
                    setOutputHigh();
                    lastStateChangeTime = currentTime;
                    currentState = STATE_PULSE_HIGH;
                    Serial.printf("PULSE #%d: HIGH for %dms (LED ON).\n", pulseCounter + 1, PULSE_DURATION_MS);
                } else {
                    Serial.printf("Pattern complete for hour %02d. Total pulses: %d. Idling (LED OFF).\n", currentPatternHour, pulsesToExecute);
                    currentState = STATE_IDLE;
                }
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------
// --- MAIN LOOP ---
// ---------------------------------------------------------------------

void loop()
{
  // Keep Wi-Fi connected
  if (WiFi.status() != WL_CONNECTED)
  {
    // Attempt to reconnect every time the loop runs, which is fast and non-blocking
    WiFi.reconnect();
  }

  // The core of the scheduling logic, runs every loop
  handlePulsePattern();

  // Allow system tasks (like Wi-Fi and SNTP) to run
  yield();
}