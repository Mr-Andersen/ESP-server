#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>

#include <Arduino_JSON.h>
#include "map"
#include "vector"

#include "Tigra.h"

#define DAC_ADDR 5

int GYRO_SENS;
// #define GYRO_SENS 0x86
#define Gyro_X0_Reg 0x1D
#define Gyro_X1_Reg 0x1E
#define Gyro_Y0_Reg 0x1F
#define Gyro_Y1_Reg 0x20
#define Gyro_Z0_Reg 0x21
#define Gyro_Z1_Reg 0x22

#define ACC_SENS 0x53
#define Acc_Data_Format 0x31
#define Acc_Pow_Reg 0x2D
#define Acc_X0_Reg 0x32
#define Acc_X1_Reg 0x33
#define Acc_Y0_Reg 0x34
#define Acc_Y1_Reg 0x35
#define Acc_Z0_Reg 0x36
#define Acc_Z1_Reg 0x37

#define ssid "default"
#define password "set-ttl-65"

const IPAddress local_ip(192, 168, 13, 37);
const IPAddress gateway(192, 168, 0, 1);
const IPAddress subnet(255, 255, 0, 0);


std::function<std::vector<char>()> wire_reader(char sensor, const std::vector <char> &registers) {
    // Serial.print("(1) registers.size() = ");
    // Serial.println(registers.size());
    return [&sensor, &registers]() -> std::vector <char> {
        Wire.beginTransmission(sensor);
        for (auto reg: registers) {
            Wire.write(reg);
        }
        Wire.endTransmission();
        Wire.requestFrom(sensor, registers.size());
        std::vector <char> result(registers.size());
        for (char i = 0; i < registers.size(); i++) {
            result[i] = Wire.read();
        }
        return result;
    };
}


int return_1(const std::vector<char> &val) {
    return 1;
}

/*
TigraServer tigra_server;
tigra_server.add_device("Gyro", TigraDevice());
tigra_server.device("Gyro").add_sensor("X", TigraSensor(
    "read-only", "word",
    wire_reader(GYRO_SENS,
        std::vector <char> ({Gyro_X0_Reg, Gyro_X1_Reg})),
    []() -> int {return 1;}
));
tigra_server.add_device("Servo", TigraDevice());
*/

WebServer server(80);

TigraServer tigra(std::map<String, TigraDevice> ({
    {
        "Gyro", TigraDevice (std::map<String, TigraSensor> ({
            {
                "X", TigraSensor ("read-only", "word",
                    []() -> std::vector<char> {
                            Wire.beginTransmission(GYRO_SENS);
                            Wire.write(Gyro_X0_Reg);
                            Wire.write(Gyro_X1_Reg);
                            Wire.endTransmission();
                            Wire.requestFrom(GYRO_SENS, 2);
                            return {Wire.read(), Wire.read()};
                        },
                    return_1)
            },
            {
                "Y", TigraSensor ("read-only", "word",
                    []() -> std::vector<char> {
                            Wire.beginTransmission(GYRO_SENS);
                            Wire.write(Gyro_Y0_Reg);
                            Wire.write(Gyro_Y1_Reg);
                            Wire.endTransmission();
                            Wire.requestFrom(GYRO_SENS, 2);
                            return {Wire.read(), Wire.read()};
                        },
                    return_1)
            },
            {
                "Z", TigraSensor ("read-only", "word",
                    []() -> std::vector<char> {
                            Wire.beginTransmission(GYRO_SENS);
                            Wire.write(Gyro_Z0_Reg);
                            Wire.write(Gyro_Z1_Reg);
                            Wire.endTransmission();
                            Wire.requestFrom(GYRO_SENS, 2);
                            return {Wire.read(), Wire.read()};
                        },
                    return_1)
            }
        }))
    },
    {
        "Acc", TigraDevice (std::map<String, TigraSensor> ({
            {
                "X", TigraSensor ("read-only", "word",
                    []() -> std::vector<char> {
                            Wire.beginTransmission(ACC_SENS);
                            Wire.write(Acc_X0_Reg);
                            Wire.write(Acc_X1_Reg);
                            Wire.endTransmission();
                            Wire.requestFrom(ACC_SENS, 2);
                            return {Wire.read(), Wire.read()};
                        },
                    return_1)
            },
            {
                "Y", TigraSensor ("read-only", "word",
                    []() -> std::vector<char> {
                            Wire.beginTransmission(ACC_SENS);
                            Wire.write(Acc_Y0_Reg);
                            Wire.write(Acc_Y1_Reg);
                            Wire.endTransmission();
                            Wire.requestFrom(ACC_SENS, 2);
                            return {Wire.read(), Wire.read()};
                        },
                    return_1)
            },
            {
                "Z", TigraSensor ("read-only", "word",
                    []() -> std::vector<char> {
                            Wire.beginTransmission(ACC_SENS);
                            Wire.write(Acc_Z0_Reg);
                            Wire.write(Acc_Z1_Reg);
                            Wire.endTransmission();
                            Wire.requestFrom(ACC_SENS, 2);
                            return {Wire.read(), Wire.read()};
                        },
                    return_1)
            }
        }))
    },
    {
        "ADC", TigraDevice (std::map<String, TigraSensor> ({
            {
                "A", TigraSensor (write_only, "B",
                    []() -> SensorValue { return {}; },
                    [](const SensorValue& bytes) -> int {
                        if (bytes.size() != 1)
                            return 1;
                        unsigned char value = bytes[0];
                        dacWrite(DAC_ADDR, value);
                        return 0;
                    })
            }
        }))
    }
}));


