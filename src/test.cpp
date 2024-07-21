#include "VCOM.h"
#include <thread>

#define TESTLEN 10
struct data {
    int a;
    float b;
    int nums[TESTLEN];
};

data temp = {114, 5.14};

void readThread(VCOM::VCOM &COM) {
    while (COM.portRead() > 0) {
        if (COM.m_data.id == 1 && COM.m_data.func_code == 0) {
            data test;
            COM.getData<data>(test);
            std::cout << test.a << " " << test.b << std::endl;
            for (int i = 0; i < TESTLEN; i++) {
                std::cout << test.nums[i] << " ";
            }
            std::cout << std::endl;
        }
    }
}


int main()
{
    VCOM::VCOM COM;
    if (COM.findConnectableDeviceDir() < 0) return -1;

    int count = 0;

    std::thread read_thread(readThread, std::ref(COM));

    while (count < 3) {
        for (int i = count++; i < TESTLEN + count; i++) {
            temp.nums[i] = i;
        }
        COM.transmit<data>(temp);
        sleep(2);
    }

    if (read_thread.joinable()) {
        read_thread.join();
    }
    return 0;
}

