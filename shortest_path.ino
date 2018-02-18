
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

const int MPU_ADDR =      0x68; // definição do endereço do sensor MPU6050 (0x68)
const int WHO_AM_I =      0x75; // registro de identificação do dispositivo
const int PWR_MGMT_1 =    0x6B; // registro de configuração do gerenciamento de energia
const int GYRO_CONFIG =   0x1B; // registro de configuração do giroscópio
const int ACCEL_CONFIG =  0x1C; // registro de configuração do acelerômetro
const int ACCEL_XOUT =    0x3B; // registro de leitura do eixo X do acelerômetro

const int sda_pin = D2; // definição do pino I2C SDA
const int scl_pin = D1; // definição do pino I2C SCL

int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
int16_t oldAcX, oldAcY, oldAcZ;

unsigned long LastRandomUpdate = 0;
unsigned long LastMsgUpdate = 0;
unsigned long LastPushUpdate = 0;

int fadeTime = 5;
int i = 0;
int randNumber;

int msgCounter = 0;
int pushCounter = 0;

bool state = HIGH;
bool restPosState = HIGH;
bool randState = HIGH;

bool publishFlag = HIGH;

int restPos;

#define UP 0
#define DOWN 1

const int minPWM = 0;
const int maxPWM = 1023;

const int minRandPWM = 0;
const int maxRandPWM = 220;

int fadeValue = 0;
int msgFadeValue = 0;
int pushFadeValue = 0;

byte fadeDirection = UP;
byte msgfadeDirection = UP;
byte pushfadeDirection = UP;
byte fadeIncrement = 1;

const int LED = D5;
unsigned long currentMillis;

bool flag = HIGH;
bool stateFlag = HIGH;
bool msgState = HIGH;
bool pubFlag = HIGH;

char oldPayload[3];
char message[6];

const char* ssid = "wifiLab";                 //wifi name
const char* password = "Spillet15";           //wifi PW
const char* mqtt_server = "192.168.31.253";   //MQTT broker ip (Raspberry pi)

const char* deviceSub = "03";                  //device number - has to be modified for every device
const char* devicePub1 = "#03,1";
const char* devicePub0 = "#03,0";


void initI2C()
{
  Wire.begin(sda_pin, scl_pin);
}

void writeRegMPU(int reg, int val)
{
  Wire.beginTransmission(MPU_ADDR);     // inicia comunicação com endereço do MPU6050
  Wire.write(reg);                      // envia o registro com o qual se deseja trabalhar
  Wire.write(val);                      // escreve o valor no registro
  Wire.endTransmission(true);           // termina a transmissão
}

uint8_t readRegMPU(uint8_t reg)        // aceita um registro como parâmetro
{
  uint8_t data;
  Wire.beginTransmission(MPU_ADDR);     // inicia comunicação com endereço do MPU6050
  Wire.write(reg);                      // envia o registro com o qual se deseja trabalhar
  Wire.endTransmission(false);          // termina transmissão mas continua com I2C aberto (envia STOP e START)
  Wire.requestFrom(MPU_ADDR, 1);        // configura para receber 1 byte do registro escolhido acima
  data = Wire.read();                   // lê o byte e guarda em 'data'
  return data;
}

//function that searches for the sensor at address 0x68
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
  int data = readRegMPU(WHO_AM_I); // Register 117 – Who Am I - 0x75

  if (data == 104)
  {
    Serial.println("MPU6050 Dispositivo respondeu OK! (104)");

    data = readRegMPU(PWR_MGMT_1); // Register 107 – Power Management 1-0x6B

    if (data == 64) Serial.println("MPU6050 em modo SLEEP! (64)");
    else Serial.println("MPU6050 em modo ACTIVE!");
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
  writeRegMPU(PWR_MGMT_1, 0); // escreve 0 no registro de gerenciamento de energia(0x68), colocando o sensor em o modo ACTIVE
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

  //  Tmp = Wire.read() << 8;
  //  Tmp |= Wire.read();

  // GyX = Wire.read() << 8;
  // GyX |= Wire.read();
  // GyY = Wire.read() << 8;
  // GyY |= Wire.read();
  // GyZ = Wire.read() << 8;
  // GyZ |= Wire.read();

  //Serial.print("AcX = "); Serial.print(AcX);
  //Serial.print(" | AcY = "); Serial.print(AcY);
  //Serial.print(" | AcZ = "); Serial.print(AcZ);
  //Serial.println(" ");

  //Serial.print(" | Tmp = "); Serial.print(Tmp / 340.00 + 36.53);
  //Serial.print(" | GyX = "); Serial.print(GyX);
  //Serial.print(" | GyY = "); Serial.print(GyY);
}

