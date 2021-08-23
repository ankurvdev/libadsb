#include "ADSBListener.h"
#include "CommonMacros.h"
#include "testadsb_app_export.h"

#include <iostream>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <libusb.h>

#if defined __ANDROID__
#include <jni.h>
#endif

extern "C" void TESTADSB_APP_EXPORT        app_start(size_t count, ...);
extern "C" void TESTADSB_APP_EXPORT        app_stop(size_t count, ...);
extern "C" const char* TESTADSB_APP_EXPORT get_webserver_url();

struct ADSBTrackerImpl : ADSB::IListener
{
    CLASS_DELETE_COPY_AND_MOVE(ADSBTrackerImpl);

    ADSBTrackerImpl(std::string_view const& deviceName) : _dump1090Provider(ADSB::CreateDump1090Provider(deviceName))
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

    // DataRecorder<Avid::Aircraft::Data> _recorder;
    std::mutex _mutex;
};

ADSBTrackerImpl*                 ptr;
extern "C" void TESTADSB_APP_EXPORT app_start(size_t count, ...)
{
#if defined __ANDROID__
    va_list args;
    va_start(args, count);
    JavaVM *vm;
    auto env = va_arg(args, JNIEnv*);    // first int
    env->GetJavaVM(&vm);
    libusb_set_option(0, LIBUSB_OPTION_ANDROID_JAVAVM, vm, 0);
    libusb_set_option(0, LIBUSB_OPTION_ANDROID_JNIENV, env, 0);
    va_end(args);
#endif
    ptr = new ADSBTrackerImpl("");
}

extern "C" void TESTADSB_APP_EXPORT app_stop(size_t /*count*/, ...)
{
    delete ptr;
}

extern "C" const char* TESTADSB_APP_EXPORT get_webserver_url()
{
    return "http://localhost:41082/index.html";
}