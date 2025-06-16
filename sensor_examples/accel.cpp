#include <Wire.h>
#include <Adafruit_LSM6DSO32.h> // Adafruit has this lib for this sensor

// Create the object first
Adafruit_LSM6DSO32 lsm6dso32;


const float MOTION_SENSITIVITY = 0.1;
const unsigned long NO_MOTION_PERIOD = 10000;
unsigned long lastMotion = 0;

void setup() {
Serial.begin(115200);
while (!Serial) delay(10);

// Iyan: This initializes The I2C if you're usibf wire.h on the atmega
Wire.begin();

// Then this initializes the sensor
if (!lsm6dso32.begin_I2C()) {
Serial.println("No sensor");
while (1) { delay(10); }
}

lastMotion = millis();
}

void loop() {
// Get the data from sensors
sensors_event_t accel;
sensors_event_t gyro;
sensors_event_t temp;
lsm6dso32.getEvent(&accel, &gyro, &temp);

float totalAccel = sqrt(accel.acceleration.x * accel.acceleration.x +
accel.acceleration.y * accel.acceleration.y +
accel.acceleration.z * accel.acceleration.z);
// Actual Acceleration
float netAccel = abs(totalAccel - 9.81);

if (netAccel > MOTION_SENSITIVITY) {
lastMotionTime = millis();
}

// Checks for a no movement condition over time.
if (millis() - lastMotionTime > NO_MOTION_PERIOD) {
send_notification(); // Iyan This was a function call to the full code. Not important here.
lastMotionTime = millis(); // reset after notification
}

delay(500);
}

