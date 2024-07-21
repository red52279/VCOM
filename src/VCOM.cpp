#include "VCOM.h"

namespace VCOM {

std::ostream& operator<<(std::ostream& cout, device_data &data) {
    for (int i = 0; i < 6; i++)
        cout << DEVICES_TYPE[i] << " : " << data.device_information[i];
    cout << "port : " << data.port;
    return cout;
}

device_data VCOM::listDevicesInformation(std::string path) { // 读取并返回设备信息
    device_data data;
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
                data.device_information[i] = buf;
            }
        }
    }
    return data;
}

int VCOM::findConnectableDeviceDir(const std::string base_path, device_data data) { // 递归读取设备信息 寻找ttyACMn
    if (base_path == DEVICES_PATH) {
        std::cout << "正在搜索可用设备..." << std::endl;
        m_connectable_port_devices.clear();
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
                findConnectableDeviceDir(base_path + "/" + ptr->d_name, data);
            } else if (regex_match(ptr->d_name, std::regex("[0-9]+-[0-9]+")) || // n-n
                regex_match(ptr->d_name, std::regex("[0-9]+-[0-9]+:[0-9]+-[0-9]+")) || // n-n:n-n
                regex_match(ptr->d_name, std::regex("[0-9]+-[0-9]+\\.[0-9]+"))) { // n-n.n
                data = listDevicesInformation(base_path + "/" + ptr->d_name);
                findConnectableDeviceDir(base_path + "/" + ptr->d_name, data);
            } else if (regex_match(ptr->d_name, std::regex("ttyACM[0-9]+"))) { // ttyACMn
                data.port = ptr->d_name;
                std::cout << "path : " << base_path << "/" << data.port << std::endl;
                m_connectable_port_devices.push_back(data);
            }
        }
    }
    closedir(dir);
    if (base_path != DEVICES_PATH) return 0;
    std::cout << "搜索完成,检测到" << m_connectable_port_devices.size() << "个可用设备" << std::endl;
    if (m_connectable_port_devices.empty()) {
        std::cout << "未找到可连接的串口设备"  << std::endl;
        sleep(2);
        return this->findConnectableDeviceDir();
    }
    for (long unsigned int i = 0; i < m_connectable_port_devices.size(); i++) {
        std::cout << "第 " << i + 1 << " 个 : " << std::endl;
        std::cout << m_connectable_port_devices[i] << std::endl;
    }
    if (m_connectable_port_devices.size() == 1) {
        m_device_data = m_connectable_port_devices[0];
    } else {
        long unsigned int select = 0;
        std::cout << "检测到多个设备" << std::endl;
        while (select < 1 || select > m_connectable_port_devices.size()) {
            std::cout << "请输入您要连接第几个设备（输入数字），当前可用 : ";
            for (long unsigned int i = 1; i <= m_connectable_port_devices.size(); i++) {
                std::cout << i;
                if (i != m_connectable_port_devices.size())
                    std::cout << " , ";
            }
            std::cin >> select;
        }
        m_device_data = m_connectable_port_devices[select - 1];
    }
    if (openPort() < 0) {
        std::cout << "当前串口打开失败\n请尝试在终端中执行以下命令\nsudo chmod 777 /dev/" << m_device_data.port << std::endl;
        close(m_uart_data.fd);
        return -1;
    } else return setUartConfig();
}

int VCOM::setUartConfig() const {
    struct termios opt{};
    int speed;
    if (tcgetattr(m_uart_data.fd, &opt) != 0) {
        std::cout << "warning : 串口设置读取失败" << std::endl;
    }
    /*设置波特率*/
    switch (m_uart_data.baud_rate) {
        case 2400:  speed = B2400;  break;
        case 4800:  speed = B4800;  break;
        case 9600:  speed = B9600;  break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        default:    speed = B115200; break;
    }
    cfsetispeed(&opt, speed);
    cfsetospeed(&opt, speed);
    tcsetattr(m_uart_data.fd, TCSANOW, &opt);
    opt.c_cflag &= ~CSIZE;
    /*设置数据位*/
    switch (m_uart_data.data_bits) {
        case 7: {opt.c_cflag |= CS7; }break;//7个数据位
        default: {opt.c_cflag |= CS8; }break;//8个数据位
    }
    /*设置奇偶校验位*/
    switch (m_uart_data.parity) { // N
        case 'n':case 'N':
        {
            opt.c_cflag &= ~PARENB;//校验位使能
            opt.c_iflag &= ~INPCK; //奇偶校验使能
        }break;
        case 'o':case 'O':
        {
            opt.c_cflag |= (PARODD | PARENB);//PARODD使用奇校验而不使用偶校验
            opt.c_iflag |= INPCK;
        }break;
        case 'e':case 'E':
        {
            opt.c_cflag |= PARENB;
            opt.c_cflag &= ~PARODD;
            opt.c_iflag |= INPCK;
        }break;
        case 's':case 'S': /*as no parity*/
        {
            opt.c_cflag &= ~PARENB;
            opt.c_cflag &= ~CSTOPB;
        }break;
        default:
        {
            opt.c_cflag &= ~PARENB;//校验位使能
            opt.c_iflag &= ~INPCK; //奇偶校验使能
        }break;
    }
    /*设置停止位*/
    switch (m_uart_data.stop_bits) {
        case 1: {opt.c_cflag &= ~CSTOPB; } break;
        case 2: {opt.c_cflag |= CSTOPB; }   break;
        default: {opt.c_cflag &= ~CSTOPB; } break;
    }

    /*处理未接收字符*/
    tcflush(m_uart_data.fd, TCIFLUSH);

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
    if ((tcsetattr(m_uart_data.fd, TCSANOW, &opt)) != 0) {
        std::cout << "warning : 串口配置失败, 将使用默认配置, 可能会导致部分数据出错" << std::endl;
        return -1;
    }
    return 1;
}

