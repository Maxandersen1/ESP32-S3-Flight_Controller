#include <SPI.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP3XX.h"

// --- PINS ---
#define I2C_SDA 4
#define I2C_SCL 5
#define SPI_MOSI 11
#define SPI_MISO 13
#define SPI_SCK  12
#define CS_ACCEL 9
#define CS_GYRO  10

Adafruit_BMP3XX bmp;

void writeSPI(int cs_pin, byte reg, byte value) {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(cs_pin, LOW);
  SPI.transfer(reg); 
  SPI.transfer(value);
  digitalWrite(cs_pin, HIGH);
  SPI.endTransaction();
}

float pitch = 0;
float roll = 0;
unsigned long last_time = 0;

float baseline_pressure = 1013.25; 
float current_altitude = 0.0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Start I2C and Barometer
  Wire.begin(I2C_SDA, I2C_SCL);
  if (bmp.begin_I2C(0x76, &Wire)) {
    bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP3_ODR_50_HZ);

    for(int i=0; i<10; i++) {
       bmp.performReading();
       delay(20);
    }

    float sum_pressure = 0;
    for(int i=0; i<50; i++) {
       bmp.performReading();
       sum_pressure += (bmp.pressure / 100.0); 
       delay(20);
    }
    baseline_pressure = sum_pressure / 50.0;
  }

  // Start IMU
  pinMode(CS_ACCEL, OUTPUT);
  pinMode(CS_GYRO, OUTPUT);
  digitalWrite(CS_ACCEL, HIGH);
  digitalWrite(CS_GYRO, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, -1);

  writeSPI(CS_ACCEL, 0x7F, 0x00);
  delay(10);
  writeSPI(CS_ACCEL, 0x7D, 0x04);
  delay(50);
  writeSPI(CS_ACCEL, 0x7C, 0x00);
  delay(50);

  last_time = micros();
}

void loop() {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  // --- LÄS ACCEL ---
  digitalWrite(CS_ACCEL, LOW);
  SPI.transfer(0x12 | 0x80); 
  SPI.transfer(0x00);        
  byte ax_l = SPI.transfer(0x00); byte ax_h = SPI.transfer(0x00);
  byte ay_l = SPI.transfer(0x00); byte ay_h = SPI.transfer(0x00);
  byte az_l = SPI.transfer(0x00); byte az_h = SPI.transfer(0x00);
  digitalWrite(CS_ACCEL, HIGH);

  float acc_x = ((int16_t)((ax_h << 8) | ax_l)) / 10922.0f;
  float acc_y = ((int16_t)((ay_h << 8) | ay_l)) / 10922.0f;
  float acc_z = ((int16_t)((az_h << 8) | az_l)) / 10922.0f;

  // --- LÄS GYRO ---
  digitalWrite(CS_GYRO, LOW);
  SPI.transfer(0x02 | 0x80);
  byte gx_l = SPI.transfer(0x00); byte gx_h = SPI.transfer(0x00);
  byte gy_l = SPI.transfer(0x00); byte gy_h = SPI.transfer(0x00);
  byte gz_l = SPI.transfer(0x00); byte gz_h = SPI.transfer(0x00);
  digitalWrite(CS_GYRO, HIGH);

  float gyr_x = ((int16_t)((gx_h << 8) | gx_l)) / 16.384f;
  float gyr_y = ((int16_t)((gy_h << 8) | gy_l)) / 16.384f;
  float gyr_z = ((int16_t)((gz_h << 8) | gz_l)) / 16.384f;

  SPI.endTransaction();

  // --- Complementary Filter ---
  unsigned long now = micros();
  float dt = (now - last_time) / 1000000.0f; 
  last_time = now;

  float pitch_acc = atan2(acc_x, sqrt(acc_y*acc_y + acc_z*acc_z)) * 180.0 / PI;
  float roll_acc  = atan2(acc_y, sqrt(acc_x*acc_x + acc_z*acc_z)) * 180.0 / PI;

  pitch = 0.98 * (pitch - gyr_y * dt) + 0.02 * pitch_acc;
  roll  = 0.98 * (roll  + gyr_x * dt) + 0.02 * roll_acc;

  // --- LÄS BAROMETERN ---
  if (bmp.performReading()) {
      current_altitude = bmp.readAltitude(baseline_pressure);
  }

  // Serial output for 3D Viewer
  Serial.print(pitch);
  Serial.print(",");
  Serial.print(roll);
  Serial.print(",");
  Serial.println(current_altitude);
  
  delay(10); 
}
