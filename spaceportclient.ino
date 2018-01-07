#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include "MFRC522.h"
#include <WiFiClientSecure.h>
#define RST_PIN 0 // RST-PIN for RC522 - RFID - SPI - Module GPIO-0 
#define SS_PIN  15  // SDA-PIN for RC522 - RFID - SPI - Module GPIO-15
#define FCLOSED_PIN D2 // RELAY-PIN in GPI0-16
#define FOPEN_PIN D1
#define BUTTON_PIN A0
const int httpPort = 80;
const int httpsPort = 443;
bool DEBUG = true;
const bool TLS = true;

// --------------- User defined values ----------------- //
const char* ssid     = "";
const char* password = "";
const char* host = "";
const String nodeid = "";
/*
 * For User Auhentication:
 * https://en.wikipedia.org/wiki/Basic_access_authentication
 * Use user:password in base64
 * Ej: https://webnet77.net/cgi-bin/helpers/base-64.pl
 * echo -n user:password | base64
*/
const char* userpass = "";
// SHA1 fingerprint of server ssl certificate.
// The certificate's common name MUST match the host.
// openssl x509 -in <certificate> -text -noout -sha1 -fingerprint | grep "SHA1 Fingerprint" | sed 's/SHA1 Fingerprint=//g' | sed 's/:/ /g'
const char* fingerprint = "";
// ------------- End User defined values --------------- //


// -------------------- Prototypes -------------------- //
void PrintDebugLn(String  text="");// Serial print line when debug == true
void PrintDebug(String text="");// Serial print when debug == true
String ReturnUidString(byte *buffer, byte bufferSize); // Reads UID from buffer and returns UID in a formatted string
String ReturnResponse(String url); //Sends request to server and returns the response in a json readable format
bool CheckForCard(); // returns true when new card is detected
bool CheckButton(); // Reads analog pin button is tied to and returns true if button is pressed
String ReturnRequestString(String url);// builds and returns the correct request string
void Unlock(); // Performs routine to unlock door
String ParseJson(String json, String key);// Returns value for key from json string
void AdminModeNotification(); // Output to notify admin mode actions
// ------------------ End Prototypes ------------------ //


MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance
void setup() {
  // set all pin modes 
  pinMode(BUTTON_PIN, INPUT);
  pinMode(FCLOSED_PIN, OUTPUT);
  pinMode(FOPEN_PIN, OUTPUT);
  digitalWrite(FOPEN_PIN, HIGH);
  digitalWrite(FCLOSED_PIN, LOW);
  // check if the button is pressed on startup to put unit in debug mode.
  if (CheckButton()){
    DEBUG = true;
  }  
  // Initialise serial communications
  Serial.begin(115200);
  delay(10);
  SPI.begin();           
  mfrc522.PCD_Init();    
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  if (DEBUG == true){ 
  PrintDebugLn();
  PrintDebug("Connecting to ");
  PrintDebugLn(ssid);
  }
  // Initialise Wifi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (DEBUG == true){
      Serial.print(".");
    }
  }
  if (DEBUG == true){ 
  PrintDebugLn("");
  PrintDebugLn("WiFi connected");  
  PrintDebug("IP address: ");
  PrintDebugLn(WiFi.localIP().toString());
     } 
}


void loop() { 
  // Wait until new card is present.
  while(!CheckForCard()){
    return;
  }   
  String UidString = ReturnUidString(mfrc522.uid.uidByte, mfrc522.uid.size);

  
  if (CheckButton()){
    // Admin mode
    PrintDebugLn("Admin Mode, checking card has admin rights");
    String url = ("/api/1/admin/uuid/") + UidString;
    String JsonResponse = ReturnResponse(url);
    String result = ParseJson(JsonResponse, "result");
    if (result == "true"){
        PrintDebugLn("Entering Admin Mode");
        AdminModeNotification();
        // Wait until new card is present.
        while(!CheckForCard()){
          delay(1);
        }   
        String NewUidString = ReturnUidString(mfrc522.uid.uidByte, mfrc522.uid.size);
        String url = ("/api/1/admin/register/uuid/") + NewUidString;
        String JsonResponse = ReturnResponse(url);
        String result = ParseJson(JsonResponse, "result");
        if (result == "true"){  
          PrintDebugLn("New Card Registered");
          AdminModeNotification();         
        }
      
    }
      PrintDebugLn("Admin Mode end");   
  }

  else{
    // Standard unlock routine
  
  //Create a URI for the request
  String url = ("/api/1/node/") + nodeid + "/uuid/" + UidString;
  String JsonResponse = ReturnResponse(url);
  String result = ParseJson(JsonResponse, "result");

  if (result == "true"){
    Unlock();
  }
  else if (result == "false"){
    PrintDebugLn("Access Denied");
  }
  else {
          PrintDebugLn("Unknown result - DENIED");    
  }
  PrintDebugLn();
  PrintDebugLn("closing connection");
  PrintDebugLn();
  }
  
}


