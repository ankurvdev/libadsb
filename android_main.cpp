#include "ADSBListener.h"
#include "CommonMacros.h"
#include "testadsb_app_export.h"

#include <iostream>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#include <libusb.h>
#pragma clang diagnostic pop

#ifdef __ANDROID__
#include <jni.h>
#endif

extern "C" void TestadsbAppExport   app_start(size_t count, ...);
extern "C" void TESTADSB_APP_EXPORT app_stop(size_t count, ...);
get_webserver_url();
extern "C" void TestadsbAppExport android_update_location(JNIEnv* jniEnv, jobject thiz, jobject location);
extern "C" void TestadsbAppExport android_update_orientation(JNIEnv* jniEnv, jobject thiz, jint location);

extern "C" void AndroidUpdateLocation(JNIEnv* /* jniEnv */, jobject /* thiz */, jobject /* location */)
{}
extern "C" void AndroidUpdateOrientation(JNIEnv* /* jniEnv */, jobject /* thiz */, jint /* location */)
{}

struct ADSBTrackerImpl : ADSB::IListener
{
    CLASS_DELETE_COPY_AND_MOVE(ADSBTrackerImpl);

    ADSBTrackerImpl()
    {
        adsb1090->Start(*this);
        uat978->Start(*this);
    }

    ~ADSBTrackerImpl() override
    {
        adsb1090->Stop();
        uat978->Stop();
    }

    void OnChanged(ADSB::IAirCraft const& a) override
    {
        std::cout << a.FlightNumber() << ":" << std::hex << a.Addr() << ":" << std::dec << " Speed:" << a.Speed() << " Alt:" << a.Altitude()
                  << " Heading:" << a.Heading() << " Climb:" << a.Climb() << " Lat:" << a.Lat1E7() << " Lon:" << a.Lon1E7() << std::endl;
    }

    std::unordered_map<uint32_t, std::chrono::system_clock::time_point> icaoTimestamps{};
    std::unordered_map<uint32_t, size_t>                                aircrafts{};
    std::unique_ptr<ADSB::IDataProvider>                                adsb1090 = ADSB::CreateADSB1090Provider();
    std::unique_ptr<ADSB::IDataProvider>                                uat978   = ADSB::CreateUAT978Provider();

    std::vector<uint8_t> data{};

    // DataRecorder<Avid::Aircraft> _recorder;
    std::mutex mutex;
};

static ADSBTrackerImpl*           Ptr = nullptr;
extern "C" void TestadsbAppExport app_start(size_t count, ...)
{
#ifdef __ANDROID__
    va_list args;
    va_start(args, count);
    JavaVM* vm;
    auto    env = va_arg(args, JNIEnv*);    // first int
    env->GetJavaVM(&vm);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

    libusb_set_option(0, LIBUSB_OPTION_ANDROID_JAVAVM, vm, 0);
    libusb_set_option(0, LIBUSB_OPTION_ANDROID_JNIENV, env, 0);
#pragma clang diagnostic pop

    va_end(args);

#endif
    ptr = new ADSBTrackerImpl();
}

extern "C" void TESTADSB_APP_EXPORT app_stop(size_t /*count*/, ...)
{
    delete ptr;
}

get_webserver_url()
{
    return "http://localhost:41082/index.html";
}
