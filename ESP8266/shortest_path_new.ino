
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

const int MPU_ADDR =      0x68;
const int WHO_AM_I =      0x75;
const int PWR_MGMT_1 =    0x6B;
const int GYRO_CONFIG =   0x1B;
const int ACCEL_CONFIG =  0x1C;
const int ACCEL_XOUT =    0x3B;

const int sda_pin = D2; // define I2C SDA
const int scl_pin = D1; // define I2C SCL

//------ holds values form MPU6050 sensor ------//
int16_t AcX, AcY, AcZ, GyX, GyY, GyZ;

//------ holds update values for fade multithreading ------//
unsigned long LastRandomUpdate = 0;
unsigned long LastMsgUpdate = 0;
unsigned long LastPushUpdate = 0;

//------ holds random number updates ------//
int randNumber;

//------ flags ------//
bool restPosState = HIGH;
bool publishState = HIGH;
bool msgState = HIGH;
bool randState = HIGH;

//------ take a snapshot of the rest position ------//
int restPos;

//------ values used to delay fade state change ------//
int msgCounter = 0;
int pushCounter = 0;

//------ fade states ------//
#define UP 0
#define DOWN 1

byte fadeDirection = UP;
byte msgfadeDirection = UP;
byte pushfadeDirection = UP;
byte fadeIncrement = 1;

//------ fade limit values ------//
const int minPWM = 0;
const int maxPWM = 1023;

const int minRandPWM = 0;
const int maxRandPWM = 160;

int fadeValue = 0;
int msgFadeValue = 0;
int pushFadeValue = 0;

int fadeMax = 1100;

//------ LED pin ------//
const int LED = D8;
//const int LED = D5;

//------ updates millis() value ------//
unsigned long currentMillis;

//------ wifi connection values ------//
const char* ssid = "Shortest_Path_AP";   //wifi name
const char* password = "ESP8266rocks";       //wifi PassWord
const char* mqtt_server = "192.168.0.3";   //MQTT broker ip (Raspberry pi)

//------ unique device values ------//
const char* deviceSub = "47";                //device number - has to be modified for every device
const char* deviceHigh = "#47,1";
const char* deviceLow = "#47,0";


void initI2C()
{
  Wire.begin(sda_pin, scl_pin);
}

void writeRegMPU(int reg, int val)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission(true);
}

uint8_t readRegMPU(uint8_t reg)
{
  uint8_t data;
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 1);
  data = Wire.read();
  return data;
}

void findMPU(int mpu_addr)
{
  Wire.beginTransmission(MPU_ADDR);
  int data = Wire.endTransmission(true);

  if (data == 0)
  {
    Serial.print("Device found at address: 0x");
    Serial.println(MPU_ADDR, HEX);
  }
  else
  {
    Serial.println("Device not found!");
  }
}

void checkMPU(int mpu_addr)
{
  findMPU(MPU_ADDR);
  int data = readRegMPU(WHO_AM_I);

  if (data == 104)
  {
    Serial.println("MPU6050 OK! (104)");

    data = readRegMPU(PWR_MGMT_1); // Register 107 – Power Management 1-0x6B

    if (data == 64) Serial.println("MPU6050 is in sleep mode! (64)");
    else Serial.println("MPU6050 ACTIVE!");
  }
  else Serial.println("Verifique dispositivo - MPU6050 NÃO disponível!");
}

void initMPU()
{
  setSleepOff();
  setGyroScale();
  setAccelScale();
}

void setSleepOff()
{
  writeRegMPU(PWR_MGMT_1, 0);
}

void setGyroScale()
{
  writeRegMPU(GYRO_CONFIG, 0);
}

void setAccelScale()
{
  writeRegMPU(ACCEL_CONFIG, 0);
}

void readRawMPU()
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(ACCEL_XOUT);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14);

  AcX = Wire.read() << 8;
  AcX |= Wire.read();
  AcY = Wire.read() << 8;
  AcY |= Wire.read();
  AcZ = Wire.read() << 8;
  AcZ |= Wire.read();

  /*
    GyX = Wire.read() << 8;
    GyX |= Wire.read();
    GyY = Wire.read() << 8;
    GyY |= Wire.read();
    GyZ = Wire.read() << 8;
    GyZ |= Wire.read();
  */
}

//------ Function used to genrate random numbers ------//
int getRandomNumber(int startNum, int endNum) {
  randomSeed(ESP.getCycleCount());
  return random(startNum, endNum);
}

WiFiClient espClient;
PubSubClient client(espClient);


void setup() {
  
  delay(100);
  Serial.print("Setting "); Serial.print(LED); Serial.println(" as OUTPUT");
  pinMode(LED, OUTPUT); //Set D8 as output pin
  Serial.begin(115200);

  int randWifiConnect = getRandomNumber(0, 15000);

  Serial.println(" ");
  Serial.print("waiting for ");
  Serial.print(randWifiConnect);
  Serial.println(" millis before wifi connect...");
  delay(randWifiConnect);

  Serial.println("initializing I2C coms...");
  initI2C();
  Serial.println("setting up MPU6050 module...");
  initMPU();
  delay(50);

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  delay(50);
}

