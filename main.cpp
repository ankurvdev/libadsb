#include "ADSBListener.h"
#include "CommonMacros.h"

#include <iostream>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace std::chrono_literals;

struct ADSBTrackerImpl : ADSB::IListener
{
    CLASS_DELETE_COPY_AND_MOVE(ADSBTrackerImpl);

    ADSBTrackerImpl() : _dump1090Provider(ADSB::CreateDump1090Provider())
    {
        std::cout << "ADSB Tracker Initializing" << std::endl;
        _dump1090Provider->Start(*this);
    }

    ~ADSBTrackerImpl() override { _dump1090Provider->Stop(); }

    void OnChanged(ADSB::IAirCraft const& a) override
    {
        std::cout << a.FlightNumber() << ":" << std::hex << a.Addr() << ":" << std::dec << " Speed:" << a.Speed() << " Alt:" << a.Altitude()
                  << " Heading:" << a.Heading() << " Climb:" << a.Climb() << " Lat:" << a.Lat1E7() << " Lon:" << a.Lon1E7() << std::endl;
    }

    std::unordered_map<uint32_t, std::chrono::system_clock::time_point> _icaoTimestamps;
    std::unordered_map<uint32_t, size_t>                                _aircrafts;
    std::unique_ptr<ADSB::IDataProvider>                                _dump1090Provider;

    std::vector<uint8_t> _data;

    // DataRecorder<Avid::Aircraft> _recorder;
    std::mutex _mutex;
};

int main()
{
    ADSBTrackerImpl tracker;
    while (true) std::this_thread::sleep_for(10s);
    return 0;
}
