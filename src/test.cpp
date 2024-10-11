#include "VCOM.h"

struct test_data{
    int a[12];
};

int count = 0;

void callBackFunc(void* data) {
    test_data m = *(test_data*)data;
    ++count;
}

int main()
{
    VCOM::VCOM com;

    VCOM::VCOM::CallbackType a;

    if (com.findConnectableDevice() < 0) return -1;

    com.terminal_monitor_ = true;

    com.callBackFuncRegister(callBackFunc, 1, 0);

    com.setDataBufferSize(1, 1, 35);

    com.setTransmitPriority(1, 1, -1);

    com.start(1); // 设置发送最大频率

    std::vector<test_data> data00, data11, data01;


    for (int i = 0; i < 5; i++) {
        com.transmit<test_data>({i}, 1, 1);
        com.transmit<test_data>({i}, 1, 0);
        com.transmit<test_data>({i}, 0, 0);
        com.transmit<test_data>({i}, 0, 1);
    }

    std::cout << "开始等待 0 1 新数据" << std::endl;
    while (!com.getNewData<void*>(nullptr, 0, 1, 2, true));
    std::cout << "收到 0 1 新数据" << std::endl;

    sleep(10); // 在收发数据时 请保证 VCOM 对象的存在

    com.getData(data00, 0, 0);
    com.getData(data11, 1, 1, true);
    std::cout << "1 0共调用了 " << count << " 次回调函数" << std::endl; // 如果注册了回调函数将收不到回调函数处理过的数据
    std::cout << "0 0共接收了 " << data00.size() << " 次串口数据" << std::endl;
    std::cout << "1 1共接收了 " << data11.size() << " 次串口数据" << std::endl;

    return 0;
}