void loop() {

  
  currentMillis = millis();

  //------ update MQTT client ------// DO NOT MOVE!!!
  client.loop();

  if (!client.connected()) {
    reconnect();
  }

  //------ check to see if sensor init correctly ------//
  if ( restPosState ) {
    delay(100);
    readRawMPU();

    restPos = AcX;
    Serial.print("restPos: ");
    Serial.println(restPos);

    if (restPos == -1) {
      Serial.println("MPU6050 doesn't communicate correctly");
      Serial.println("restarting...");
      ESP.restart();
    }
    else {
      Serial.println("sensor data is A-ok");
      digitalWrite(LED, HIGH); delay(200);
      digitalWrite(LED, LOW);  delay(200);
      digitalWrite(LED, HIGH); delay(200);
      digitalWrite(LED, LOW);
      Serial.println("starting loop...");
    }
    restPosState = LOW;
  }

  //------- update sensor values ------//
  readRawMPU();

  if (( AcX > restPos + 1550  || AcX < restPos - 1550 ) && publishState)
  {
    publishState = !publishState;
    Serial.print("turning ON device: "); Serial.println(deviceSub);
    client.publish("inData", deviceHigh);
  }

  if (( AcX > (restPos + 1600)  || AcX < (restPos - 1600)) && pushFadeValue >= 250 )
  {
    if ( pushfadeDirection == UP ) {
      pushFadeValue = 150;
      pushCounter = 0;
    }
    if ( pushCounter >= 1250 ) {
      pushFadeValue = 100;
      pushCounter = 0;
      pushfadeDirection = UP;
      Serial.println("was here");
    }
    Serial.print("turning ON device agian: "); Serial.println(deviceSub);
    client.publish("inData", deviceHigh);
  }

  if (( currentMillis - LastPushUpdate ) > 4 ) {
    if ( !publishState ) {
      randState = LOW;
      randNumber = 0;
      msgState = HIGH;
      push_fade();
    }
    LastPushUpdate = currentMillis;
  }

  if (( currentMillis - LastMsgUpdate ) > 4 ) {
    if ( !msgState ) {
      randState = LOW;
      msg_fade();
    }
    LastMsgUpdate = currentMillis;
  }

  if (( currentMillis - LastRandomUpdate ) > 2 ) {  //4
    random_fade( 0, 30000 );                        //random value limits
    LastRandomUpdate = currentMillis;
  }
}

void setup_wifi() {

  delay(10);                                        //start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);                       //connect...

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.print("WiFi connected");
  //Serial.println("IP address: " + WiFi.localIP());
}

//------ MQTT msg received ------//
void callback( char* topic, byte * payload, unsigned int length ) {

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  if (topic == "outData") { }

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    //msg.concat((char)payload[i]);
  }
  //Serial.print("msg: "); Serial.print((char)payload[0]); Serial.println((char)payload[1]);

  if ((char)payload[0] == deviceSub[0] && (char)payload[1] == deviceSub[1]) {

    msgState = LOW;   // turn on the msg_fade_function
    msgfadeDirection = UP;
    msgCounter = 0;

    if ( msgCounter != 1 ) {
      msgFadeValue = 0;
    }
    else {
      msgFadeValue = 100;
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    delay(10);
    Serial.print("Attempting MQTT connection...");

    if (client.connect(deviceSub)) {
      Serial.println("connected");
      client.publish("test", deviceSub );               //publish to "test" topic on MQTT connect
      client.subscribe("outData");
    }                                                    //...and resubscribe to topic outData
    else {
      Serial.print("failed, rc=");                      //if failed to connect --> retry every 5 sec.
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}

void push_fade() {
  pushCounter++;
  Serial.print("pushFadeValue: "); Serial.println(pushFadeValue);

  if ( pushfadeDirection == UP ) {
    pushFadeValue++;
    if ( pushCounter >= fadeMax ) {
      pushfadeDirection = DOWN;
    }
  }
  else {
    pushFadeValue--;
    if ( pushFadeValue <= minPWM ) {

      pushfadeDirection = UP;
      pushCounter = 0;
      pushFadeValue = 0;

      Serial.print("turning OFF device: "); Serial.println(deviceSub);
      client.publish("inData", deviceLow);

      publishState = HIGH;   //allow MQTT publich
      randState = HIGH;      //allow random number generation
      //msgFadeValue = 0;    //allow msg fade

      analogWrite(LED, 0); //make sure to turn LED off completely
    }
  }
  analogWrite(LED, pushFadeValue);
}

void msg_fade() {
  msgCounter++;
  Serial.print("msgFadeValue: "); Serial.println(msgFadeValue);

  if ( msgfadeDirection == UP ) {
    msgFadeValue++;
    if ( msgCounter >= fadeMax ) {
      msgfadeDirection = DOWN;
    }
  }
  else {
    msgFadeValue--;
    if (msgFadeValue <= minPWM) {

      msgFadeValue = 0;
      msgfadeDirection = UP;
      msgCounter = 0;

      msgState = HIGH;   //Stop exe msg_fade function
      randState = HIGH;  //turn on random numbers generation

      analogWrite(LED, 0);
    }
  }
  analogWrite(LED, msgFadeValue);
}

void random_fade(int minVal, int maxVal) {

  if ( randState ) {
    randNumber = getRandomNumber(minVal, maxVal);
    //Serial.print("New random number: "); Serial.println(randNumber);
  }

  if ( randNumber == 2 ) {  //could be any number between min and max
    Serial.print("magic number: "); Serial.println(randNumber);
    randState = LOW;

    if ( fadeDirection == UP ) {
      fadeValue = fadeValue + fadeIncrement;
      analogWrite(LED, fadeValue);
      if (fadeValue >= maxRandPWM) {               // At max, limit and change direction
        fadeValue = maxRandPWM;
        fadeDirection = DOWN;
      }
    }
    else {
      fadeValue = fadeValue - fadeIncrement;       // if we aren't going up, we're going down
      analogWrite(LED, fadeValue);
      if (fadeValue <= minRandPWM) {               // At min, limit and change direction
        fadeValue = minRandPWM;
        fadeDirection = UP;
        randState = HIGH;
        randNumber = 0;
      }
      /*
        if (fadeValue == minRandPWM) {
          randState = HIGH;
          randNumber = LOW;
        }
      */
    }
  }
}

