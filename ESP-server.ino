#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>

#include <driver/dac.h>

#include <Arduino_JSON.h>
#include "map"
#include "vector"

#include "Tigra.h"


int GYRO_SENS;
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

// #define ssid "net-name"
// #define password "password"

static const IPAddress local_ip(192, 168, 13, 37);
static const IPAddress gateway(192, 168, 0, 1);
static const IPAddress subnet(255, 255, 0, 0);


reader_func wire_reader(char sensor, std::vector<char> registers) {
    return [sensor, registers]() -> SensorValue {
        Wire.beginTransmission(sensor);
        for (auto reg: registers) {
            Wire.write(reg);
        }
        Wire.endTransmission();
        Wire.requestFrom(sensor, registers.size());
        std::vector<char> result(registers.size());
        for (char i = 0; i < registers.size(); i++) {
            result[i] = Wire.read();
        }
        return result;
    };
}


reader_func axis_reader(char sens, char reg_x0, char reg_y0) {
    return [sens, reg_x0, reg_y0]() -> SensorValue {
        Wire.beginTransmission(sens);
        Wire.write(reg_x0);
        Wire.write(reg_y0);
        Wire.endTransmission();
        Wire.requestFrom(sens, 2);
        return { Wire.read(), Wire.read() };
    };
}


int return_1(const std::vector<char> &val) {
    return 1;
}

WebServer server(80);

TigraServer tigra(std::map<String, TigraDevice> ({
    {
        "Gyro", TigraDevice (std::map<String, TigraSensor> ({
            {
                "X", TigraSensor (read_only, "h",
                    axis_reader(GYRO_SENS, Gyro_X0_Reg, Gyro_X1_Reg),
                    return_1)
            },
            {
                "Y", TigraSensor (read_only, "h",
                    /*[]() -> SensorValue {
                            Wire.beginTransmission(GYRO_SENS);
                            Wire.write(Gyro_Y0_Reg);
                            Wire.write(Gyro_Y1_Reg);
                            Wire.endTransmission();
                            Wire.requestFrom(GYRO_SENS, 2);
                            return {Wire.read(), Wire.read()};
                        },*/
                    axis_reader(GYRO_SENS, Gyro_Y0_Reg, Gyro_Y1_Reg),
                    return_1)
            },
            {
                "Z", TigraSensor (read_only, "h",
                    axis_reader(GYRO_SENS, Gyro_Z0_Reg, Gyro_Z1_Reg),
                    return_1)
            }
        }))
    },
    {
        "Acc", TigraDevice (std::map<String, TigraSensor> ({
            {
                "X", TigraSensor (read_only, "h",
                    axis_reader(ACC_SENS, Acc_X0_Reg, Acc_X1_Reg),
                    return_1)
            },
            {
                "Y", TigraSensor (read_only, "h",
                    axis_reader(ACC_SENS, Acc_Y0_Reg, Acc_Y1_Reg),
                    return_1)
            },
            {
                "Z", TigraSensor (read_only, "h",
                    axis_reader(ACC_SENS, Acc_Z0_Reg, Acc_Z1_Reg),
                    return_1)
            }
        }))
    },
    {
        "Blink", TigraDevice (std::map<String, TigraSensor> ({
            {
                "A", TigraSensor (write_only, "B",
                    []() -> SensorValue {
                        return SensorValue();
                    },
                    [](const SensorValue& bts) -> int {
                        if (bts.size() != 1)
                            return 1;
                        char val = bts[0];
                        dac_output_voltage(DAC_CHANNEL_1, val);
                        return 0;
                    })
            }
        }))
    }
}));


void handleRoot() {
    Serial.println("'/' requested");
    char temp[1024];
    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;

    snprintf(temp, 1024, "\
<html>\
    <head>\
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
</html>\
",
    hr, min % 60, sec % 60);
    server.send(200, "text/html", temp);
}


char hexToByte(char c) {
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('a' <= c && c <= 'z')
        return c - 'a' + 10;
    if ('A' <= c && c <= 'Z')
        return c - 'A' + 10;
}

SensorValue hexToSensorValue(const std::string& hexed) {
    // Order of encoding data on client side:
    // data -> vector of bytes (how sensor wants to receive them)
    //     -> each byte (256) encoded into 2 [0-9a-zA-Z] symbols (hexed)
    std::vector<char> res(hexed.size() / 2);
    for (unsigned int i = 0; i < hexed.size() / 2; i++) {
        res[i] = hexToByte(hexed[2 * i]) * 16
               + hexToByte(hexed[2 * i + 1]);
    }
    return res;
}


void setup() {
    dac_output_enable(DAC_CHANNEL_1);

    Serial.begin(115200);
    delay(100);
    Wire.begin(4, 0, 9600);

    Serial.println("");

    Wire.beginTransmission(ACC_SENS);
    Wire.write(Acc_Pow_Reg);
    Wire.write(8);

    for (int i = ACC_SENS; i < 256; i++) {
        Wire.beginTransmission(i);
        if (!Wire.endTransmission()) {
            GYRO_SENS = i;
            break;
        }
    }

    Wire.beginTransmission(GYRO_SENS);
    Wire.write(0x3E);
    Wire.write(0x00);

    Wire.write(0x16);
    Wire.write(0x18);

    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP("esp32", "hse-only!");
    Serial.print("My IP is: ");
    Serial.println(WiFi.softAPIP());


    /*if (MDNS.begin("esp32")) {
        Serial.println("MDNS responder started");
    }*/

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
        std::vector<char> val = tigra.device(server.pathArg(0)).sensor(server.pathArg(1)).get();
        JSONVar res;
        for (int i = 0; i < val.size(); i++) {
            res[i] = val[i];
        }
        server.send(
            200, "application/json",
            JSON.stringify(res)
        );
    });
    server.on("/set_value/{}/{}/{}", [&tigra]() {
        String device_name = server.pathArg(0);
        String sensor_name = server.pathArg(1);
        String hex_value = server.pathArg(2);
        SensorValue value = hexToSensorValue(hex_value.c_str());
        Serial.print("value (");
        Serial.print(value.size());
        Serial.print("): [");
        for (auto c : value) {
            Serial.print((int) c);
            Serial.print(", ");
        }
        Serial.println("];");
        int res =
            tigra.device(device_name)
                .sensor(sensor_name)
                .set(value);
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
