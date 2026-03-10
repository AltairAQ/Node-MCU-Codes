/*****************************************************************
 ESP32 DEVKIT V1 - ESP-NOW MESH NODE
*****************************************************************/

#include <WiFi.h>
#include <esp_now.h>

#define NODE_ID 1
#define MAX_PEERS 20

#define TYPE_HELLO 0
#define TYPE_MSG   1

typedef struct Packet {
  uint8_t type;
  uint8_t sender;
  uint8_t receiver;
  uint8_t hop;
  char message[120];
} Packet;

Packet packet;

uint8_t peerMacs[MAX_PEERS][6];
int peerCount = 0;


/************ ADD PEER ************/
void addPeer(const uint8_t *mac)
{
  for(int i=0;i<peerCount;i++)
  {
    if(memcmp(peerMacs[i], mac, 6) == 0)
      return;
  }

  if(peerCount >= MAX_PEERS) return;

  memcpy(peerMacs[peerCount], mac, 6);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if(esp_now_add_peer(&peerInfo) == ESP_OK)
  {
    Serial.print("Peer added: ");

    for(int i=0;i<6;i++){
      Serial.printf("%02X", mac[i]);
      if(i<5) Serial.print(":");
    }

    Serial.println();
  }

  peerCount++;
}


/************ SEND TO ALL PEERS ************/
void broadcast(Packet *p)
{
  for(int i=0;i<peerCount;i++)
  {
    esp_now_send(peerMacs[i], (uint8_t*)p, sizeof(Packet));
  }
}


/************ HELLO DISCOVERY ************/
void sendHello()
{
  Packet p;

  p.type = TYPE_HELLO;
  p.sender = NODE_ID;
  p.receiver = 255;
  p.hop = 0;

  strcpy(p.message,"HELLO");

  uint8_t broadcastMac[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

  esp_now_send(broadcastMac,(uint8_t*)&p,sizeof(p));

  Serial.println("HELLO broadcast sent");
}


/************ SEND CALLBACK ************/
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
  Serial.print("Send Status: ");
  Serial.println(status==ESP_NOW_SEND_SUCCESS?"OK":"FAIL");
}


/************ RECEIVE CALLBACK ************/
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len)
{
  const uint8_t *mac = info->src_addr;

  memcpy(&packet,incomingData,sizeof(packet));

  addPeer(mac);

  Serial.println("\n--- Packet Received ---");

  Serial.print("From Node: ");
  Serial.println(packet.sender);

  Serial.print("To Node: ");
  Serial.println(packet.receiver);

  Serial.print("Message: ");
  Serial.println(packet.message);

  if(packet.receiver==NODE_ID || packet.receiver==255)
  {
    Serial.println("Delivered locally.");
  }

  if(packet.receiver!=NODE_ID)
  {
    packet.hop++;

    if(packet.hop<5)
    {
      broadcast(&packet);
    }
  }
}


/************ SERIAL TERMINAL ************/
void handleSerial()
{
  if(!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if(cmd.startsWith("send"))
  {
    int a = cmd.indexOf(' ');
    int b = cmd.indexOf(' ',a+1);

    int receiver = cmd.substring(a+1,b).toInt();
    String msg = cmd.substring(b+1);

    Packet p;

    p.type = TYPE_MSG;
    p.sender = NODE_ID;
    p.receiver = receiver;
    p.hop = 0;

    msg.toCharArray(p.message,120);

    broadcast(&p);
  }

  else if(cmd.startsWith("broadcast"))
  {
    String msg = cmd.substring(10);

    Packet p;

    p.type = TYPE_MSG;
￼
￼
￼

    p.sender = NODE_ID;
    p.receiver = 255;
    p.hop = 0;

    msg.toCharArray(p.message,120);

    broadcast(&p);
￼
￼
￼

  }
}


/************ SETUP ************/
void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println("BOOT OK");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if(esp_now_init()!=ESP_OK)
  {
    Serial.println("ESP-NOW INIT FAILED");
    return;
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  Serial.print("Node Started. ID=");
  Serial.println(NODE_ID);

  delay(2000);

  sendHello();
}


/************ LOOP ************/
void loop()
{
  handleSerial();
}