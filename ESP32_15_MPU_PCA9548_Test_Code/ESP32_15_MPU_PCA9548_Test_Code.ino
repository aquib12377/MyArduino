I#include <Arduino.h>
#include <Wire.h>

#define SDA 21
#define SCL 22
#define I2C_FREQ 400000UL

constexpr uint8_t MUX1 = 0x71; // A0=1
constexpr uint8_t MUX2 = 0x74; // A2=1
constexpr uint8_t MPU_ADDR = 0x68; // all your boards respond here

// MPU regs
constexpr uint8_t REG_WHO_AM_I     = 0x75;
constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
constexpr uint8_t REG_CONFIG       = 0x1A;
constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;

struct ImuRaw{ int16_t ax,ay,az,t,gx,gy,gz; };

struct Slot { uint8_t mux; uint8_t ch; bool ok; uint8_t who; };
Slot slots[15] = {
  {MUX1,0,false,0},{MUX1,1,false,0},{MUX1,2,false,0},{MUX1,3,false,0},
  {MUX1,4,false,0},{MUX1,5,false,0},{MUX1,6,false,0},{MUX1,7,false,0},
  {MUX2,0,false,0},{MUX2,1,false,0},{MUX2,2,false,0},
  {MUX2,3,false,0},{MUX2,4,false,0},{MUX2,5,false,0},{MUX2,6,false,0},
};

bool muxWrite(uint8_t mux, uint8_t mask){
  Wire.beginTransmission(mux); Wire.write(mask);
  return Wire.endTransmission()==0;
}
inline void muxOff(uint8_t mux){ muxWrite(mux,0x00); }

bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t val){
  Wire.beginTransmission(addr); Wire.write(reg); Wire.write(val);
  return Wire.endTransmission()==0;
}
bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n){
  Wire.beginTransmission(addr); Wire.write(reg);
  if (Wire.endTransmission(false)!=0) return false;
  if (Wire.requestFrom((int)addr,(int)n)!=n) return false;
  for(uint8_t i=0;i<n;i++) buf[i]=Wire.read();
  return true;
}

bool mpuInit(uint8_t addr){
  if(!i2cWrite8(addr, REG_PWR_MGMT_1, 0x00)) return false; // wake
  delay(5);
  if(!i2cWrite8(addr, REG_SMPLRT_DIV, 9))    return false; // 100 Hz
  if(!i2cWrite8(addr, REG_CONFIG, 0x03))     return false; // DLPF 3
  if(!i2cWrite8(addr, REG_GYRO_CONFIG, 0x00))return false; // ±250 dps
  if(!i2cWrite8(addr, REG_ACCEL_CONFIG,0x00))return false; // ±2 g
  return true;
}

bool mpuReadAll(uint8_t addr, ImuRaw& r){
  uint8_t b[14];
  if(!i2cRead(addr, REG_ACCEL_XOUT_H, b, 14)) return false;
  r.ax=(int16_t)(b[0]<<8|b[1]);  r.ay=(int16_t)(b[2]<<8|b[3]);
  r.az=(int16_t)(b[4]<<8|b[5]);  r.t =(int16_t)(b[6]<<8|b[7]);
  r.gx=(int16_t)(b[8]<<8|b[9]);  r.gy=(int16_t)(b[10]<<8|b[11]);
  r.gz=(int16_t)(b[12]<<8|b[13]);
  return true;
}

void setup(){
  Serial.begin(115200); delay(200);
  Wire.begin(SDA,SCL);
  Wire.setClock(I2C_FREQ);

  // Turn all channels off
  muxOff(MUX1); muxOff(MUX2);

  Serial.println("\nDetecting sensors per channel...");
  for (auto &s: slots){
    if(!muxWrite(s.mux, 1<<s.ch)){ 
      Serial.printf("MUX 0x%02X CH%u: select failed\n", s.mux, s.ch);
      continue;
    }
    // Probe @0x68 only (your scan shows all at 0x68)
    uint8_t who=0;
    bool present = i2cRead(MPU_ADDR, REG_WHO_AM_I, &who, 1);
    muxOff(s.mux);

    if(!present){ 
      Serial.printf("MUX 0x%02X CH%u: no device @0x68\n", s.mux, s.ch);
      continue;
    }

    // Accept MPU6050 (0x68) and MPU6500-class (0x70)
    if (who==0x68 || who==0x70){
      s.ok=true; s.who=who;
      // init it now
      muxWrite(s.mux, 1<<s.ch);
      bool inited = mpuInit(MPU_ADDR);
      muxOff(s.mux);
      Serial.printf("MUX 0x%02X CH%u: found @0x68, WHO=0x%02X%s, init=%s\n",
        s.mux, s.ch, who, (who==0x70?" (6500-class)":""),
        inited?"OK":"FAIL");
    } else {
      Serial.printf("MUX 0x%02X CH%u: unexpected WHO=0x%02X @0x68\n", s.mux, s.ch, who);
    }
    delay(2);
  }
  Serial.println("Init pass done.\n");
}

void loop(){
  for (auto &s: slots){
    if(!s.ok) continue;
    if(!muxWrite(s.mux, 1<<s.ch)){
      Serial.printf("MUX 0x%02X CH%u: select failed (read)\n", s.mux, s.ch);
      continue;
    }
    ImuRaw r; bool ok = mpuReadAll(MPU_ADDR, r);
    muxOff(s.mux);
    if(!ok){
      Serial.printf("MUX 0x%02X CH%u: read fail\n", s.mux, s.ch);
      continue;
    }
    float ax=r.ax/16384.0f, ay=r.ay/16384.0f, az=r.az/16384.0f;
    float gx=r.gx/131.0f,  gy=r.gy/131.0f,  gz=r.gz/131.0f;
    float tC=r.t/340.0f + 36.53f;
    Serial.printf("MUX 0x%02X CH%u WHO=0x%02X | "
                  "A[g]=[%.3f,%.3f,%.3f] G[°/s]=[%.2f,%.2f,%.2f] T=%.2f°C\n",
                  s.mux, s.ch, s.who, ax,ay,az, gx,gy,gz, tC);
  }
  delay(100);
}
