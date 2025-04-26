#include <Wire.h>
#include "MAX30105.h" 
#include "heartRate.h"
#include "DHT.h"
#include <FirebaseESP32.h>
#include <FirebaseESP32HTTPClient.h>
#include <FirebaseJson.h>
#include "WiFi.h"
#define FB_HOST "" // Type FireBase host address here
#define FB_AUTH "" // Type auth key here
#define WSSID "Lelouch" // SSID (doesnt really matter if it gets pushed to the repo I guess)
#define WPASS "boom1234" // Password
#define DHTTYPE DHT11
#define DHTPIN 18     
DHT dht(DHTPIN, DHTTYPE);
MAX30105 particleSensor;

double aved = 0; double avir = 0;
double sirrms = 0;
double srrms = 0;
double h;
int i = 0;
int N_r = 100;

double ESpO2 = 95.0;
double FSpO2 = 0.7; 
double f_rate = 0.95; 
#define TIMETOBOOT 3000
#define SCALE 88.0 
#define SAMPLING 5 
#define FINGER_ON 30000 
#define MINIMUM_SPO2 80.0

const byte R_SIZE = 4; 
byte rates[R_SIZE]; 
byte rate_Spot = 0;
long last_Beat = 0; 
float bpm;
int beat_Avg;
int j=0;

#define USEFIFO

FirebaseData firebaseData;
FirebaseJson json;

void setup()
{
  Serial.begin(115200);
  Serial.println("Initializing...");
  dht.begin();
  
  while (!particleSensor.begin(Wire, I2C_SPEED_FAST))
  {
    Serial.println("MAX30102 was not found. ");
  }
  
  byte ledBrightness = 0x7F; 
  byte sampleAverage = 4; 
  byte ledMode = 2; 
  int sampleRate = 200; 
  int pulseWidth = 411; 
  int adcRange = 16384; 
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); 
  particleSensor.enableDIETEMPRDY();

  WiFi.begin(WSSID, WPASS);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
 
  Firebase.begin(FB_HOST, FB_AUTH);
  Firebase.reconnectWiFi(true);
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  Firebase.setwriteSizeLimit(firebaseData, "tiny");
  Serial.println("------------------------------------");
  Serial.println("Connected...");

 xTaskCreate(SendToFirebase, "Send To Firebase",20000, NULL, 1, NULL); 
}


void SendToFirebase( void * parameter )
    {
        for(;;)
        {
        Serial.println("\nSent to firebase\n");
        json.set("/Value", double(beatAvg));
            Firebase.updateNode(firebaseData,"/BPM",json);
            json.set("/Value", ESpO2);
            Firebase.updateNode(firebaseData,"/SPO2",json);
            json.set("/Value", h);
            Firebase.updateNode(firebaseData,"/Humidity",json);
        }
        vTaskDelete( NULL );
    }

void loop()
{
    j++;
    uint32_t ir_dat, red_dat , green_dat;
    double f_red, f_ir;
    double SpO2_dat = 0; 
    long last_Msg = 0;
    long present = millis();
    if (present - lastMsg > 3000) {
        last_Msg = present;
    #ifdef USEFIFO
        particleSensor.check(); 
    
    while (particleSensor.available()) {
    #ifdef MAX30105
    red_dat = particleSensor.getFIFORed(); 
        ir_dat = particleSensor.getFIFOIR();  
    #else
        red_dat = particleSensor.getFIFOIR(); 
        ir_dat = particleSensor.getFIFORed(); 
    #endif

    long ir_Value = particleSensor.getIR();
 
    if (checkForBeat(ir_Value) == true)
    {

        long delta = millis() – last_Beat;
        last_Beat = millis();
        
        bpm= 60 / (delta / 1000.0);
        
        if (bpm< 255 && bpm > 20)
        {
            rates[rate_Spot++] = (byte)bpm;
            rate_Spot %= R_SIZE;

            beat_Avg = 0;
            for (byte x = 0 ; x < RATE_SIZE ; x++)
            beat_Avg += rates[x];
            beat_Avg /= RATE_SIZE;
        }
    }

    Serial.print(", Avg BPM=");
    Serial.print(beat_Avg);

    if (irValue < 10000)
        Serial.print(" No finger?");
    
    i++;
    f_red = (double)red_dat;
    f_ir = (double)ir_dat;
    aved = aved * f_rate + (double)red_dat * (1.0 – f_rate);
    avir = avir * f_rate + (double)ir_dat * (1.0 – f_rate);
    srrms += (f_red - aved) * (f_red - aved); 
    sirrms += (f_ir - avir) * (f_ir - avir);
    
    if ((i % SAMPLING) == 0) 
    {
        if ( millis() > TIMETOBOOT) {
            if (ir < FINGER_ON) ESpO2 = MINIMUM_SPO2; 
                Serial.print(" Oxygen % = ");
            if( ESpO2 >= 100)      
            {
                ESpO2=100;
                Serial.println("100");
            
            }  
            else
            {
                Serial.println(ESpO2);
            }
            }
    }
    
    if ((i % N_r) == 0) {
        double R = (sqrt(srrms) / aved) / (sqrt(sirrms) / avir);
        // Serial.println(R);
        SpO2_dat = -23.3 * (R - 0.4) + 100; //http://ww1.microchip.com/downloads/jp/AppNotes/00001525B_JP.pdf
        ESpO2 = FSpO2 * ESpO2 + (1.0 - FSpO2) * SpO2_dat;
        srrms = 0.0; sirrms = 0.0; i = 0;
        break;
        }
        particleSensor.nextSample(); 
    }
    
    h = dht.readHumidity();
    Serial.print(F("Humidity: "));
    Serial.print(h);
    Serial.print(F("\n"));  
    #endif
    }
}

