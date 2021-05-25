#include "ADSBListener.h"

#include <stdexcept>
#include <thread>

extern "C"
{
#include "dump1090.h"
}

// TODO : Thread safety
static ADSB::IListener* singletonListener = nullptr;

extern "C" void startlistener(const char*);
extern "C" void stoplistener();

struct ModeMessageImpl : ADSB::IModeMessage
{
    ModeMessageImpl(struct modesMessage* mm) {}
};

struct AirCraftImpl : ADSB::IAirCraft
{
    AirCraftImpl(struct aircraft* a) {}

    virtual uint32_t         MessageCount() const override { return 0; }
    virtual uint32_t         Addr() const override { return _a->addr; }
    virtual std::string_view FlightNumber() const override { return _a->flight; }
    virtual time_point       LastSeen() const override { return time_point::clock::from_time_t(_a->seen); }
    virtual uint32_t         SquakCode() const override { return _a->modeA; }
    virtual int32_t          Altitude() const override { return _a->altitude; }
    virtual uint32_t         Speed() const override { return _a->speed; }
    virtual uint32_t         Heading() const override { return _a->track; }
    virtual int32_t          Climb() const override { return _a->vert_rate; }
    virtual int32_t          Lat1E7() const override { return static_cast<int32_t>(_a->lat * 10000000.0); }
    virtual int32_t          Lon1E7() const override { return static_cast<int32_t>(_a->lon * 10000000.0); }

    struct aircraft* _a;
};

struct Dum1090DataProvider : ADSB::IDataProvider
{
    Dum1090DataProvider(std::string_view const& deviceName) : _deviceName(deviceName) {}

    virtual void Start(ADSB::IListener& listener) override
    {
        if (singletonListener != nullptr)
        {
            std::logic_error("Unfortunately we can only have a single listener in a process");
        }

        singletonListener = &listener;

        _thrd = std::thread([&]() { startlistener(_deviceName.c_str()); });
    }

    virtual void Stop() override
    {
        stoplistener();
        _thrd.join();
        singletonListener = nullptr;
    }

    std::thread _thrd;
    std::string _deviceName;
};

extern "C" void modesQueueOutput(struct modesMessage* mm)
{
    singletonListener->OnMessage(ModeMessageImpl(mm), AirCraftImpl(interactiveFindAircraft(mm->addr)));
}

extern "C" void modesSendAllClients(int service, void* msg, int len)
{
}

std::unique_ptr<ADSB::IDataProvider> ADSB::CreateDump1090Provider(std::string_view const& deviceName)
{
    return std::make_unique<Dum1090DataProvider>(deviceName);
}
