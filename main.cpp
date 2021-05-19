#include "ADSBListener.h"

#include <iostream>

int main()
{
    ADSB::Listener listener;
    listener.OnMessage([&](auto& message){
       // std::cout << message << std::endl;
    });
    listener.Start();

}