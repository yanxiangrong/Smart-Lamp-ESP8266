#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <string>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

#define LAMP_PIN D1
#define BUTTON_PIN D5
#define BUTTON_LED D2

const char *ssid = "ASUS"; // key in your own SSID
const char *password = "acm2607."; // key in your own Wi-Fi access point password
const uint16_t id = 2508;
const char *host = "218.196.13.20";
const uint16_t port = 9528;

bool isLampOn = false;
bool lampStatus = false;
bool lastButton = false;
unsigned long lastTime = 0;
unsigned long lastSendStatus = 0;
int reconnectTime = 1;
WiFiClient client;
SoftwareSerial softwareSerial(D6, D7);
SoftwareSerial softwareSerial2(D3, D0);
DFRobotDFPlayerMini player;

void connectWiFi() {
    // We start by connecting to a Wi-Fi network
    Serial.println();
    Serial.print("Connecting to WiFi ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    int i = 60;
    while (WiFi.status() != WL_CONNECTED and i) {
        delay(500);
        Serial.print(".");

        i--;
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void connectServer() {
    Serial.print("connecting to ");
    Serial.print(host);
    Serial.print(':');
    Serial.println(port);

    if (!client.connect(host, port)) {
        Serial.println("connection failed");
//        delay(5000);
//        EspClass::reset();
    }
}

void setLedStatus() {
    static double i = 0.0;
    if (isLampOn) {
        digitalWrite(BUTTON_LED, HIGH);
    } else {
        int val = (int) (abs(sin(i)) * 127);
        analogWrite(BUTTON_LED, val);
        i += 0.001;
        if (i >= 2 * PI) {
            i = 0.0;
        }
    }
}

void setLampStatus() {
    if (isLampOn == lampStatus) return;
    unsigned long nowTime = millis();
//    digitalWrite(LAMP_PIN, isLampOn);

    if (nowTime - lastTime > 2000) {    // 离上次开关后要大于2秒才能开关，保护电灯
        lampStatus = isLampOn;
        digitalWrite(LAMP_PIN, isLampOn);
        player.play(isLampOn ? 2 : 3);
        lastTime = nowTime;
    }
}

void getButtonStatus() {
    bool buttonStatus = digitalRead(BUTTON_PIN);
    if (buttonStatus != lastButton) {
        lastButton = buttonStatus;
        isLampOn = !buttonStatus;
    }
}

void sendStatus() {
    unsigned long nowTime = millis();

    if (nowTime - lastSendStatus > 2000 and client.availableForWrite()) {
        digitalWrite(LED_BUILTIN, LOW);

        byte buf[] = {0xFE, 0x06, 0x91, 0x90, (byte) (id >> 8), (byte) (id & 0xFF), '0', ',',
                      (byte) (isLampOn + '0'), ',', '0', '.', isLampOn ? (byte) '6' : (byte) '1',
                      0xFF};
        client.write(buf, sizeof(buf));
        client.flush();
        lastSendStatus = nowTime;
        digitalWrite(LED_BUILTIN, HIGH);
    }
}

void acceptCommand() {
    static uint8_t buf[20];
    static int i = 0;

    if (!client.available()) {
        return;
    }

    digitalWrite(LED_BUILTIN, LOW);

    Serial.println("receive command");
    uint16_t commandId;
    while (client.available()) {
        auto ch = (uint8_t) client.read();
        buf[i] = ch;
        i++;

        if (ch == 0xFE) {
            i = 1;
        }
        Serial.print((int) ch);
        Serial.print(',');

        if (ch == 255) {
            commandId = ((uint16_t) buf[4] << 8) | (uint16_t) buf[5];
            if (commandId == id) {
                int setStatus = buf[7];
                isLampOn = setStatus;
            }
            Serial.println();
            break;
        }
        if ((unsigned int) i > sizeof(buf)) {
            Serial.println();
            i = 0;
        }
    }

    digitalWrite(LED_BUILTIN, HIGH);
}

void voiceInit() {
    softwareSerial.begin(9600);
    softwareSerial.println("PasswordTrigger,0,$");
    delay(50);
}

void getVoiceCommand() {
    if (!softwareSerial.available()) {
        return;
    }
    int result = 0;
    while (softwareSerial.available()) {
        result = softwareSerial.read();
    }
    if (result == 1) {
        isLampOn = true;
        return;
    }
    if (result == 2) {
        isLampOn = false;
        return;
    }
}

void setup() {
// write your initialization code here
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(LAMP_PIN, OUTPUT);
    pinMode(BUTTON_LED, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(0, INPUT_PULLUP);
    digitalWrite(LED_BUILTIN, LOW);
    lastButton = !digitalRead(BUTTON_PIN);

    Serial.begin(115200);
    softwareSerial2.begin(9600);
    delay(100);
    voiceInit();

    Serial.println();
    Serial.println();
    Serial.print("Client ID: ");
    Serial.println(id);
    connectWiFi();

    connectServer();

    player.begin(softwareSerial2);
    Serial.println("DFPlayer Mini online.");
    player.volume(30);
    player.play(1);

    digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
// write your code here
    static int lastReconnected = 0;
    if (!client.connected() and ((millis() - lastReconnected) / 1000 >= reconnectTime)) {
        digitalWrite(LED_BUILTIN, LOW);
        Serial.println();
        Serial.println("Reconnecting");
        if (!WiFi.isConnected()) {
            WiFi.reconnect();
        }
        if (WiFi.status() == WL_CONNECTED) {
            connectServer();
        }
//        if (!client.connected()) {
//            EspClass::reset();
//        }
        lastReconnected = (int) millis();
        if (reconnectTime < 16) {
            reconnectTime *= 2;
        }
        digitalWrite(LED_BUILTIN, HIGH);
    }

    getVoiceCommand();
    acceptCommand();
    getButtonStatus();
    setLampStatus();
    setLedStatus();
    sendStatus();
}