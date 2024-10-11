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
#include <utility>
#include <vector>
#include "CRC.h"
#include <regex>
#include <functional>
#include <thread>
#include <deque>
#include <mutex>
#include <map>
#include <atomic>


// 不同linux发行版路径可能不同，此路径为Ubuntu22.04.1
#define DEVICES_PATH "/sys/devices/pci0000:00"

//最大缓冲区大小，不可修改
#define BUFFER_SIZE 64

namespace VCOM {

    struct uart_data {
        int fd;
        int baud_rate;
        int data_bits;
        int stop_bits;
        char parity;
    };

// 依次打印的串口设备信息
    static constexpr int TYPE_NUMBER = 6;
    const std::string DEVICES_TYPE[TYPE_NUMBER] = {
            "product",
            "version",
            "manufacturer",
            "idProduct",
            "idVendor",
            "serial"
    };

    class VCOM {
    public:
        VCOM() : uart_data_{-1}, is_running_(false), terminal_monitor_(false) {

        }

        ~VCOM() {
            if (is_running_) stop();
        }

        /**
         * @brief 回调函数类型
         */
        using CallbackType = std::function<void(void*)>;

        int findConnectableDevice(const std::string base_path = DEVICES_PATH);

        void callBackFuncRegister(CallbackType callBackFunc, uint8_t func_code, uint16_t id);

        void setDataBufferSize(uint8_t func_code, uint16_t id, int max_size);

        void setTerminalMap(uint8_t func_code, uint16_t id, bool is_open);

        void setTransmitPriority(uint8_t func_code, uint16_t id, int priority);

        void clearDataBuffer();

        void clearDataBuffer(uint8_t func_code, uint16_t id);

        void transmit(std::vector<uint8_t> &data, uint8_t func_code, uint16_t id, bool is_open = false);

        void start(double hz = 100);

        void stop();

        /**
         * @brief 将要发送的数据加入到待发送队列中
         *
         * @tparam T 要发送的数据类型
         * @param package_data 要发送的数据
         * @param fun_code 功能码
         * @param id 识别码
         */
        template <class T>
        void transmit(T package_data, uint8_t fun_code, uint16_t id, bool terminal = false) {
            if (sizeof(T) > BUFFER_SIZE - 16) {
                std::cout << "自定义数据包过大,当前" << sizeof(T) << "字节（最大48字节）" << std::endl;
                return;
            }
            VCOMData tdata;
            tdata.id = id;
            tdata.func_code = fun_code;
            tdata.time = std::chrono::system_clock::now();
            tdata.data.reserve(sizeof(package_data));
            for (int i = 0; i < sizeof(package_data); i++) {
                tdata.data.push_back(*((uint8_t *) (&package_data) + i));
            }
            {
                std::unique_lock<std::mutex> lock(transmit_queue_mutex_);
                if (transmit_queue_.empty()) {
                    transmit_queue_.emplace_front(tdata, terminal);
                    return;
                }
                for (auto it_ = transmit_queue_.begin(); it_ != transmit_queue_.end(); it_++) {
                    int to_insert_p = getTransmitPriority(tdata.func_code, tdata.id);
                    int it_p = getTransmitPriority(it_->first.func_code, it_->first.id);
                    if (to_insert_p > it_p) continue;
                    transmit_queue_.insert(it_, std::make_pair(tdata, terminal));
                    return;
                }
                transmit_queue_.emplace_back(tdata, terminal);
                return;
            }
        }

