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

    com.terminal_monitor_ = false;

    com.callBackFuncRegister(callBackFunc, 1, 0);

    com.start(30); // 设置发送最大频率

    for (int i = 0; i < 50; i++)
        com.transmit<test_data>({i}, 1, 0);

    sleep(5); // 在收发数据时 请保证 VCOM 对象的存在

    std::vector<test_data> datas;
    com.getData(datas, 1, 0);
    std::cout << "共调用了 " << count << " 次回调函数" << std::endl; // 如果注册了回调函数将收不到回调函数处理过的数据
    std::cout << "共接收了 " << datas.size() << " 次串口数据" << std::endl;

    return 0;
}