// ------------- Functions ----------- //

void PrintDebugLn(String text)
{
  if (DEBUG==true){
    Serial.println(text);
  }
}

void PrintDebug(String text)
{
  if (DEBUG==true){
    Serial.print(text);
  }
}

bool CheckButton(){
  if (analogRead(BUTTON_PIN) > 500){
    return true;
  }
  else{
    return false;
  }
}

String ReturnUidString(byte *buffer, byte bufferSize) {
  String UidString = "";
  for (byte i = 0; i < bufferSize; i++) {
    UidString += ".";
    UidString += buffer[i];
  }
  //remove first . to maintain compatibility.
  UidString.remove(0,1);
  PrintDebugLn("RFID Tag Detected...");
  PrintDebugLn("Card UID:");
  PrintDebugLn(UidString);
  PrintDebugLn();
  PrintDebugLn();
  return UidString;
}

bool CheckForCard(){
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return false;
  }
  // Select one of the cards
  else if ( ! mfrc522.PICC_ReadCardSerial()) {
    return false;
  }
  else{
    return true;
  } 
}

String ReturnRequestString(String url){  
   return String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Authorization: Basic " + userpass + "\r\n" + "Connection: close\r\n\r\n";  
}

void Unlock(){
        PrintDebugLn("Access Granted");
        digitalWrite(FCLOSED_PIN, HIGH);
        digitalWrite(FOPEN_PIN, LOW);
        PrintDebugLn("Door Open");
        delay(1500);
        digitalWrite(FCLOSED_PIN, LOW);
        digitalWrite(FOPEN_PIN, HIGH);
}

String ParseJson(String json, String key){
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);
  PrintDebug("Parsing for key: ");
  PrintDebugLn(key);
  PrintDebug("Value: ");
  PrintDebugLn(root[key]);
  return root[key];
}

String ReturnResponse(String url) {
  PrintDebugLn("connecting to ");
  PrintDebugLn(host);
  PrintDebug("Requesting URL: ");
  PrintDebugLn(url);

  if (TLS != true){

  WiFiClient client;
  if (!client.connect(host, httpPort)) {
    PrintDebugLn("connection failed");
  }
  // This will send the request to the server
  client.println(ReturnRequestString(url));             
  delay(10);

    // Read the response
  String line;
  PrintDebugLn("Response below: "); 
  while(client.connected()){   
    line = client.readStringUntil('\r');
    PrintDebugLn(line);
    String result = line.substring(1,2);
    
    if (result=="[") //detects the beginning of the string json
    {
      line.remove(0,2);
      line.remove(line.length()-1,2);
      PrintDebug("Json response: ");
      PrintDebugLn(line);
      return line;
    }
  }
  }
  else {
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  if (!client.connect(host, httpsPort)) {
    PrintDebugLn("connection failed");
  }

  if (client.verify(fingerprint, host)) {
    PrintDebugLn("certificate matches");
  } else {
    PrintDebugLn("certificate doesn't match");
  }
  // This will send the request to the server
  client.println(ReturnRequestString(url));             
  delay(10);
  
  // Read the response
  String line;
  PrintDebugLn("Response below: "); 
  while(client.connected()){   
    line = client.readStringUntil('\r');
    PrintDebugLn(line);
    String result = line.substring(1,2);
    
    if (result=="[") //detects the beginning of the string json
    {
      line.remove(0,2);
      line.remove(line.length()-1,2);
      PrintDebug("Json response: ");
      PrintDebugLn(line);
      return line;
    }
  } 
  }
}

void AdminModeNotification(){
         PrintDebugLn("ADMIN ACTION");
       for (int i=0; i<10; i++){
        digitalWrite(FCLOSED_PIN, HIGH);
        digitalWrite(FOPEN_PIN, LOW);
        delay(200);
        digitalWrite(FCLOSED_PIN, LOW);
        digitalWrite(FOPEN_PIN, HIGH);
        delay(200);
       }
  
}

