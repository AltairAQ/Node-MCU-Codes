#include <WiFi.h>           // remove this line for ESP8266
#include "painlessMesh.h"

#define MESH_PREFIX "MeshNet"
#define MESH_PASSWORD "12345678"
#define MESH_PORT 5555

painlessMesh mesh;

String lastMsgID = "";
const String SECRET_KEY = "meshkey123";   // encryption key

//--------------------------------------------------
// Simple XOR encryption
//--------------------------------------------------

String encryptDecrypt(String msg) {

  String output = "";

  for (int i = 0; i < msg.length(); i++) {
    char c = msg[i] ^ SECRET_KEY[i % SECRET_KEY.length()];
    output += c;
  }

  return output;
}

//--------------------------------------------------
// Message receive callback
//--------------------------------------------------

void receivedCallback(uint32_t from, String &msg) {

  int p1 = msg.indexOf('|');
  int p2 = msg.indexOf('|', p1 + 1);
  int p3 = msg.indexOf('|', p2 + 1);

  String msgID = msg.substring(0, p1);
  String sender = msg.substring(p1 + 1, p2);
  String receiver = msg.substring(p2 + 1, p3);
  String encryptedMsg = msg.substring(p3 + 1);

  // prevent loops
  if (msgID == lastMsgID) return;
  lastMsgID = msgID;

  //--------------------------------------------------
  // broadcast message
  //--------------------------------------------------

  if (receiver == "ALL") {

    String decrypted = encryptDecrypt(encryptedMsg);

    Serial.println("📢 Broadcast Message");
    Serial.print("From Node: ");
    Serial.println(sender);

    Serial.print("Message: ");
    Serial.println(decrypted);
    Serial.println();

    mesh.sendBroadcast(msg);
    return;
  }

  //--------------------------------------------------
  // private message
  //--------------------------------------------------

  if (receiver == String(mesh.getNodeId())) {

    String decrypted = encryptDecrypt(encryptedMsg);

    Serial.println("🔐 Encrypted Message Received");
    Serial.print("From Node: ");
    Serial.println(sender);

    Serial.print("Decrypted Message: ");
    Serial.println(decrypted);
    Serial.println();

  } else {

    // forward encrypted message
    mesh.sendBroadcast(msg);

  }
}

//--------------------------------------------------
// Setup
//--------------------------------------------------

void setup() {

  Serial.begin(115200);

  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  mesh.onReceive(&receivedCallback);

  Serial.println("Secure Mesh Started");

  Serial.print("My Node ID: ");
  Serial.println(mesh.getNodeId());
}

//--------------------------------------------------
// Main loop
//--------------------------------------------------

void loop() {

  mesh.update();

  if (Serial.available()) {

    String input = Serial.readStringUntil('\n');

    int split = input.indexOf('|');

    String receiver = input.substring(0, split);
    String message = input.substring(split + 1);

    // encrypt message
    String encrypted = encryptDecrypt(message);

    String msgID = String(random(100000,999999));

    String packet = msgID + "|" + String(mesh.getNodeId()) + "|" + receiver + "|" + encrypted;

    mesh.sendBroadcast(packet);
  }
