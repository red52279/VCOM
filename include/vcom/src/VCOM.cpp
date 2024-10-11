#include "VCOM.h"


namespace VCOM {

    /**
     * @brief 读取并返回设备信息
     *
     * @param path 当前路径
     *
     */
    void VCOM::listDevicesInformation(std::string path) {
        if (path[path.size() - 1] != '/')
            path += '/';
        for (int i = 0; i < TYPE_NUMBER; i++) {
            std::string s_path = path;
            s_path += DEVICES_TYPE[i];
            int fd = open(s_path.c_str(), O_RDONLY | O_NOCTTY);
            if (fd > 0) {
                char buf[256];
                memset(buf, '\0', sizeof(buf));
                if (read(fd, buf, sizeof(buf)) > 0) {
                    device_data_.device_information[i] = buf;
                }
            }
        }
    }

    /**
     * @brief 递归读取设备信息 寻找ttyACMn
     *
     * @param base_path 搜索根目录
     * @return 寻找到的设备数量
     */
    int VCOM::findConnectableDevice(const std::string base_path) {
        if (base_path == DEVICES_PATH) {
            std::cout << "正在搜索可用设备..." << std::endl;
            connectable_port_devices_.clear();
        }
        DIR *dir;
        struct dirent *ptr;
        if ((dir = opendir(base_path.c_str())) == nullptr) {
            perror("Open dir error...");
            printf("\ndir : %s\n", base_path.c_str());
            exit(1);
        }
        while ((ptr = readdir(dir)) != nullptr) {
            if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0) {
                continue;
            } else if (ptr->d_type == 4) {   //dir
                if (regex_match(ptr->d_name, std::regex("0000:[0-9]+:[0-9a-z]+\\.[0-9a-z]+")) || // 0000:xx:xx.xx
                    regex_match(ptr->d_name, std::regex("usb[0-9]+")) || // usbN
                    regex_match(ptr->d_name, std::regex("[0-9]+-[0-9]+:[0-9]+\\.[0-9]+")) || // n-n:n.n
                    regex_match(ptr->d_name, std::regex("[0-9]+-[0-9]+\\.[0-9]+:[0-9]+\\.[0-9]+")) || // n-n.n:n.n
                    regex_match(ptr->d_name, std::regex("tty"))) { // tty
                    findConnectableDevice(base_path + "/" + ptr->d_name);
                } else if (regex_match(ptr->d_name, std::regex("[0-9]+-[0-9]+")) || // n-n
                           regex_match(ptr->d_name, std::regex("[0-9]+-[0-9]+:[0-9]+-[0-9]+")) || // n-n:n-n
                           regex_match(ptr->d_name, std::regex("[0-9]+-[0-9]+\\.[0-9]+"))) { // n-n.n
                    listDevicesInformation(base_path + "/" + ptr->d_name);
                    findConnectableDevice(base_path + "/" + ptr->d_name);
                } else if (regex_match(ptr->d_name, std::regex("ttyACM[0-9]+"))) { // ttyACMn
                    device_data_.port = ptr->d_name;
                    std::cout << "path : " << base_path << "/" << device_data_.port << std::endl;
                    connectable_port_devices_.push_back(device_data_);
                }
            }
        }
        closedir(dir);
        if (base_path != DEVICES_PATH) return 0;
        std::cout << "搜索完成,检测到" << connectable_port_devices_.size() << "个可用设备" << std::endl;
        static int test_num = 3; // 尝试次数
        if (connectable_port_devices_.empty() && --test_num) {
            std::cout << "未找到可连接的串口设备" << std::endl;
            sleep(2);
            return this->findConnectableDevice();
        } else if (test_num < 1) {
            std::cout << "未找到可连接的串口设备" << std::endl;
            return -1;
        }
        for (long unsigned int i = 0; i < connectable_port_devices_.size(); i++) {
            std::cout << "第 " << i + 1 << " 个 : " << std::endl;
            std::cout << connectable_port_devices_[i] << std::endl;
        }
        if (connectable_port_devices_.size() == 1) {
            device_data_ = connectable_port_devices_[0];
        } else if (connectable_port_devices_.size() > 1) {
            long unsigned int select = 0;
            std::cout << "检测到多个设备" << std::endl;
            while (select < 1 || select > connectable_port_devices_.size()) {
                std::cout << "请输入您要连接第几个设备（输入数字），当前可用 : ";
                for (long unsigned int i = 1; i <= connectable_port_devices_.size(); i++) {
                    std::cout << i;
                    if (i != connectable_port_devices_.size())
                        std::cout << " , ";
                }
                std::cin >> select;
            }
            device_data_ = connectable_port_devices_[select - 1];
        }
        if (openPort() < 0) {
            std::cout << "当前串口打开失败\n请尝试在终端中执行以下命令\nsudo chmod 777 /dev/" << device_data_.port
                      << std::endl;
            close(uart_data_.fd);
            return -1;
        } else return setUartConfig();
    }

    /**
     * @brief 设置底层串口配置
     *
     * @return 是否修改成功
     *
     */
    int VCOM::setUartConfig() const {
        struct termios opt{};
        int speed;
        if (tcgetattr(uart_data_.fd, &opt) != 0) {
            std::cout << "warning : 串口设置读取失败" << std::endl;
        }
        /*设置波特率*/
        switch (uart_data_.baud_rate) {
            case 2400:
                speed = B2400;
                break;
            case 4800:
                speed = B4800;
                break;
            case 9600:
                speed = B9600;
                break;
            case 19200:
                speed = B19200;
                break;
            case 38400:
                speed = B38400;
                break;
            default:
                speed = B115200;
                break;
        }
        cfsetispeed(&opt, speed);
        cfsetospeed(&opt, speed);
        tcsetattr(uart_data_.fd, TCSANOW, &opt);
        opt.c_cflag &= ~CSIZE;
        /*设置数据位*/
        switch (uart_data_.data_bits) {
            case 7: {
                opt.c_cflag |= CS7;
            }
                break;//7个数据位
            default: {
                opt.c_cflag |= CS8;
            }
                break;//8个数据位
        }
        /*设置奇偶校验位*/
        switch (uart_data_.parity) { // N
            case 'n':
            case 'N': {
                opt.c_cflag &= ~PARENB;//校验位使能
                opt.c_iflag &= ~INPCK; //奇偶校验使能
            }
                break;
            case 'o':
            case 'O': {
                opt.c_cflag |= (PARODD | PARENB);//PARODD使用奇校验而不使用偶校验
                opt.c_iflag |= INPCK;
            }
                break;
            case 'e':
            case 'E': {
                opt.c_cflag |= PARENB;
                opt.c_cflag &= ~PARODD;
                opt.c_iflag |= INPCK;
            }
                break;
            case 's':
            case 'S': /*as no parity*/
            {
                opt.c_cflag &= ~PARENB;
                opt.c_cflag &= ~CSTOPB;
            }
                break;
            default: {
                opt.c_cflag &= ~PARENB;//校验位使能
                opt.c_iflag &= ~INPCK; //奇偶校验使能
            }
                break;
        }
        /*设置停止位*/
        switch (uart_data_.stop_bits) {
            case 1: {
                opt.c_cflag &= ~CSTOPB;
            }
                break;
            case 2: {
                opt.c_cflag |= CSTOPB;
            }
                break;
            default: {
                opt.c_cflag &= ~CSTOPB;
            }
                break;
        }

        /*处理未接收字符*/
        tcflush(uart_data_.fd, TCIFLUSH);

        /*关闭串口回显*/
        /*禁止翻译指令*/
        opt.c_lflag &= ~(ICANON | ISIG | ECHO | ECHOE | ECHOK | ECHONL | NOFLSH);

        /*禁止将输入中的回车翻译为新行*/
        /*禁止将所有接收的字符裁减为7比特*/
        opt.c_iflag &= ~(INLCR | ICRNL | IGNCR | IXON | IXOFF | ISTRIP | IXANY);

        opt.c_oflag &= ~(OPOST | ONLCR | OCRNL);

        /*设置等待时间和最小接收字符*/
        opt.c_cc[VTIME] = 100;
        opt.c_cc[VMIN] = 0;

        /*激活新配置*/
        if ((tcsetattr(uart_data_.fd, TCSANOW, &opt)) != 0) {
            std::cout << "warning : 串口配置失败, 将使用默认配置, 可能会导致部分数据出错" << std::endl;
            return -1;
        }
        return 1;
    }

    /**
     * @brief 根据串口名称打开串口并返回串口缓冲区文件描述符
     *
     * @param dev 串口名称
     * @param baud_rate 波特率
     * @param data_bits 数据位
     * @param parity 奇偶校验
     * @param stop_bits 停止位
     * @return 串口缓冲区文件描述符
     */
    int VCOM::openPort(const char *dev, int baud_rate, int data_bits, char parity, int stop_bits) {
        if (*dev == *"") {
            if (device_data_.port.empty()) {
                std::cout << "未配置串口信息" << std::endl;
                return -1;
            } else {
                std::string s = "/dev/" + device_data_.port;
                dev = s.c_str();
            }
        }
        uart_data_.baud_rate = baud_rate;
        uart_data_.data_bits = data_bits;
        uart_data_.parity = parity;
        uart_data_.stop_bits = stop_bits;
        int fd = open(dev, O_RDWR | O_NOCTTY);
        if (fd < 0) {
            perror("open serial port");
            return -1;
        }
        //恢复串口为阻塞状态
        //非阻塞：fcntl(fd,F_SETFL,FNDELAY)
        //阻塞：fcntl(fd,F_SETFL,0)
        if (fcntl(fd, F_SETFL, 0) < 0) {
            perror("fcntl F_SETFL\n");
            return -1;
        }
        /*测试是否为终端设备*/
        if (isatty(STDIN_FILENO) == 0) {
            perror("standard input is not a terminal device");
            return -1;
        }
        uart_data_.fd = fd;
        if (setUartConfig() < 0) {
            perror("set com config error");
            return -1;
        }
        std::cout << device_data_.port << " 串口已打开" << std::endl;
        return fd;
    }

    /**
     * @brief 发送数据
     *
     * @param data 发送的数据
     * @param func_code 功能码
     * @param id 识别号
     */
    void VCOM::transmit(std::vector<uint8_t> &data, uint8_t func_code, uint16_t id, bool is_open) {
        if (this->uart_data_.fd == -1) {
            std::cout << "未找到设备 (请调用findConnectableDevice方法以寻找设备)" << std::endl;
            return;
        }
        uint16_t len = data.size();
        uint8_t buff[BUFFER_SIZE];
        memset(buff, '\0', sizeof(buff));
        buff[0] = 0x5a;
        buff[1] = func_code;
        *((uint16_t *) (buff + 2)) = id;
        *((uint16_t *) (buff + 4)) = len;
        memcpy(buff + 6, data.data(), len);
        *((uint16_t *) (buff + 6 + len)) = (len == 0) ? 0 : CRC::Verify_CRC16_Check_Sum(data);
        uint8_t is_success = write(uart_data_.fd, buff, 8 + len);
        if (terminal_monitor_ || is_open) {
            std::cout << "--------------------\n";
            if (is_success > 0) {
                std::cout << "send : ";
                for (int i = 0; i < 8 + len; i++) {
                    std::cout << std::hex << (unsigned int) (unsigned char) buff[i] << " ";
                }
                std::cout << "\nfunc_code : " << std::hex << (unsigned int) (unsigned char) func_code;
                std::cout << "\nid : " << std::hex << (unsigned int) (unsigned char) id;
                std::cout << std::endl;
                tcflush(uart_data_.fd, TCIOFLUSH);
            } else {
                std::cout << "send error" << std::endl;
            }
            std::cout << "--------------------" << std::endl;
            std::cout.unsetf(std::ios::hex);
        }
    }

    /**
     * @brief 读取串口缓冲区,读取到数据添加到数据缓冲队列中
     *
     * @return 读取到的数据位数
     */
    int VCOM::portRead() {
        if (this->uart_data_.fd == -1) {
            std::cout << "未找到设备 (请调用findConnectableDevice方法以寻找设备)" << std::endl;
            return -1;
        }
        int bytesRead = read(uart_data_.fd, buffer_, sizeof(buffer_));
        if (bytesRead < 0) {
            std::cerr << "Error reading serial port." << std::endl;
            close(uart_data_.fd);
            return -1;
        }
        if (bytesRead == 0) return 0;
        {
            std::unique_lock<std::mutex> lock(read_queue_mutex_);
            read_queue_.emplace_back(std::vector<uint8_t>(buffer_, buffer_ + BUFFER_SIZE),
                                     std::chrono::system_clock::now());
        }
        return bytesRead;
    }

    /**
     * @brief 从数据缓冲队列读取数据并解包加验证，将解包的数据加入到解包数据处理队列
     *
     * @return 解包加验证后的数据位数
     */
    int VCOM::processData() {
        std::chrono::system_clock::time_point read_time;
        std::vector<uint8_t> read_data;
        {
            std::unique_lock<std::mutex> lock(read_queue_mutex_);
            if (read_queue_.empty()) return 0;
            read_data = read_queue_.front().first;
            read_time = read_queue_.front().second;
            read_queue_.pop_front();
        }
        int ptr = -1;
        while (++ptr < BUFFER_SIZE - 7) {
            uint16_t len = *(uint16_t *) (read_data.data() + 4 + ptr) + 6;
            if (*(read_data.data() + ptr) != 0x5a || len > BUFFER_SIZE - 7) continue;
            uint16_t received_CRC = *((uint16_t *) (read_data.data() + len + ptr));
            VCOMData vcom_data_buffer_;
            for (int i = 6; i < len; i++)
                vcom_data_buffer_.data.push_back(*(read_data.data() + i + ptr));
            uint16_t calculate_CRC = (len == 6) ? 0 : CRC::Verify_CRC16_Check_Sum(vcom_data_buffer_.data);
            if (received_CRC != calculate_CRC) continue;
            vcom_data_buffer_.func_code = *(read_data.data() + ptr + 1);
            vcom_data_buffer_.id = *(uint16_t *) (read_data.data() + 2 + ptr);
            vcom_data_buffer_.time = read_time;

            auto max_it_ = data_count_max_.find(std::make_pair(vcom_data_buffer_.func_code, vcom_data_buffer_.id));
            if (max_it_ != data_count_max_.end()) if (max_it_->second == 0) return 0;

            {
                std::unique_lock<std::mutex> lock(data_queue_mutex_);
                auto data_it_ = data_queue_.find(std::make_pair(vcom_data_buffer_.func_code, vcom_data_buffer_.id));
                if (data_it_ == data_queue_.end()) {
                    data_queue_.emplace(std::make_pair(vcom_data_buffer_.func_code, vcom_data_buffer_.id), std::deque<VCOMData> {vcom_data_buffer_});
                } else {
                    data_it_->second.push_back(vcom_data_buffer_);
                    if (max_it_ != data_count_max_.end() && max_it_->second > 0 && data_it_->second.size() > max_it_->second) {
                        data_it_->second.pop_front();
                    }
                }
            }

            if (terminal_monitor_ || getTerminalMap(vcom_data_buffer_.func_code, vcom_data_buffer_.id)) {
                std::cout << "--------------------\nread : ";
                for (int i = 0; i < len + 2; i++)
                    std::cout << std::hex << (unsigned int) (unsigned char) (read_data.data() + ptr)[i] << " ";
                std::cout << "\nCRC 验证成功";
                std::cout << "\nfunc_code : " << std::hex << (unsigned int) (unsigned char) vcom_data_buffer_.func_code;
                std::cout << "\nid : " << std::hex << (unsigned int) (unsigned char) vcom_data_buffer_.id;
                std::cout << "\ndata : ";
                if (vcom_data_buffer_.data.empty())
                    std::cout << "NULL";
                for (unsigned char i: vcom_data_buffer_.data)
                    std::cout << std::hex << (unsigned int) (unsigned char) i << " ";
                std::cout.unsetf(std::ios::hex);
                std::cout << "\n--------------------" << std::endl;
            }

            return static_cast<int>(vcom_data_buffer_.data.size());
        }
        return 0;
    }

    /**
     * @brief 从解包数据处理队列读取数据，并寻找对应回调函数并调用
     */
    void VCOM::tryStartCallBackFunc() {
        std::unique_lock<std::mutex> lock(data_queue_mutex_);
        if (data_queue_.empty()) return;
        for (auto &qdata: data_queue_) {
            auto it_ = callBackFuncMap_.find(std::make_pair(qdata.first.first, qdata.first.second));
            if (it_ == callBackFuncMap_.end()) continue; // 未找到注册的回调函数
            if (it_->first.first != qdata.first.first || it_->first.second != qdata.first.second) continue; // func_code id 不一致
            for (VCOMData data: qdata.second) {
                for (CallBackFuncInfo &func: it_->second) {
                    func.func(static_cast<void *>(data.data.data()));
                }
            }
            qdata.second.clear();
        }
    }

    /**
     * @brief 发送第一个待发送数据队列的数据
     */
    void VCOM::transmitQueueFrontData() {
        std::unique_lock<std::mutex> lock(transmit_queue_mutex_);
        if (transmit_queue_.empty()) return;
        transmit(transmit_queue_.back().first.data, transmit_queue_.back().first.func_code, transmit_queue_.back().first.id,
                 transmit_queue_.back().second);
        transmit_queue_.pop_back();
    }

    /**
     * @brief 根据func_code和id注册回调函数
     *
     * @param callBackFunc 回调函数指针 void(void*)
     * @param func_code 功能码 uint16_t
     * @param id 识别码 uint8_t
     */
    void VCOM::callBackFuncRegister(CallbackType callBackFunc, uint8_t func_code, uint16_t id) { // 寻找id与func_code一致的回调函数并调用
        auto it_ = callBackFuncMap_.find(std::make_pair(func_code, id));
        if (it_ != callBackFuncMap_.end()) {
            it_->second.emplace_back(CallBackFuncInfo{std::move(callBackFunc), func_code, id});
        } else {
            callBackFuncMap_.emplace(std::make_pair(func_code, id), std::vector<CallBackFuncInfo>{});
            callBackFuncRegister(callBackFunc, func_code, id);
        }
    }

    /**
     * @brief 设置该func_code与id对应的解包数据缓冲区最大数量。 0为不接收，负数表示无限制(默认)
     *
     * @param func_code 功能码
     * @param id 识别码
     * @param max_size 最大数量
     */
    void VCOM::setDataBufferSize(uint8_t func_code, uint16_t id, int max_size) {
        auto max_it = data_count_max_.find(std::make_pair(func_code, id));
        if (max_size >= 0) {
            if (max_it == data_count_max_.end()) {
                data_count_max_.emplace(std::make_pair(func_code, id), max_size);
            } else {
                max_it->second = max_size;
            }
        } else if (max_it != data_count_max_.end()) {
            data_count_max_.erase(max_it);
        }
    }

    /**
     * @brief 清空数据缓冲区
     */
    void VCOM::clearDataBuffer() {
        {
            std::unique_lock<std::mutex> lock(read_queue_mutex_);
            read_queue_.clear();
        }
        {
            std::unique_lock<std::mutex> lock(data_queue_mutex_);
            data_queue_.clear();
        }
    }


    /**
     * @brief 清空对应func_code和id的数据缓冲区
     *
     * @param func_code 功能码
     * @param id 识别码
     */
    void VCOM::clearDataBuffer(uint8_t func_code, uint16_t id) {
        auto data_it_ = data_queue_.find(std::make_pair(func_code, id));
        if (data_it_ == data_queue_.end()) return;
        data_it_->second.clear();
    }

    /**
     * @brief 开启串口
     *
     * @param hz 发送数据的最大频率
     */
    void VCOM::start(double hz) {

        is_running_ = true;

        read_thread_ = std::thread([this](){
            while (is_running_){
                portRead();
            }
        });

        process_thread_ = std::thread([this](){
            while (is_running_) {
                processData();
                tryStartCallBackFunc();
            }
        });

        transmit_thread_ = std::thread([this, hz](){
            while (is_running_) {

                auto start = std::chrono::steady_clock::now();

                transmitQueueFrontData();

                auto now = std::chrono::steady_clock::now();
                std::chrono::duration<double> elapsed = now - start;

                if (elapsed.count() < 1 / hz)
                    std::this_thread::sleep_for(std::chrono::duration<double>(1 / hz - elapsed.count()));
            }

        });
    }

    /**
     * @brief 停止串口
     */
    void VCOM::stop() {
        if (!is_running_) return;
        is_running_ = false;
        std::cout << "等待线程关闭(0 / 3)" << std::endl;
        while (!process_thread_.joinable());
        process_thread_.join();
        std::cout << "等待线程关闭(1 / 3)" << std::endl;
        while (!read_thread_.joinable());
        read_thread_.join();
        std::cout << "等待线程关闭(2 / 3)" << std::endl;
        while (!transmit_thread_.joinable());
        transmit_thread_.join();
        std::cout << "等待线程关闭(3 / 3)" << std::endl;
        close(uart_data_.fd);
    }

    /**
     * @brief 用于设置某一id与func_code是否开启终端监听
     *
     * @param func_code 功能码
     * @param id 识别码
     * @param is_open 是否开启
     */
    void VCOM::setTerminalMap(uint8_t func_code, uint16_t id, bool is_open) {
        auto it_ = terminal_map_.find(std::make_pair(func_code, id));
        if (it_ == terminal_map_.end()) {
            terminal_map_.emplace(std::make_pair(func_code, id), is_open);
        } else {
            it_->second = is_open;
        }
    }

    /**
     * @brief 获取某一id与func_code是否开启终端监听（未设置则返回为0）
     *
     * @param func_code 功能码
     * @param id 识别码
     * @return 是否开启
     */
    bool VCOM::getTerminalMap(uint8_t func_code, uint16_t id) {
        auto it_ = terminal_map_.find(std::make_pair(func_code, id));
        if (it_ == terminal_map_.end()) return false;
        return it_->second;
    }

    /**
     * @brief 设置传输优先级
     *
     * @param func_code 功能码
     * @param id 识别码
     * @param priority 优先级
     */
    void VCOM::setTransmitPriority(uint8_t func_code, uint16_t id, int priority) {
        auto it_ = transmit_priority_.find(std::make_pair(func_code, id));
        if (it_ == transmit_priority_.end()) {
            transmit_priority_.emplace(std::make_pair(func_code, id), priority);
        } else {
            it_->second = priority;
        }

    }

    /**
     * @brief 返回func_code与id对应的传输优先级
     *
     * @param func_code 功能码
     * @param id 识别码
     * @return 优先级
     */
    int VCOM::getTransmitPriority(uint8_t func_code, uint16_t id) {
        auto it_ = transmit_priority_.find(std::make_pair(func_code, id));
        if (it_ == transmit_priority_.end()) return 0;
        return it_->second;
    }
}