void handleRoot() {
    char temp[625];
    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;

    snprintf(temp, 625,
    "<html>\
      <head>\
        <meta http-equiv='refresh' content='5'/>\
        <title>ESP32 Server</title>\
        <style>\
          body {\
            background-color: #cccccc;\
            font-family: Arial, Helvetica, Sans-Serif;\
            Color: #000088;\
          }\
        </style>\
      </head>\
      <body>\
        <h1>Hello from ESP32!</h1>\
        <p>Uptime: %02d:%02d:%02d</p>\
        - GET or POST <a href='/devices'>here</a> for list of devices;<br>\
        - GET or POST url like `/device/&ltdevicename&gt` for devicename's options;<br>\
        - GET or POST url like `/device/&ltdevicename&gt/&ltsensorname&gt` for sensorname's options;<br>\
      </body>\
    </html>", hr, min % 60, sec % 60);
    server.send(200, "text/html", temp);
}


void setup() {
    Serial.begin(115200);
    delay(100);
    Wire.begin(4, 0, 9600);

    Serial.println("");

    Wire.beginTransmission(ACC_SENS);
    Wire.write(Acc_Pow_Reg);
    Wire.write(8);
    Serial.print("Transmission result: ");
    Serial.println(Wire.endTransmission());

    bool on_acc = false;
    for (int i = 0; i < 256; i++) {
        Wire.beginTransmission(i);
        uint8_t res = Wire.endTransmission();
        if (!res && on_acc) {
            GYRO_SENS = i;
            break;
        }
        if (i == ACC_SENS)
            on_acc = true;
        /*if (res == 0) {
            Serial.print("Success with i = ");
            Serial.println(i);
        }*/
    }

    Wire.beginTransmission(GYRO_SENS);
    Wire.write(0x3E);
    Wire.write(0x00);

    // Wire.write(0x15);
    // Wire.write(0x07);

    Wire.write(0x16);
    Wire.write(0x18); // 0x1E

    // Wire.write(0x17);
    // Wire.write(0x00);
    Serial.print("Transmission result: ");
    Serial.println(Wire.endTransmission());

    // WiFi.mode(WIFI_STA);
    // WiFi.begin(ssid, password);
    // WiFi.config(IP_ADDR, GATEWAY_IP, SUBNET, DNS_IP);
    // WiFi.begin(ssid);
    Serial.println("");

    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP("esp32", "hse-only!");
    Serial.print("My IP is: ");
    Serial.println(WiFi.softAPIP());
    /*
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    */


    if (MDNS.begin("esp32")) {
        Serial.println("MDNS responder started");
    }

    server.on("/", handleRoot);
    server.on("/devices", [&tigra]() {
        server.send(
            200, "application/json",
            JSON.stringify(tigra.devices())
        );
    });
    server.on("/device/{}", [&tigra]() {
        server.send(
            200, "application/json",
            JSON.stringify(tigra.device(server.pathArg(0)).options())
        );
    });
    server.on("/device/{}/sensor/{}", [&tigra]() {
        server.send(
            200, "application/json",
            JSON.stringify(tigra.device(server.pathArg(0)).sensor(server.pathArg(1)).options())
        );
    });
    server.on("/get_value/{}/{}", [&tigra]() {
        std::vector <char> val = tigra.device(server.pathArg(0)).sensor(server.pathArg(1)).get();
        JSONVar res;
        for (int i = 0; i < val.size(); i++) {
            res[i] = val[i];
        }
        server.send(
            200, "application/json",
            JSON.stringify(res)
        );
    });

    server.begin();
    Serial.println("HTTP server started");
}


void loop() {
    server.handleClient();
}