int getRandomNumber(int startNum, int endNum) {
  randomSeed(ESP.getCycleCount());
  return random(startNum, endNum);
}

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {

  pinMode(LED, OUTPUT);
  Serial.begin(115200);

  int randWifiConnect = getRandomNumber(0, 1000);
  Serial.println(" ");
  Serial.print("waiting for ");
  Serial.print(randWifiConnect);
  Serial.println(" millis before WiFi connect...");
  delay(randWifiConnect);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  delay(10);

  Serial.println("initializing I2C coms...");
  initI2C();
  Serial.println("setting up MPU6050 module...");
  initMPU();
  //Serial.println("Finding MPU6050 I2C adress...");
  //checkMPU(MPU_ADDR);
}

void loop() {

  currentMillis = millis();

  client.loop();

  if (!client.connected()) {
    reconnect();
  }

  readRawMPU();

  if ( restPosState ) {
    restPos = AcX;
    Serial.print("restPos: ");
    Serial.println(restPos);
    restPosState = LOW;

    if (restPos == -1) {
      Serial.println("MPU6050 doesn't communicate correctly");
      ESP.restart();
      delay(1000);
    }
    else {
      Serial.println("sensor data is A-ok");
      digitalWrite(LED, HIGH);
      delay(200);
      digitalWrite(LED, LOW);
      delay(200);
      digitalWrite(LED, HIGH);
      delay(200);
      digitalWrite(LED, LOW);
    }
  }

  if ( AcX > ( restPos + 1500 ) || AcX < ( restPos - 1500 )) {
    client.publish("inData", devicePub1);
    Serial.println("turning ON device");
    publishFlag = LOW;
  }

  if (( currentMillis - LastPushUpdate) > 4 ) {
    if ( !publishFlag ) {
      randState = LOW;
      msgState = HIGH;
      push_fade();
    }
    LastPushUpdate = currentMillis;
  }

  if (( currentMillis - LastMsgUpdate) > 4 ) {
    if ( !msgState ) {
      randState = LOW;
      msg_fade();
    }
    LastMsgUpdate = currentMillis;
  }

  if (( currentMillis - LastRandomUpdate ) > 2 ) { //4
    random_fade( 0, 8000 );
    LastRandomUpdate = currentMillis;
  }
}

void setup_wifi() {

  delay(10);                      //start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.print(ssid);

  WiFi.begin(ssid, password);               //connect...

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("WiFi connected");
  Serial.println("IP address: " + WiFi.localIP());
}


//*********Receiving data********//
void callback(char* topic, byte * payload, unsigned int length) {

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  if (topic == "outData") {  }

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.print("msg: ");
  Serial.print((char)payload[0]);
  Serial.println((char)payload[1]);

  if ((char)payload[0] == deviceSub[0] && (char)payload[1] == deviceSub[1]) {
    msgState = LOW;
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
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

  if (pushfadeDirection == UP) {
    pushFadeValue = pushFadeValue + 1;
    if (pushFadeValue >= maxPWM) {
      pushFadeValue = maxPWM;
      pushCounter = pushCounter + 1;
      if ( pushCounter == 3000 ) {
        pushfadeDirection = DOWN;
        pushCounter = 0;
      }
    }
  }
  else {
    pushFadeValue = pushFadeValue - 1;
    if (pushFadeValue <= minPWM) {
      pushFadeValue = minPWM;
      pushfadeDirection = UP;
      client.publish("inData", devicePub0); Serial.println("turning OFF device");
      publishFlag = HIGH;
      randState = HIGH;
      msgFadeValue = 0;
      analogWrite(LED, 0);
    }
  }
  analogWrite(LED, pushFadeValue);
  Serial.print("pushFadeValue: "); Serial.println(pushFadeValue);
}

void msg_fade() {

  //Serial.println("Data has been recived...");

  analogWrite(LED, msgFadeValue);
  Serial.print("msgFadeValue: ");
  Serial.println(msgFadeValue);

  if (msgfadeDirection == UP) {
    msgFadeValue = msgFadeValue + fadeIncrement;
    if (msgFadeValue >= maxPWM) {
      msgFadeValue = maxPWM;
      msgCounter = msgCounter + fadeIncrement;
      if (msgCounter == 2000) {
        msgfadeDirection = DOWN;
        msgCounter = 0;
      }
    }
  }
  else if (msgfadeDirection == DOWN) {
    msgFadeValue = msgFadeValue - fadeIncrement;
    if (msgFadeValue <= minPWM) {
      msgFadeValue = minPWM;
      msgfadeDirection = UP;
      msgState = HIGH;
      randState = HIGH;
      analogWrite(LED, 0);
    }
  }
}

void random_fade(int minVal, int maxVal) {

  if ( randState ) {
    randNumber = getRandomNumber(minVal, maxVal);
    //Serial.print("New random number: "); Serial.println(randNumber);
  }

  if ( randNumber == 2 ) {  //could be any number between min and max
    Serial.print("Fixed random number: "); Serial.println(randNumber);
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
      }
      if (fadeValue == minRandPWM) {
        randState = HIGH;
        randNumber = 0;
      }
    }
    Serial.print("fadeValue: "); Serial.println(fadeValue);
  }
}



