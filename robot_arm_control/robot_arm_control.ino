#include <Braccio.h>
#include <Servo.h>

// Servo objects for each motor
Servo base;        // M1
Servo shoulder;    // M2
Servo elbow;      // M3
Servo wrist_rot;  // M4
Servo wrist_ver;  // M5
Servo gripper;    // M6

// Current position storage
struct Position {
  int base;       // M1: 0-180
  int shoulder;   // M2: 15-165
  int elbow;      // M3: 0-180
  int wrist_ver;  // M4: 0-180
  int wrist_rot;  // M5: 0-180
  int gripper;    // M6: 10-73
} currentPos;

// Preset positions
const Position HOME_POSITION = {90, 45, 180, 180, 90, 10};
const Position READY_POSITION = {90, 90, 90, 90, 90, 10};
const Position PICK_POSITION = {0, 90, 90, 90, 90, 10};
const Position PLACE_POSITION = {180, 90, 90, 90, 90, 10};

// Function declarations
void moveToPosition(Position pos, int stepDelay = 20);
void pickAndPlace(int pickBase, int placeBase);
void wave();
void scan();

void setup() {
  Serial.begin(9600);  // Start serial communication
  
  // Initialize the Braccio
  Braccio.begin();
  
  // Move to home position and store it as current position
  moveToPosition(HOME_POSITION);
  currentPos = HOME_POSITION;
  
  printMenu();
}

void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    executeCommand(command);
  }
}

void printMenu() {
  Serial.println("\n=== Braccio Robot Control Menu ===");
  Serial.println("h - Move to home position");
  Serial.println("r - Move to ready position");
  Serial.println("p - Start pick and place sequence");
  Serial.println("w - Wave gesture");
  Serial.println("s - Scan area");
  Serial.println("g - Open/Close gripper");
  Serial.println("m - Enter manual control mode");
  Serial.println("? - Show this menu");
}

void executeCommand(char command) {
  switch (command) {
    case 'h':
      Serial.println("Moving to home position...");
      moveToPosition(HOME_POSITION);
      break;
      
    case 'r':
      Serial.println("Moving to ready position...");
      moveToPosition(READY_POSITION);
      break;
      
    case 'p':
      Serial.println("Starting pick and place sequence...");
      pickAndPlace(0, 180);  // Full range pick and place
      break;
      
    case 'w':
      Serial.println("Waving...");
      wave();
      break;
      
    case 's':
      Serial.println("Scanning area...");
      scan();
      break;
      
    case 'g':
      toggleGripper();
      break;
      
    case 'm':
      manualControl();
      break;
      
    case '?':
      printMenu();
      break;
      
    default:
      Serial.println("Unknown command. Press ? for menu.");
  }
}

void moveToPosition(Position pos, int stepDelay) {
  // Ensure step delay is within allowed range (10-30ms)
  stepDelay = constrain(stepDelay, 10, 30);
  
  // Move to position using ServoMovement
  Braccio.ServoMovement(stepDelay, pos.base, pos.shoulder, pos.elbow, 
                       pos.wrist_ver, pos.wrist_rot, pos.gripper);
                       
  // Update current position
  currentPos = pos;
  delay(50);  // Small delay to ensure movement completion
}

void pickAndPlace(int pickBase, int placeBase) {
  // Starting position
  Position pickup = READY_POSITION;
  pickup.base = pickBase;
  pickup.gripper = 73;  // Open gripper
  
  moveToPosition(pickup);
  delay(1000);
  
  // Close gripper
  pickup.gripper = 10;
  moveToPosition(pickup);
  delay(1000);
  
  // Lift
  pickup.shoulder = 45;
  moveToPosition(pickup);
  
  // Move to place position
  Position place = pickup;
  place.base = placeBase;
  moveToPosition(place);
  
  // Open gripper
  place.gripper = 73;
  moveToPosition(place);
  
  // Return to ready position
  moveToPosition(READY_POSITION);
}

