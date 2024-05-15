#include "VCOM.h"

int main()
{
    VCOMCOMM::VCOMCOMM COM;
    VCOMCOMM::VCOMData data = {1.14, 5.14};
    if (COM.FindConnectableDeviceDir() > 0) {
        std::cout << "send y : " << data.y_angle << std::endl;
        std::cout << "send p : " << data.p_angle << std::endl;
        COM.Transmit(data);
        if (COM.PortRead() > 0) {
            VCOMCOMM::VCOMData angle = COM.m_data;
            std::cout << "read y : " << angle.y_angle << std::endl;
            std::cout << "read p : " << angle.p_angle << std::endl;
        } 
    }

    return 0;
}
