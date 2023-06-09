#include <Wire.h>
#include <Servo.h>           //Need for Servo pulse output
#include <SoftwareSerial.h>  //Need for Wireless


//State machine       <------ will need to add a bunch more states to this I think
enum STATE {
  INITIALISING,
  FIND_CLOSEST_FIRE,
  TRAVEL_TO_FIRE,
  FIGHT_FIRE,
  STOPPED
};

// Defines //
#define INTERNAL_LED 13
#define TIMER_FREQ 10
// Serial Data input pin
#define BLUETOOTH_RX 10
// Serial Data output pin
#define BLUETOOTH_TX 11
#define STARTUP_DELAY 10  // Seconds
#define LOOP_DELAY 10     // miliseconds
#define SAMPLE_DELAY 10   // miliseconds
// USB Serial Port
#define OUTPUTMONITOR 0
#define OUTPUTPLOTTER 0
// Bluetooth Serial Port
#define OUTPUTBLUETOOTHMONITOR 1
#define IR_Left A1    // Sharp IR GP2Y0A41SK0F (4-30cm, analog) Back Left
#define IR_Front_Left A4   // Sharp IR GP2Y0A41SK0F (4-30cm, analog) Front Left
#define IR_Right A2   // Sharp IR GP2Y0A41SK0F (4-30cm, analog) Back Right
#define IR_Front_Right A3  // Sharp IR GP2Y0A41SK0F (4-30cm, analog) Front Right
#define echoPin A7  // attach pin A4 Arduino to pin Echo of HC-SR04
#define trigPin A6  //attach pin A5 Arduino to pin Trig of HC-SR04
#define phototransistor_left_1 A15
#define phototransistor_left_2 A14
#define phototransistor_right_1 A12
#define phototransistor_right_2 A11

SoftwareSerial BluetoothSerial(BLUETOOTH_RX, BLUETOOTH_TX);
Servo myservo;  // create servo object to control a servo
int pos = 0;    // variable to store the servo position
int fanPin = 45;  // the digital output pin connected to the MOSFET's gate
int T = 100;                    // T is the time of one loop
int sensorValue = 0;            // read out value of sensor
int averagePhototransistorRead = 0;            //Defining the Global Ultrasonic Reading
int maxPhototransistorRead = 0;                 //Definining Global Max Ultrasonic
int firesExtinguished = 0;
int minimumLight = 5;
byte serialRead = 0;  // for serial print control
//Default motor control pins
const byte left_front = 50;
const byte left_rear = 51;
const byte right_rear = 46;
const byte right_front = 47;
//Default ultrasonic ranging sensor pins, these pins are defined my the Shield
const int TRIG_PIN = 48;
const int ECHO_PIN = 49;
// Anything over 400 cm (23200 us pulse) is "out of range". Hit:If you decrease to this the ranging sensor but the timeout is short, you may not need to read up to 4meters.
const unsigned int MAX_DIST = 23200;

volatile bool _frontLeft;   //Bools for obstacle avoidance check
volatile bool _frontRight;
volatile bool _ultrasonic;
volatile bool _left;
volatile bool _right;

Servo left_front_motor;   // create servo object to control Vex Motor Controller 29
Servo left_rear_motor;    // create servo object to control Vex Motor Controller 29
Servo right_rear_motor;   // create servo object to control Vex Motor Controller 29
Servo right_front_motor;  // create servo object to control Vex Motor Controller 29
Servo turret_motor;

int speed_val = 150;
int speed_change;

//Serial Pointer
HardwareSerial *SerialCom;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////Setup Function