void wave() {
  // Move to wave starting position
  Position wavePos = READY_POSITION;
  wavePos.base = 90;      // Center position
  wavePos.shoulder = 45;  // Lift arm up
  wavePos.elbow = 180;    // Extend arm
  wavePos.wrist_ver = 90; // Neutral wrist position
  wavePos.wrist_rot = 90; // Neutral wrist rotation
  moveToPosition(wavePos);
  delay(500);

  // Perform waving motion
  for (int i = 0; i < 4; i++) {
    // Wave up
    wavePos.wrist_ver = 45;
    moveToPosition(wavePos, 10);
    delay(200);
    
    // Wave down
    wavePos.wrist_ver = 135;
    moveToPosition(wavePos, 10);
    delay(200);
  }
  
  // Return to ready position
  moveToPosition(READY_POSITION);
}

void scan() {
  Position scanPos = READY_POSITION;
  
  // Scan from left to right
  for (int baseAngle = 0; baseAngle <= 180; baseAngle += 45) {
    scanPos.base = baseAngle;
    moveToPosition(scanPos);
    delay(500);
  }
  
  moveToPosition(READY_POSITION);
}

void toggleGripper() {
  Position newPos = currentPos;
  newPos.gripper = (currentPos.gripper <= 30) ? 73 : 10;
  moveToPosition(newPos);
}

void manualControl() {
  Serial.println("\nManual Control Mode");
  Serial.println("Enter commands in format: 'joint angle'");
  Serial.println("Joints: 1=base, 2=shoulder, 3=elbow, 4=wrist_ver, 5=wrist_rot, 6=gripper");
  Serial.println("Example: '1 90' sets base to 90 degrees");
  Serial.println("Ranges:");
  Serial.println("1: Base (0-180)");
  Serial.println("2: Shoulder (15-165)");
  Serial.println("3: Elbow (0-180)");
  Serial.println("4: Wrist Vertical (0-180)");
  Serial.println("5: Wrist Rotation (0-180)");
  Serial.println("6: Gripper (10-73)");
  Serial.println("Enter 'x' to exit manual mode");
  
  while (true) {
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      input.trim();  // Remove any whitespace
      
      if (input == "x") {
        Serial.println("Exiting manual mode");
        break;
      }
      
      // Parse joint number and angle
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex == -1) {
        Serial.println("Invalid input format. Use 'joint angle'");
        continue;
      }
      
      int joint = input.substring(0, spaceIndex).toInt();
      int angle = input.substring(spaceIndex + 1).toInt();
      
      Position newPos = currentPos;
      bool validInput = true;
      
      // Input validation and position update
      switch (joint) {
        case 1:
          if (angle >= 0 && angle <= 180) {
            newPos.base = angle;
            Serial.println("Moving base to " + String(angle));
          } else validInput = false;
          break;
        case 2:
          if (angle >= 15 && angle <= 165) {
            newPos.shoulder = angle;
            Serial.println("Moving shoulder to " + String(angle));
          } else validInput = false;
          break;
        case 3:
          if (angle >= 0 && angle <= 180) {
            newPos.elbow = angle;
            Serial.println("Moving elbow to " + String(angle));
          } else validInput = false;
          break;
        case 4:
          if (angle >= 0 && angle <= 180) {
            newPos.wrist_ver = angle;
            Serial.println("Moving wrist vertical to " + String(angle));
          } else validInput = false;
          break;
        case 5:
          if (angle >= 0 && angle <= 180) {
            newPos.wrist_rot = angle;
            Serial.println("Moving wrist rotation to " + String(angle));
          } else validInput = false;
          break;
        case 6:
          if (angle >= 10 && angle <= 73) {
            newPos.gripper = angle;
            Serial.println("Moving gripper to " + String(angle));
          } else validInput = false;
          break;
        default:
          Serial.println("Invalid joint number (use 1-6)");
          validInput = false;
      }
      
      if (!validInput) {
        Serial.println("Invalid angle for joint " + String(joint));
        Serial.println("Please check the allowed ranges");
        continue;
      }
      
      moveToPosition(newPos);
    }
  }
}