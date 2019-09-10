#ifndef TIGRA_H
#define TIGRA_H

#include <Arduino.h>


// В этом файле содержатся несколько классов, отвечающие ресурсам,
// которыми располагает микроконтроллер. Всё устройство - это TigraServer
// (изначально предполагалось, что в коде такой TigraServer конструируется,
// а потом запускается одним методом и самостоятельно обрабатывает запросы
// (т.е. server.on(...) в void setup() {...} быть не должно), но мне
// не удалось сделать наследование TigraServer : WebServer). TigraServer
// в качестве полей содержит TigraDevice (пример девайса: гироскоп). Они,
// в свою очередь, содержат в себе TigraSensor - это уже собственно что-то,
// с чего можно снимать данные, или писать на них данные (в случае с
// гироскопом это угловое ускорение по трём осям, т.е. три read-only сенсора).
// По задумке автора, такая структура должна отвечать реальной иерархии
// датчиков и потому быть удобной.


typedef std::vector<char> SensorValue;
typedef std::function<SensorValue()> reader_func;
typedef std::function<int(SensorValue)> writer_func;
enum rw_mode {
    read_write,
    read_only,
    write_only
};


// Класс, отвечающий за некоторый сенсор
class TigraSensor {
private:
    const reader_func _get_value;
    const writer_func _set_value;
    const rw_mode _mode;
    const String _type;

public:
    TigraSensor():
        _get_value(nullptr),
        _set_value(nullptr),
        _mode(),
        _type() {}

    TigraSensor(const TigraSensor &sensor):
        _get_value(sensor._get_value),
        _set_value(sensor._set_value),
        _mode(sensor._mode),
        _type(sensor._type) {}

    TigraSensor(rw_mode mode, String type,
                const reader_func get_value,
                const writer_func set_value):
        _mode(mode),
        _type(type),
        _get_value(get_value),
        _set_value(set_value) {}

    const rw_mode mode() {
        return _mode;
    }
    const String type() {
        return _type;
    }

    SensorValue get() const {
        return _get_value();
    }

    int set(const SensorValue &vec) const {
        return _set_value(vec);
    }

    JSONVar options() {
        JSONVar result;
        result["type"] = _type;
        if (_mode == read_write)
            result["mode"] = "read_write";
        else if (_mode == read_only)
            result["mode"] = "read_only";
        else if (_mode == write_only)
            result["mode"] = "write_only";
        auto value = get();
        for (unsigned int i = 0; i < value.size(); i++)
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
