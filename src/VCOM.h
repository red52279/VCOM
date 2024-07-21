#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <iconv.h>
#include <iostream>
#include <dirent.h>
#include <vector>
#include "CRC.h"
#include <regex>

#define DEVICES_PATH "/sys/devices/pci0000:00"

namespace VCOMCOMM {

#define BUFFER_SIZE 64

struct VCOMData {
    uint16_t len;
    uint8_t func_code;
    uint8_t id;
    std::vector<uint8_t> data;
};

constexpr int TYPE_NUMBER = 6;
const std::string DEVICES_TYPE[TYPE_NUMBER] = {
        "product",
        "version",
        "manufacturer",
        "idProduct",
        "idVendor",
        "serial" 
};

struct uart_data {
    int fd;
    int baud_rate;
    int data_bits;
    int stop_bits;
    char parity;
};

struct device_data {
    device_data() {
        device_information = std::vector<std::string>(TYPE_NUMBER);
    }
    bool operator <(device_data & a) {
        if (a.empty() || this->empty())
        return device_information[4] < a.device_information[4];
    }
    bool operator ==(const device_data & a) const {
        return this->device_information == a.device_information;
    }
    bool empty() {
        return this->device_information.empty() &&
            this->port.empty();
    }
    std::vector<std::string> device_information;
    std::string port;
};
std::ostream& operator<<(std::ostream& cout, device_data &data);

class VCOMCOMM {
private:
    device_data listDevicesInformation(std::string path); // 读取当前路径下的设备信息，并存储
    char m_buffer[BUFFER_SIZE]; // 接收缓冲区
    int setUartConfig() const; // 配置串口
public:
    ~VCOMCOMM() {
        close(m_uart_data.fd);
    }
    int openPort(const char* dev = "", int baud_rate = 115200, int data_bits = 8,
                    char parity = 'S', int stop_bits = 1); // 打开串口
    int findConnectableDeviceDir(const std::string base_path = DEVICES_PATH,
                                device_data data = device_data()); // 寻找可用设备

    int portRead(); // 已识别设备后读取通讯信息
    void transmit(std::vector<uint8_t> &data, uint8_t func_code = 0, uint16_t id = 1);
    device_data m_device_data; // 要连接的设备信息
    VCOMData m_data; // 发送/接受的信息
    uart_data m_uart_data;  // 串口传输相关配置
    std::vector<device_data> m_connectable_port_devices; // 检索到的可连接设备

    template <class T>
    void transmit(T &package_data, uint8_t fun_code = 0, uint16_t id = 1) {
        if (sizeof(package_data) > BUFFER_SIZE - 16) {
            std::cout << "自定义数据包过大,当前" << sizeof(package_data) << "字节（最大48字节）" << std::endl;
            return;
        }
        std::vector<uint8_t> data;
        for (int i = 0; i < sizeof(package_data); i++) {
            data.push_back(*((uint8_t*)(&package_data) + i));
        }
        transmit(data, fun_code, id);
    }
    template <class T>
    void getData(T &package_data) {
        if (sizeof(package_data) > BUFFER_SIZE - 16) {
            std::cout << "自定义数据包过大,当前" << sizeof(package_data) << "字节（最大48字节）" << std::endl;
            return;
        }
        std::vector<uint8_t> buff(this->m_data.len);
        for (int i = 0; i < buff.size(); i++) {
            buff[i] = this->m_data.data[i];
        }
        package_data = *(T*)(buff.data());
    }
};


}
