#ifndef TIGRA_H
#define TIGRA_H

#include<Arduino.h>


class TigraSensor {
private:
    const std::function<std::vector<char>()> _get_value;
    // std::vector<char> (*const _get_value)();
    const std::function<int(const std::vector<char> &)> _set_value;
    // int (*const _set_value)(const std::vector<char> &);
    const String _mode;
    const String _type;

public:
    TigraSensor():
        _get_value(nullptr),
        _set_value(nullptr),
        _mode(NULL),
        _type(NULL) {}

    TigraSensor(const TigraSensor &sensor):
        _get_value(sensor._get_value),
        _set_value(sensor._set_value),
        _mode(sensor._mode),
        _type(sensor._type) {}

    TigraSensor(String mode, String type,
                const std::function<std::vector<char> ()> get_value,
                const std::function<int(const std::vector<char> &)> set_value):
        _mode(mode),
        _type(type),
        _get_value(get_value),
        _set_value(set_value) {}

    const String mode() {
        return _mode;
    }
    const String type() {
        return _type;
    }

    std::vector<char> get() const {
        return _get_value();
    }

    int set(const std::vector<char> &vec) const {
        return _set_value(vec);
    }

    JSONVar options() {
        JSONVar result;
        result["type"] = _type;
        result["mode"] = _mode;
        auto value = get();
        for (int i = 0; i < value.size(); i++)
            result["value"][i] = value[i];
        return result;
    }
};


class TigraDevice {
private:
    std::map<String, TigraSensor> _sensors;

public:
    TigraDevice():
        _sensors() {}

    TigraDevice(const TigraDevice &device):
        _sensors(device._sensors) {}

    TigraDevice(const std::map<String, TigraSensor> &sensors):
        _sensors(sensors) {}

    std::vector<String> sensors() const {
        std::vector<String> result;
        for (auto sensor: _sensors) {
            result.push_back(sensor.first);
        }
        return result;
    }

    TigraSensor &sensor(String sensor_name) {
        return _sensors[sensor_name];
    }

    void add_sensor(String sensor_name, const TigraSensor &sensor) {
        _sensors.insert(std::pair<String, TigraSensor> (sensor_name, sensor));
    }

    JSONVar options() {
        JSONVar result;
        for (auto sensor: _sensors) {
            result[sensor.first]["mode"] = sensor.second.mode();
            result[sensor.first]["type"] = sensor.second.type();
            auto value = sensor.second.get();
            for (int i = 0; i < value.size(); i++)
                result[sensor.first]["value"][i] = value[i];
        }
        return result;
    }
};


class TigraServer {
private:
    std::map<String, TigraDevice> _devices;
public:
    TigraServer(): _devices() {}

    TigraServer(const TigraServer &server): _devices(server._devices) {}

    TigraServer(const std::map<String, TigraDevice> &devices):
        _devices(devices) {}

    void add_device(String device_name, const TigraDevice &device) {
        _devices.insert(std::pair<String, TigraDevice> ({device_name, device}));
    }

    JSONVar devices() const {
        JSONVar result;
        int i = 0;
        for (auto device: _devices) {
            result[i] = device.first;
            i++;
        }
        return result;
    }

    TigraDevice &device(String device_name) {
        return _devices[device_name];
    }
};


#endif