        /**
         * @brief 读取已经解包的数据队列,获取所有新数据
         *
         * @tparam T 功能码和识别码对应的要读取的数据类型
         * @param package_data 接收数据的动态数组
         * @param fun_code 功能码
         * @param id 识别码
         */
        template <class T>
        int getNewData(std::vector<T> &package_data, uint8_t func_code, uint16_t id, bool is_open = false) {
            auto time = std::chrono::system_clock::now();
            int receive_count = 0;
            setTerminalMap(func_code, id, is_open);
            if (sizeof(T) > BUFFER_SIZE - 16) {
                std::cout << "自定义数据包过大,当前" << sizeof(T) << "字节（最大48字节）" << std::endl;
                return false;
            }
            {
                std::unique_lock<std::mutex> lock(data_queue_mutex_);
                auto data_it_ = data_queue_.find(std::make_pair(func_code, id));
                if (data_it_ != data_queue_.end()) {
                    for (VCOMData &qdata: data_it_->second) {
                        if (qdata.func_code == func_code && qdata.id == id && qdata.time > time) {
                            package_data.push_back(*((T *) (qdata.data.data())));
                        }
                    }
                    data_it_->second.clear();
                }

            }
        }

        /**
         * @brief 读取已经解包的数据队列,获取新的单个数据
         *
         * @tparam T 功能码和识别码对应的要读取的数据类型
         * @param package_data 接收数据的指针
         * @param fun_code 功能码
         * @param id 识别码
         */
        template <class T>
        bool getNewData(T *package_data_ptr, uint8_t func_code, uint16_t id, int count = 1, bool is_open = false) {
            auto time = std::chrono::system_clock::now();
            int receive_count = 0;
            setTerminalMap(func_code, id, is_open);
            while (true) {
                if (receive_count >= count) return true;
                if (sizeof(T) > BUFFER_SIZE - 16) {
                    std::cout << "自定义数据包过大,当前" << sizeof(T) << "字节（最大48字节）" << std::endl;
                    return false;
                }
                {
                    std::unique_lock<std::mutex> lock(data_queue_mutex_);
                    auto data_it_ = data_queue_.find(std::make_pair(func_code, id));
                    if (data_it_ == data_queue_.end()) return false;
                    std::deque<VCOMData> to_remove;
                    bool is_found = false;
                    for (VCOMData &qdata: data_it_->second) {
                        if (qdata.func_code == func_code && qdata.id == id) {
                            to_remove.push_back(qdata);
                            if (qdata.time > time) {
                                is_found = true;
                                receive_count++;
                                break;
                            }
                        }
                    }
                    if (is_found && package_data_ptr != nullptr) {
                        *package_data_ptr = *((T *) (to_remove.back().data.data()));
                    }
                    for (const VCOMData &data: to_remove) {
                        data_it_->second.erase(std::remove(data_it_->second.begin(), data_it_->second.end(), data),
                                               data_it_->second.end());
                    }
                }
            }
        }


        /**
         * @brief 读取已经解包的数据队列,获取所有数据
         *
         * @tparam T 功能码和识别码对应的要读取的数据类型
         * @param package_data 接收数据的动态数组
         * @param fun_code 功能码
         * @param id 识别码
         */
        template <class T>
        int getData(std::vector<T> &package_data, uint8_t func_code, uint16_t id, bool is_open = false) {
            if (sizeof(T) > BUFFER_SIZE - 16) {
                std::cout << "自定义数据包过大,当前" << sizeof(T) << "字节（最大48字节）" << std::endl;
                return false;
            }
            setTerminalMap(func_code, id, is_open);
            std::unique_lock<std::mutex> lock(data_queue_mutex_);
            auto data_it_ = data_queue_.find(std::make_pair(func_code, id));
            if (data_it_ == data_queue_.end()) return 0;
            int count = 0;
            for (VCOMData &qdata: data_it_->second) {
                if (qdata.func_code == func_code && qdata.id == id) {
                    ++count;
                    package_data.push_back(*((T*)(qdata.data.data())));
                }
            }
            data_it_->second.clear();
            return count;
        }

