#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>
#include <Servo.h>

// Hardware Pin Definitions
#define RST_PIN 9
#define SS_PIN 10
#define PIR_PIN 2
#define SERVO_PIN 6

// Initialize Objects
MFRC522 rfid(SS_PIN, RST_PIN);
// Raw LCD mapping: RS, EN, D4, D5, D6, D7
LiquidCrystal lcd(A0, A1, A2, A3, A4, A5);
Servo deadbolt;

// Authorized Master Key (Update this hex array with your physical tag's actual UID later)
byte masterUID[4] = {0xCB, 0xE, 0x4F, 0x7};

// State Machine Enums
enum SystemState {
  IDLE_STATE,
  AWAKE_STATE,
  AUTH_STATE,
  ACTION_STATE
};

volatile SystemState currentState = IDLE_STATE;

// Hardware Interrupt Service Routine for the PIR Sensor
void wakeUp() {
  if (currentState == IDLE_STATE) {
    currentState = AWAKE_STATE;
  }
}

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  
  // Initialize the 16x2 raw LCD
  lcd.begin(16, 2);
  
  deadbolt.attach(SERVO_PIN);
  deadbolt.write(0); // Set to initial locked position
  
  pinMode(PIR_PIN, INPUT);
  // Attach the hardware interrupt to Digital Pin 2 (RISING edge)
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), wakeUp, RISING);
  
  Serial.println("System Initialized. Waiting for motion...");
}

void loop() {
  switch (currentState) {
    
    case IDLE_STATE:
      // System asleep, clear screen, wait for PIR hardware interrupt
      lcd.clear();
      // Since the raw LCD backlight is hardwired to 5V, the screen will glow empty
      break;
      
    case AWAKE_STATE:
      // PIR triggered, prompt user
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Scan ID Card...");
      currentState = AUTH_STATE;
      break;
      
    case AUTH_STATE:
      // Actively wait and check for an RFID tag
      if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        
        bool accessGranted = true;
        
        // Loop through the 4 bytes of the scanned card's UID to verify
        for (byte i = 0; i < 4; i++) {
          if (rfid.uid.uidByte[i] != masterUID[i]) {
            accessGranted = false; // Mismatch found
            break;                 // Stop checking immediately
          }
        }
        
        // Print the scanned UID to the Serial Monitor so you can find your real tag's ID
        Serial.print("Scanned UID: ");
        for (byte i = 0; i < 4; i++) {
          Serial.print(rfid.uid.uidByte[i], HEX);
          Serial.print(" ");
        }
        Serial.println();
        
        if (accessGranted) {
          Serial.println("Authorized Key Detected.");
          currentState = ACTION_STATE;
        } else {
          Serial.println("WARNING: Unauthorized Access Attempt.");
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("ACCESS DENIED");
          delay(2000); // Display warning for 2 seconds
          currentState = IDLE_STATE; // Reset back to sleep
        }
        
        // Command the RFID module to stop reading this specific card to prevent spam
        rfid.PICC_HaltA(); 
      }
      break;
      
    case ACTION_STATE:
      // Update UI for successful authentication
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Access Granted");
      lcd.setCursor(0,1);
      lcd.print("Unlocking...");
      
      // Actuate the deadbolt
      deadbolt.write(90); 
      delay(3000); // Hold open for 3 seconds
      
      // Lock it back up
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Locking...");
      deadbolt.write(0);  
      delay(1000); // Give the mechanical servo time to travel back
      
      // Reset the system
      currentState = IDLE_STATE; 
      break;
  }
}
