/*
 * ============================================================
 *  Secure Encrypted Mesh Communication
 *  Compatible with: ESP32 (WiFi.h) | ESP8266 (ESP8266WiFi.h)
 *
 *  Protocol:  painlessMesh over WiFi Ad-Hoc
 *  Encryption: XOR cipher (symmetric, key-based)
 *  Packet Format: <msgID>|<senderID>|<receiverID>|<encryptedPayload>
 *
 * ============================================================
 */

// ── Platform Detection ──────────────────────────────────────
// Automatically selects the correct WiFi library depending
// on the target hardware. No manual switching needed.
#ifdef ESP32
  #include <WiFi.h>          // ESP32 WiFi driver
#else
  #include <ESP8266WiFi.h>   // ESP8266 WiFi driver
#endif

#include "painlessMesh.h"

// ── Mesh Network Configuration ──────────────────────────────
// All nodes on the same mesh MUST share these credentials.
// Change before deployment — defaults are for testing only.
#define MESH_PREFIX   "MeshNet"     // SSID of the mesh network
#define MESH_PASSWORD "12345678"    // Mesh access password
#define MESH_PORT     5555          // UDP port painlessMesh uses

// ── Encryption Key ──────────────────────────────────────────
// Symmetric XOR key — both ends must use the same key.
// Longer key = less repeating pattern. Treat this as a secret.
const String SECRET_KEY = "meshkey123";

// ── Dedup Tracker ───────────────────────────────────────────
// Stores the last seen message ID to drop re-broadcast loops.
// Single-slot cache: sufficient for low-traffic tactical use.
String lastMsgID = "";

// ── Mesh Object ─────────────────────────────────────────────
painlessMesh mesh;


// ============================================================
//  XOR ENCRYPT / DECRYPT
//  Symmetric: same function encrypts and decrypts.
//  Key wraps cyclically over message length.
//  ⚠  XOR is obfuscation, not strong encryption.
//     Replace with AES if operational security is critical.
// ============================================================
String encryptDecrypt(String msg) {
  String output = "";
  for (int i = 0; i < (int)msg.length(); i++) {
    // XOR each char against the cycling key character
    char c = msg[i] ^ SECRET_KEY[i % SECRET_KEY.length()];
    output += c;
  }
  return output;
}


// ============================================================
//  RECEIVED CALLBACK
//  Fired by painlessMesh whenever this node gets a packet.
//  Handles:
//    1. Broadcast  → receiver == "ALL"
//    2. Direct     → receiver == this node's ID
//    3. Relay      → packet meant for someone else, forward it
// ============================================================
void receivedCallback(uint32_t from, String &msg) {

  // ── Parse packet fields ────────────────────────────────
  // Format: <msgID>|<sender>|<receiver>|<encryptedPayload>
  int p1 = msg.indexOf('|');
  int p2 = msg.indexOf('|', p1 + 1);
  int p3 = msg.indexOf('|', p2 + 1);

  String msgID        = msg.substring(0, p1);
  String sender       = msg.substring(p1 + 1, p2);
  String receiver     = msg.substring(p2 + 1, p3);
  String encryptedMsg = msg.substring(p3 + 1);

  // ── Deduplication guard ────────────────────────────────
  // painlessMesh can deliver the same packet multiple times
  // via different paths. Drop anything we've already processed.
  if (msgID == lastMsgID) return;
  lastMsgID = msgID;

  // ── Case 1: Broadcast message ──────────────────────────
  // Receiver field is "ALL" — meant for every node on mesh.
  // Decrypt, print, then re-broadcast to propagate further.
  if (receiver == "ALL") {
    String decrypted = encryptDecrypt(encryptedMsg);

    Serial.println("📢 [BROADCAST]");
    Serial.print("  From   : "); Serial.println(sender);
    Serial.print("  Message: "); Serial.println(decrypted);
    Serial.println();

    mesh.sendBroadcast(msg);   // propagate encrypted original
    return;
  }

  // ── Case 2: Direct message for THIS node ──────────────
  // Receiver matches our own node ID — decrypt and display.
  if (receiver == String(mesh.getNodeId())) {
    String decrypted = encryptDecrypt(encryptedMsg);

    Serial.println("🔐 [PRIVATE MESSAGE]");
    Serial.print("  From   : "); Serial.println(sender);
    Serial.print("  Message: "); Serial.println(decrypted);
    Serial.println();

  } else {
    // ── Case 3: Relay — forward to destination ─────────
    // Not for us. Re-broadcast the encrypted packet so it
    // can hop toward the intended recipient.
    mesh.sendBroadcast(msg);
  }
}


// ============================================================
//  SETUP
//  Initialises serial, mesh network, and registers callback.
// ============================================================
void setup() {
  Serial.begin(115200);

  // Start the mesh — nodes auto-discover and self-heal
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);

  // Register our receive handler
  mesh.onReceive(&receivedCallback);

  Serial.println("\n[PHANTOM] Secure Mesh Node Online");
  Serial.print("[PHANTOM] Node ID: ");
  Serial.println(mesh.getNodeId());
}


// ============================================================
//  LOOP
//  Keeps mesh alive and checks Serial for outgoing messages.
//
//  Serial input format: <receiverID>|<plaintext message>
//  Use "ALL" as receiverID for broadcast.
//
//  Example:
//    ALL|hello everyone
//    3456789012|secret message
// ============================================================
void loop() {
  mesh.update();   // must be called repeatedly — drives mesh internals

  // ── Outgoing message via Serial ────────────────────────
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();   // strip trailing \r on Windows terminals

    int split = input.indexOf('|');
    if (split == -1) return;   // malformed input, skip it

    String receiver = input.substring(0, split);
    String message  = input.substring(split + 1);

    // Encrypt before transmitting — receiver decrypts with same key
    String encrypted = encryptDecrypt(message);

    // Random 6-digit ID for dedup tracking across the mesh
    String msgID = String(random(100000, 999999));

    // Assemble and broadcast the full packet
    String packet = msgID + "|" + String(mesh.getNodeId()) + "|" + receiver + "|" + encrypted;
    mesh.sendBroadcast(packet);

    Serial.print("[PHANTOM] Sent → "); Serial.println(receiver);
  }
}