        /**
         * @brief 读取已经解包的数据队列,获取单个数据
         *
         * @tparam T 功能码和识别码对应的要读取的数据类型
         * @param package_data 接收数据的指针
         * @param fun_code 功能码
         * @param id 识别码
         */
        template <class T>
        bool getData(T *package_data_ptr, uint8_t func_code, uint16_t id, bool is_open = false) {
            if (sizeof(T) > BUFFER_SIZE - 16) {
                std::cout << "自定义数据包过大,当前" << sizeof(T) << "字节（最大48字节）" << std::endl;
                return false;
            }
            setTerminalMap(func_code, id, is_open);
            std::unique_lock<std::mutex> lock(data_queue_mutex_);
            auto data_it_ = data_queue_.find(std::make_pair(func_code, id));
            if (data_it_ == data_queue_.end()) return false;
            for (VCOMData &qdata: data_it_->second) {
                if (qdata.func_code == func_code && qdata.id == id) {
                    if (package_data_ptr != nullptr) {
                        *package_data_ptr = *((T*)(qdata.data.data()));
                    }
                    data_it_->second.pop_front();
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief 打开后发送的数据与正确接收的数据会被输出到终端
         */
        bool terminal_monitor_;

        /**
         * @brief 串口传输相关配置(一般默认即可)
         */
        uart_data uart_data_;

    private:
        struct CallBackFuncInfo {
            CallbackType func;
            uint8_t func_code;
            uint16_t id;
        };

        struct VCOMData {
            uint8_t func_code;
            uint16_t id;
            std::chrono::system_clock::time_point time;
            std::vector<uint8_t> data;
            bool operator==(const VCOMData &other) const {
                return func_code == other.func_code && id == other.id && data == other.data && time == other.time;
            }
        };

        struct device_data {
            device_data() {
                device_information = std::vector<std::string>(TYPE_NUMBER);
            }
            bool operator <(const device_data &a) {
                if (a.empty() || this->empty())
                    return device_information[4] < a.device_information[4];
                return false;
            }
            bool operator ==(const device_data &a) const {
                return this->device_information == a.device_information;
            }
            bool empty() const {
                return this->device_information.empty() && this->port.empty();
            }
            std::vector<std::string> device_information;
            std::string port;
            friend std::ostream& operator<<(std::ostream& cout, const device_data& data) {
                for (int i = 0; i < 6; i++)
                    cout << DEVICES_TYPE[i] << " : " << data.device_information[i];
                cout << "port : " << data.port;
                return cout;
            }
        };

        void transmitQueueFrontData();

        int openPort(const char *dev = "", int baud_rate = 115200, int data_bits = 8,
                     char parity = 'S', int stop_bits = 1); // 打开串口

        std::vector<device_data> connectable_port_devices_; // 检索到的可连接设备
        device_data device_data_; // 要连接的设备信息

        int portRead(); // 已识别设备后读取通讯信息

        int processData(); // 读取数据后对数据进行处理
        void listDevicesInformation(std::string path); // 读取当前路径下的设备信息，并存储
        int setUartConfig() const; // 配置串口
        uint8_t buffer_[BUFFER_SIZE]; // 接收缓冲区

        std::map<std::pair<uint8_t, uint16_t>, std::vector<CallBackFuncInfo>> callBackFuncMap_; // 储存回调函数
        void tryStartCallBackFunc();

        bool getTerminalMap(uint8_t func_code, uint16_t id);

        int getTransmitPriority(uint8_t func_code, uint16_t id);

        std::atomic<bool> is_running_;

        std::thread process_thread_;
        std::mutex data_queue_mutex_;
        std::map<std::pair<uint8_t, uint16_t>, std::deque<VCOMData>> data_queue_; // 解包数据队列
        std::map<std::pair<uint8_t, uint16_t>, int> data_count_max_; // 设定的最大个数

        std::thread read_thread_;
        std::mutex read_queue_mutex_;
        std::deque<std::pair<std::vector<uint8_t>, std::chrono::system_clock::time_point>> read_queue_; // 串口读取队列
        std::map<std::pair<uint8_t, uint16_t>, bool> terminal_map_;

        std::thread transmit_thread_;
        std::mutex transmit_queue_mutex_;
        std::deque<std::pair<VCOMData, bool>> transmit_queue_; // 待发送数据队列
        std::map<std::pair<uint8_t, uint16_t>, int> transmit_priority_; // 发送优先级

    };
}
