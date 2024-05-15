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

#define BUFFER_SIZE 64
#define DEVICES_PATH "/sys/devices/pci0000:00"

namespace VCOMCOMM {

constexpr int TYPE_NUMBER = 6;
const std::string DEVICES_TYPE[TYPE_NUMBER] = {
        "product",
        "version",
        "manufacturer",
        "idProduct",
        "idVendor",
        "serial" 
};

struct VCOMData {
    float y_angle;
    float p_angle;
};

struct uart_data {
    int fd;
    int baud_rate;
    int data_bits;
    int stop_bits;
    char parity;
};

struct device_data {
    bool operator <(const device_data & a) const {
        return idProduct < a.idProduct;
    }
    bool operator ==(const device_data & a) const {
        return (this->product == a.product && 
            this->version == a.version &&
            this->manufacturer == a.manufacturer &&
            this->idProduct == a.idProduct &&
            this->idVendor == a.idVendor &&
            this->serial == a.serial);
    }
    bool empty() {
        return this->product.empty() && 
            this->version.empty() &&
            this->manufacturer.empty() &&
            this->idProduct.empty() &&
            this->idVendor.empty() &&
            this->serial.empty() &&
            this->port.empty();
    }
    std::string product;
    std::string version;
    std::string manufacturer;
    std::string idProduct;
    std::string idVendor;
    std::string serial;
    std::string port;
};
std::ostream& operator<<(std::ostream& cout, device_data &data);
class VCOMCOMM {
private:
    device_data ListDevicesInformation(std::string path);
public:
    ~VCOMCOMM() {
        close(m_uart_data.fd);
    }
    int SetUartConfig() const;
    int OpenPort(const char* dev = "", int baud_rate = 115200, int data_bits = 8,
                    char parity = 'S', int stop_bits = 1);
    int PortRead();
    int FindConnectableDeviceDir(const std::string base_path = DEVICES_PATH,
                                device_data data = device_data());
    void Transmit(VCOMData data, uint8_t fun_code = 0, uint16_t id = 1);
    void Transmit(std::vector<uint8_t> &data, uint8_t fun_code = 0, uint16_t id = 1);
    char m_buffer[64];
    device_data m_device_data; // 要连接的设备信息
    VCOMData m_data; // 发送/接受的信息
    uart_data m_uart_data;  // 串口传输相关配置
    std::vector<device_data> m_connectable_port_devices; // 检索到的可连接设备
};
}
