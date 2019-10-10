#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>

#include <driver/dac.h>
#include <driver/ledc.h>
#include <esp_err.h>
/*#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>*/

#include <Arduino_JSON.h>
#include "map"
#include "vector"

#include "Tigra.h"


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

static const IPAddress local_ip(192, 168, 13, 37);
static const IPAddress gateway(192, 168, 0, 1);
static const IPAddress subnet(255, 255, 0, 0);


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

static TigraServer tigra(std::map<String, TigraDevice> ({
    /*{
        "Gyro", TigraDevice (std::map<String, TigraSensor> ({
            {
                "X", TigraSensor (read_only, "h",
                    axis_reader(GYRO_SENS, Gyro_X0_Reg, Gyro_X1_Reg),
                    return_1)
            },
            {
                "Y", TigraSensor (read_only, "h",
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
    },*/
    {
        "Blink", TigraDevice (std::map<String, TigraSensor> ({
            {
                "A", TigraSensor (write_only, "B",
                    []() -> SensorValue {
                        return SensorValue();
                    },
                    [](const SensorValue& bts) -> int {
                        uint32_t val = 0;
                        size_t size = bts.size();
                        for (unsigned int i = 0; i < size; i++)
                            val = val * 256 + bts[i];
                        // dac_output_voltage(DAC_CHANNEL_1, val);
                        return ledc_set_duty_and_update(
                            LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, val, 0
                        );
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
    Serial.begin(115200);
    delay(100);

    WiFi.mode(WIFI_AP);
    WiFi.softAP("esp32", "hse-only!");
    delay(100);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    delay(100);
    Serial.print("My IP is: ");
    Serial.println(WiFi.softAPIP());

    // dac_output_enable(DAC_CHANNEL_1);
    ledc_timer_config_t ledc_timer = {
        LEDC_HIGH_SPEED_MODE,
        LEDC_TIMER_20_BIT,
        LEDC_TIMER_0,
        45,
    };
    Serial.print("ledc_timer_config() -> ");
    Serial.print(ledc_timer_config(&ledc_timer));
    delay(100);
    Serial.println("");

    ledc_channel_config_t ledc_channel = {
        33,
        LEDC_HIGH_SPEED_MODE,
        LEDC_CHANNEL_0,
        LEDC_INTR_DISABLE,
        LEDC_TIMER_0,
        127,
        0,
    };
    Serial.print("ledc_channel_config() -> ");
    Serial.print(ledc_channel_config(&ledc_channel));
    delay(100);
    Serial.println();

    ledc_fade_func_install(0);
    delay(100);

    // Wire.begin(4, 0, 9600);

    Serial.println("");

    /*Wire.beginTransmission(ACC_SENS);
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
    Wire.write(0x18);*/


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