void setup(void) {
    turret_motor.attach(11);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(INTERNAL_LED, OUTPUT);
    
    // The Trigger pin will tell the sensor to range find
    pinMode(TRIG_PIN, OUTPUT);
    digitalWrite(TRIG_PIN, LOW);
    
    // Setup the Serial port and pointer, the pointer allows switching the debug info through the USB port(Serial) or Bluetooth port(Serial1) with ease.
    SerialCom = &Serial;
    SerialCom->begin(115200);
    SerialCom->println("MECHENG706 Project 2");
    delay(1000);
    Serial.begin(9600);
    // this section is initialize the sensor, find the the value of voltage when gyro is zero
    pinMode(fanPin, OUTPUT);

    myservo.attach(21);  // attaches the servo on pin 21 to the servo object
    
    //Ultrasonic Setup
    pinMode(trigPin, OUTPUT);                          // Sets the trigPin as an OUTPUT
    pinMode(echoPin, INPUT);                           // Sets the echoPin as an INPUT
    
    Serial.println("Setup Complete");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////MAIN LOOP

void loop(void)  
{ 
    static STATE machine_state = INITIALISING;
    //Finite-state machine Code
    switch (machine_state) {
        case INITIALISING:
            machine_state = initialising();
            break;
        case FIND_CLOSEST_FIRE:  //Lipo Battery Volage OK
            machine_state = find_closest_fire();
            break;
        case TRAVEL_TO_FIRE:  //Lipo Battery Volage OK
            machine_state = travel_to_fire();
            break;  
        case FIGHT_FIRE:  //Lipo Battery Volage OK
            machine_state = fight_fire();
            break; 
        case STOPPED:  //Lipo Battery Volage OK
            machine_state = stopped();
            break;  
    };
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////STATES

STATE initialising() {
    SerialCom->println("INITIALISING....");
    delay(1000);
    SerialCom->println("Enabling Motors...");
    enable_motors();
    SerialCom->println("RUNNING STATE...");
    return FIND_CLOSEST_FIRE;
}

STATE find_closest_fire() {
    myservo.write(81);
    averagePhototransistorRead = 0;  // Reset the global phototransistor reading
    maxPhototransistorRead = 0; // Reset the global phototransistor maximum
    int _brightnessCount = 0;
    int _direction = findLightDirection();

    if (_direction >= 0) {
        ccw();
    } else {
        cw();
    }

    while (_brightnessCount < 150){
        averagePhototransistorRead = averagePhototransistor();
        if (averagePhototransistorRead > 10){_brightnessCount++;}
    }
    stop();

    return TRAVEL_TO_FIRE;
}

STATE travel_to_fire() {
    stop();
    averagePhototransistorRead = 0;  // Reset the global phototransistor reading
    maxPhototransistorRead = 0; // Reset the global phototransistor maximum
    myservo.write(81);

    float currentAngleMove=0;     
    float x_error = 0, angle_error = 0, y_error = 0;
    float x_velocity = 0, angular_velocity = 0, y_velocity = 0;
    float x_k_p = 20, x_k_i = 0.5, x_k_d = 1.002;
    float angle_k_p = 4, angle_k_i = 0.0, angle_k_d = 0.0001; 
    float y_k_p = 40, y_k_i = 0.5, y_k_d = 1.002;
    float previous_x_error = 0, previous_angle_error = 0, previous_y_error = 0;
    float integral_x_error = 0, integral_angle_error = 0, integral_y_error;
    float derivative_x_error = 0, derivative_angle_error = 0, derivative_y_error = 0;
    float radius = 2.6, length = 8.5, width = 9.2;    // wheel specs
    float theta_dot_1 = 0, theta_dot_2 = 0, theta_dot_3 = 0, theta_dot_4 = 0;
    float x_ultrasonic = 0, y_left = 0, y_right = 0, front_left = 0, front_right = 0;
    float old_servoAngle = 80, new_servoAngle = 0, servoAngle = 0, xDistanceDesired = 5; //<------we could replace this with looking for brightness instead of distance? IDK

    int _noFireCheck = 0;
    int _fireCheck = 0;
    int count = 0;
    int _servoWrite = 80;
    int _closePhototransistor = 0;

    bool xExit = false;
       
    while (true) {
        x_ultrasonic = ultrasonic();
        y_left = read_IR(IR_Left);
        y_right = read_IR(IR_Right);
        front_left = read_IR(IR_Front_Left);
        front_right = read_IR(IR_Front_Right);
        averagePhototransistorRead = averagePhototransistor();
        _closePhototransistor = closePhototransistor();

        if (averagePhototransistorRead < 10){
            _noFireCheck++;
            if (_noFireCheck > 2){
                _fireCheck = 0;
            }
        }
        else{
            _fireCheck++;

            if (_noFireCheck != 0 && _fireCheck > 7){
                _noFireCheck = 0;
            }

            new_servoAngle = old_servoAngle + findLightDirection();
        }

        if(_noFireCheck > 10){
            stop();
            return FIND_CLOSEST_FIRE;
        }


        if (new_servoAngle > 160 || new_servoAngle < 20){
              stop();
              if (averagePhototransistorRead > 100){
                 reverse();
                 delay(400);
                 stop();
              }
              return FIND_CLOSEST_FIRE;
        }

        if (new_servoAngle > 90){
          _servoWrite = 90;
        }
        else if (new_servoAngle < 70){
          _servoWrite = 70;
        }   
        else {_servoWrite = new_servoAngle;}     
        myservo.write(_servoWrite);
        
        // Calculate errors // 
        //////////////////////////////////////
        x_error = (xDistanceDesired - x_ultrasonic);
        if (x_error > 200){
            x_error = 200;
        }
        if (x_error < -200){
            x_error = -200;
        }

        y_error = 0;
        if (y_left < 6){
            y_error = -30;
        }
        if (y_right < 6){
            y_error = 40;
        }
        if (y_right < 8 && y_left < 8){
            if (y_left < 5 && y_right > 5){
                y_error = -30;
            }
            else if (y_right < 5 && y_left > 5){
                y_error = 30;
            }
            else{
                y_error = 0;
            }
        }
    
        angle_error = -(new_servoAngle - old_servoAngle);
        if (angle_error > 90){
            angle_error = 90;
        }
        if (angle_error < -90){
            angle_error = -90;
        }

        //Obstacle Avoidance via PID
        //////////////////////////////////////////////
        if (_closePhototransistor < 550){ //VALUE TO BE TUNED THIS NEEDS TO BE ACCURATE
            if (front_left < 8 && y_left < 6){
                reverse();
                delay(1000);
                stop();
                return FIND_CLOSEST_FIRE;
            }
            if (front_right < 8 && y_right < 6){
                reverse();
                delay(1000);
                stop();
                return FIND_CLOSEST_FIRE;
            }
            if(x_ultrasonic < 9 || front_left < 9){
                x_error = 0;
                y_error = -100;
            }
            else if(front_right < 9){
                x_error = 0;
                y_error = 100;
            }           
        }

        if (abs(integral_x_error) < 50){
            integral_x_error += x_error;
        }
    
        if (abs(integral_angle_error) < 10){
            integral_angle_error += angle_error;
        }

        if (abs(integral_y_error) < 50){
            integral_y_error += y_error;
        }
        
        // Calculate derivatives
        derivative_x_error = x_error - previous_x_error;
        derivative_y_error = y_error - previous_y_error;
        derivative_angle_error = angle_error - previous_angle_error;
    
        x_velocity = x_k_p * x_error + x_k_i * integral_x_error + x_k_d * derivative_x_error;
        y_velocity = y_k_p * y_error + y_k_i * integral_y_error + y_k_d * derivative_y_error;
        angular_velocity = angle_k_p * angle_error + angle_k_i * integral_angle_error + angle_k_d * derivative_angle_error;
    
        if (x_velocity > 500){ x_velocity = 500;}
        if (x_velocity < -500){ x_velocity = -500;}

        if (y_velocity > 600){ y_velocity = 600;}
        if (y_velocity < -600){ y_velocity = -600;}
    
        if (angular_velocity > 50){ angular_velocity = 50;}
        if (angular_velocity < -50){ angular_velocity = -50;}
    
        // Calculate control outputs
        theta_dot_1 = ( 1 / radius ) * (x_velocity + y_velocity - (angular_velocity*(length+width)));
        theta_dot_2 = ( 1 / radius ) * (x_velocity - y_velocity + (angular_velocity*(length+width)));
        theta_dot_3 = ( 1 / radius ) * (x_velocity - y_velocity - (angular_velocity*(length+width)));
        theta_dot_4 = ( 1 / radius ) * (x_velocity + y_velocity + (angular_velocity*(length+width)));
    
        // Send angular velocities of wheels to robot servo motor
        left_front_motor.writeMicroseconds(1500 - theta_dot_1);
        right_front_motor.writeMicroseconds(1500 + theta_dot_2);
        left_rear_motor.writeMicroseconds(1500 - theta_dot_3);
        right_rear_motor.writeMicroseconds(1500 + theta_dot_4);
        
        // Update previous error values
        previous_x_error = x_error;
        previous_y_error = y_error;
        previous_angle_error = angle_error;
        old_servoAngle = new_servoAngle;

        //Exit conditions 
    
        if (_closePhototransistor > 575){
            if (abs(x_error)<3 || read_IR(IR_Front_Right) < 5 || read_IR(IR_Front_Left) < 5){
              xExit = true;
              digitalWrite(fanPin, HIGH);
            }
        }
        else {xExit = false;}
    
        if (xExit){
          count++;
        }
         
        if (count > 5){
            stop(); 
            return FIGHT_FIRE;
        }
        if (!xExit){
            count = 0;
            digitalWrite(fanPin, LOW);
        }
        delay(100);

    }
    
    return FIGHT_FIRE;
}

STATE fight_fire() {
    digitalWrite(fanPin, HIGH);
    averagePhototransistorRead = averagePhototransistor();
    int _closePhototransistor = closePhototransistor();
    
    if ((closePhototransistor()+closePhototransistor())/2 < 200){ //Should implement a count average here of some sort to prevent false readings. 
        digitalWrite(fanPin, LOW);
        return FIND_CLOSEST_FIRE;
    }

    int new_servoAngle = 80;
    int old_servoAngle = myservo.read();
    while (_closePhototransistor > 500){
            new_servoAngle = old_servoAngle + findLightDirection();
            new_servoAngle = (new_servoAngle > 120) ? 90 : new_servoAngle;
            new_servoAngle = (new_servoAngle < 50) ? 90 : new_servoAngle;
            myservo.write(new_servoAngle);
            old_servoAngle = new_servoAngle;
            _closePhototransistor = closePhototransistor();
            delay(10);
    }
    if ((closePhototransistor()+closePhototransistor()) /2 > 300){
        return FIGHT_FIRE;  //<---Backup incase while loop exits incorrectly
    }

    delay(100);
    //Light should now be out
    digitalWrite(fanPin, LOW);  // turn off the fan

    firesExtinguished++;

    if (firesExtinguished < 2){
        reverse();
        delay(500);
        stop();
        return FIND_CLOSEST_FIRE;
    }
    else {
        return STOPPED;
    }
}


STATE stopped() {
    //Stop of Lipo Battery voltage is too low, to protect Battery
    static byte counter_lipo_voltage_ok;
    static unsigned long previous_millis;
    int Lipo_level_cal;
    disable_motors();

    if (millis() - previous_millis > 500) {  //print massage every 500ms
         previous_millis = millis();
         SerialCom->println("STOPPED---------");


#ifndef NO_BATTERY_V_OK
    //500ms timed if statement to check lipo and output speed settings
    if (is_battery_voltage_OK()) {
        SerialCom->print("Lipo OK waiting of voltage Counter 10 < ");
        SerialCom->println(counter_lipo_voltage_ok);
        counter_lipo_voltage_ok++;
        if (counter_lipo_voltage_ok > 10) {  //Making sure lipo voltage is stable
            counter_lipo_voltage_ok = 0;
            enable_motors();
            SerialCom->println("Lipo OK returning to RUN STATE");
            return STOPPED;
        }
    } else {
        counter_lipo_voltage_ok = 0;
    }
#endif
  }
  return STOPPED;
}

int findLightDirection() {
    const int numReadings = 10; // Number of readings to take
    int totalLeftBrightness = 0;
    int totalRightBrightness = 0;

    // Take multiple readings and average them
    for (int i = 0; i < numReadings; ++i) {
        totalLeftBrightness += (phototransistor(phototransistor_left_1) + phototransistor(phototransistor_left_2)) / 2;
        totalRightBrightness += (phototransistor(phototransistor_right_1) + phototransistor(phototransistor_right_2)) / 2;
    }

    int leftBrightness = totalLeftBrightness / numReadings;
    int rightBrightness = totalRightBrightness / numReadings;
    int gain_max = 10;
    if (leftBrightness > 300 || rightBrightness > 300){
        gain_max = 5;
    }

    int difference = leftBrightness - rightBrightness;
    int direction = (difference > 0) - (difference < 0); // 1 if left is brighter, -1 if right is brighter, 0 if equal

    // Take the absolute difference and scale it by some factor for a larger response
    // We use min to cap it at 15
    int magnitude = min(abs(difference) * 0.5, gain_max);

    return direction * magnitude;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////SENSOR FUNCTIONS

float ultrasonic() {  //<------------------------------------------------ Ultrasonic
     // Clears the trigPin condition
    
    digitalWrite(trigPin, LOW);
    delayMicroseconds(24);
    
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(120);
    digitalWrite(trigPin, LOW);
    
    float duration = pulseIn(echoPin, HIGH);
    float measurement = duration * 0.034 / 2; // convert duration to distance measurement    
    return measurement;
}


float read_IR(uint8_t Sensor) {
    
    float distance_measurement = 0;
    float sensorMeasurementTotal = 0;
    int i = 0;

    int _iterations = 4;
    while (i < _iterations){
        sensorMeasurementTotal += analogRead(Sensor) * (5.0 / 1023.0); // Reading sensor value and converting it to voltage
        i++;
    }
    
    float sensorMeasurement = sensorMeasurementTotal / _iterations;
    
    if(Sensor == IR_Front_Left){
        distance_measurement = 8.05 * pow(sensorMeasurement, -1.93);
    }
    if(Sensor == IR_Front_Right){
        distance_measurement = 9.1618 * pow(sensorMeasurement, -1.132);
    }
    if(Sensor == IR_Left){
        distance_measurement = 20.204 * pow(sensorMeasurement, -1.072);
    }
    if(Sensor == IR_Right){
        distance_measurement = 23.596 * pow(sensorMeasurement, -1.818);
    }

    return distance_measurement; 
}

float phototransistor(uint8_t Sensor){
    int iterations = 5;
    int count = 0;
    long brightness = 0;
    while (count < iterations){
        brightness += analogRead(Sensor); //A value of 0 means no light, 1024 means maximum light
        count++;
    }
    return (brightness / iterations);
}

float averagePhototransistor(){
    return (phototransistor(phototransistor_left_1) + phototransistor(phototransistor_left_2) + phototransistor(phototransistor_right_1) + phototransistor(phototransistor_right_2)) / 4;
}

float closePhototransistor(){
    return (phototransistor(phototransistor_left_1) + phototransistor(phototransistor_right_1)) / 2;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BASIC KINEMATICS
void disable_motors() {
    left_front_motor.detach();   // detach the servo on pin left_front to turn Vex Motor Controller 29 Off
    left_rear_motor.detach();    // detach the servo on pin left_rear to turn Vex Motor Controller 29 Off
    right_rear_motor.detach();   // detach the servo on pin right_rear to turn Vex Motor Controller 29 Off
    right_front_motor.detach();  // detach the servo on pin right_front to turn Vex Motor Controller 29 Off
  
    pinMode(left_front, INPUT);
    pinMode(left_rear, INPUT);
    pinMode(right_rear, INPUT);
    pinMode(right_front, INPUT);
}

void enable_motors() {
    left_front_motor.attach(left_front);    // attaches the servo on pin left_front to turn Vex Motor Controller 29 On
    left_rear_motor.attach(left_rear);      // attaches the servo on pin left_rear to turn Vex Motor Controller 29 On
    right_rear_motor.attach(right_rear);    // attaches the servo on pin right_rear to turn Vex Motor Controller 29 On
    right_front_motor.attach(right_front);  // attaches the servo on pin right_front to turn Vex Motor Controller 29 On
}

void strafe_left ()
{
    left_front_motor.writeMicroseconds(1500 - speed_val);
    left_rear_motor.writeMicroseconds(1500 + speed_val);
    right_rear_motor.writeMicroseconds(1500 + speed_val);
    right_front_motor.writeMicroseconds(1500 - speed_val);
}

void strafe_right ()
{
    left_front_motor.writeMicroseconds(1500 + speed_val);
    left_rear_motor.writeMicroseconds(1500 - speed_val);
    right_rear_motor.writeMicroseconds(1500 - speed_val);
    right_front_motor.writeMicroseconds(1500 + speed_val);
}

void ccw ()
{
    left_front_motor.writeMicroseconds(1500 - speed_val);
    left_rear_motor.writeMicroseconds(1500 - speed_val);
    right_rear_motor.writeMicroseconds(1500 - speed_val);
    right_front_motor.writeMicroseconds(1500 - speed_val);
}
void cw ()
{
    left_front_motor.writeMicroseconds(1500 + speed_val);
    left_rear_motor.writeMicroseconds(1500 + speed_val);
    right_rear_motor.writeMicroseconds(1500 + speed_val);
    right_front_motor.writeMicroseconds(1500 + speed_val);
}

void forward()
{
    left_front_motor.writeMicroseconds(1500 + speed_val);
    left_rear_motor.writeMicroseconds(1500 + speed_val);
    right_rear_motor.writeMicroseconds(1500 - speed_val);
    right_front_motor.writeMicroseconds(1500 - speed_val);
}

void reverse ()
{
    left_front_motor.writeMicroseconds(1500 - speed_val);
    left_rear_motor.writeMicroseconds(1500 - speed_val);
    right_rear_motor.writeMicroseconds(1500 + speed_val);
    right_front_motor.writeMicroseconds(1500 + speed_val);
}

void stop()  //Stop
{
    left_front_motor.writeMicroseconds(1500);
    left_rear_motor.writeMicroseconds(1500);
    right_rear_motor.writeMicroseconds(1500);
    right_front_motor.writeMicroseconds(1500);
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BATTERY CHECK & ULTRASONIC RANGE

#ifndef NO_BATTERY_V_OK
boolean is_battery_voltage_OK() {
  static byte Low_voltage_counter;
  static unsigned long previous_millis;

  int Lipo_level_cal;
  int raw_lipo;
  //the voltage of a LiPo cell depends on its chemistry and varies from about 3.5V (discharged) = 717(3.5V Min) https://oscarliang.com/lipo-battery-guide/
  //to about 4.20-4.25V (fully charged) = 860(4.2V Max)
  //Lipo Cell voltage should never go below 3V, So 3.5V is a safety factor.
  raw_lipo = analogRead(A0);
  Lipo_level_cal = (raw_lipo - 717);
  Lipo_level_cal = Lipo_level_cal * 100;
  Lipo_level_cal = Lipo_level_cal / 143;

  if (Lipo_level_cal > 0 && Lipo_level_cal < 160) {
    previous_millis = millis();
    SerialCom->print("Lipo level:");
    SerialCom->print(Lipo_level_cal);
    SerialCom->print("%");
    // SerialCom->print(" : Raw Lipo:");
    // SerialCom->println(raw_lipo);
    SerialCom->println("");
    Low_voltage_counter = 0;
    return true;
  } else {
    if (Lipo_level_cal < 0)
      SerialCom->println("Lipo is Disconnected or Power Switch is turned OFF!!!");
    else if (Lipo_level_cal > 160)
      SerialCom->println("!Lipo is Overchanged!!!");
    else {
      SerialCom->println("Lipo voltage too LOW, any lower and the lipo with be damaged");
      SerialCom->print("Please Re-charge Lipo:");
      SerialCom->print(Lipo_level_cal);
      SerialCom->println("%");
    }

    Low_voltage_counter++;
    if (Low_voltage_counter > 5)
      return false;
    else
      return true;
  }
}
#endif

#ifndef NO_HC - SR04
void HC_SR04_range() {
  unsigned long t1;
  unsigned long t2;
  unsigned long pulse_width;
  float cm;
  float inches;

  // Hold the trigger pin high for at least 10 us
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Wait for pulse on echo pin
  t1 = micros();
  while (digitalRead(ECHO_PIN) == 0) {
    t2 = micros();
    pulse_width = t2 - t1;
    if (pulse_width > (MAX_DIST + 1000)) {
      SerialCom->println("HC-SR04: NOT found");
      return;
    }
  }

  // Measure how long the echo pin was held high (pulse width)
  // Note: the micros() counter will overflow after ~70 min

  t1 = micros();
  while (digitalRead(ECHO_PIN) == 1) {
    t2 = micros();
    pulse_width = t2 - t1;
    if (pulse_width > (MAX_DIST + 1000)) {
      SerialCom->println("HC-SR04: Out of range");
      return;
    }
  }

  t2 = micros();
  pulse_width = t2 - t1;

  // Calculate distance in centimeters and inches. The constants
  // are found in the datasheet, and calculated from the assumed speed
  //of sound in air at sea level (~340 m/s).
  cm = pulse_width / 58.0;
  inches = pulse_width / 148.0;

  // Print out results
  if (pulse_width > MAX_DIST) {
    SerialCom->println("HC-SR04: Out of range");
  } else {
    SerialCom->print("HC-SR04:");
    SerialCom->print(cm);
    SerialCom->println("cm");
  }
}
#endif