int VCOM::openPort(const char* dev, int baud_rate, int data_bits, char parity, int stop_bits) {
    if (*dev == *"") {
        if (m_device_data.port.empty()) {
            std::cout << "未配置串口信息" << std::endl;
            return -1;
        } else {
            std::string s = "/dev/" + m_device_data.port;
            dev = s.c_str();
        }
    }
    m_uart_data.baud_rate = baud_rate;
    m_uart_data.data_bits = data_bits;
    m_uart_data.parity = parity;
    m_uart_data.stop_bits = stop_bits;
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
    m_uart_data.fd = fd;
    if (setUartConfig() < 0) {
        perror("set com config error");
        return -1;
    }
    std::cout << m_device_data.port << " 串口已打开" << std::endl;
    return fd;
}



void VCOM::transmit(std::vector<uint8_t> &data, uint8_t func_code, uint16_t id) {
    uint16_t len = data.size();
    uint8_t buff[BUFFER_SIZE];
    memset(buff, '\0', sizeof(buff));
    buff[0] = 0x5a;
    buff[1] = func_code;
    *((uint16_t *) (buff + 2)) = id;
    *((uint16_t *) (buff + 4)) = len;
    memcpy(buff + 6, data.data(), len);
    *((uint16_t *) (buff + 6 + len)) = (len == 0) ? 0 : CRC::Verify_CRC16_Check_Sum(data);
    std::cout << "--------------------" << std::endl;
    if (write(m_uart_data.fd, buff, 8 + len) > 0) {
        std::cout << "send : ";
        for (int i = 0; i < 8 + len; i++) {
            std::cout << std::hex << (unsigned int) (unsigned char)buff[i] << " ";
        }
        std::cout << std::endl;
        std::cout << "--------------------" << std::endl;
        tcflush(m_uart_data.fd, TCIOFLUSH);
    } else {
        std::cout << "send error" << std::endl;
        std::cout << "--------------------" << std::endl;
    }
    std::cout.unsetf(std::ios::hex);
}

int VCOM::portRead() {
    int bytesRead = read(m_uart_data.fd, m_buffer, sizeof(m_buffer));
    if (bytesRead < 0) {
        std::cerr << "Error reading serial port." << std::endl;
        close(m_uart_data.fd);
        return -1;
    }
    if (bytesRead == 0) return 0;
    int ptr = -1;
    while (++ptr < BUFFER_SIZE - 7) {
        uint16_t len = *(uint16_t*)(m_buffer + 4 + ptr) + 6;
        if (*(m_buffer + ptr) != 0x5a || len > BUFFER_SIZE - 7) continue;
        uint16_t received_CRC = *((uint16_t *) (m_buffer + len + ptr));
        std::vector<uint8_t> data;
        for (int i = 6; i < len; i++)
            data.push_back(*(m_buffer + i + ptr));
        uint16_t calculate_CRC = (len == 6) ? 0 : CRC::Verify_CRC16_Check_Sum(data);
        if (received_CRC != calculate_CRC) continue;
        std::cout << "--------------------\nread : ";
        for (int i = 0; i < len + 2; i++)
            std::cout << std::hex << (unsigned int)(unsigned char)(m_buffer + ptr)[i] << " ";
        std::cout << "\nCRC 验证成功\ndata : ";
        if (data.empty())
            std::cout << "NULL";
        for (unsigned char i : data)
            std::cout << std::hex << (unsigned int)(unsigned char)i << " ";
        std::cout.unsetf(std::ios::hex);
        std::cout << "\n--------------------" << std::endl;
        m_data.func_code = *(m_buffer + ptr + 1);
        m_data.id = *(uint16_t*)(m_buffer + 2 + ptr);
        m_data.len = *(uint16_t*)(m_buffer + 4 + ptr);
        m_data.data = data;
        return data.size();
    }
}
}
