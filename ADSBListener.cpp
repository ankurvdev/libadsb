#include "ADSBListener.h"

#include <fstream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <thread>

extern "C"
{
#include "dump1090.h"
}
#include "RTLSDR.h"
#include "UAT978.h"
// TODO : Thread safety
static ADSB::IListener* singletonListener = nullptr;

extern "C" int                        initlistener(uint32_t index);
extern "C" void                       startlistener();
extern "C" void                       stoplistener();
std::unique_ptr<RTLSDR::IDataHandler> CreateMessageHandler978(size_t index);

struct ModeMessageImpl : ADSB::IModeMessage
{
    ModeMessageImpl(struct modesMessage* mm) {}
};

struct AirCraftImpl : ADSB::IAirCraft
{
    AirCraftImpl(struct aircraft* a) : _a(a) {}

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

struct MessageHandler1090
{
    MessageHandler1090(uint32_t index978)
    {
        if (initlistener(index978) != 0)
        {
            throw std::runtime_error("Cannot initialize device");
        }
    }
    void Start(ADSB::IListener& listener)
    {
        if (singletonListener != nullptr)
        {
            std::logic_error("Unfortunately we can only have a single listener in a process");
        }

        singletonListener = &listener;

        _thrd = std::thread([&]() { startlistener(); });
    }

    void Stop()
    {
        stoplistener();
        _thrd.join();
        singletonListener = nullptr;
    }

    std::thread _thrd;
};

struct ADSBDataProviderImpl : ADSB::IDataProvider
{
    ADSBDataProviderImpl(uint32_t index978, uint32_t index1090)
    {
        if (index978 != std::numeric_limits<uint32_t>::max())
        {
            try
            {
                _handler978.reset(new MessageHandler978(index978));
            }
            catch (std::exception const& ex)
            {
                std::cerr << "Cannot open device for 978Mhz" << std::endl;
            }
        }
        if (index1090 != std::numeric_limits<uint32_t>::max())
        {
            try
            {
                _handler1090.reset(new MessageHandler1090(index1090));
            }
            catch (std::exception const& ex)
            {
                std::cerr << "Cannot open device for 1090Mhz" << std::endl;
            }
        }
        if (_handler1090 == nullptr && _handler978 == nullptr)
        {
            throw std::runtime_error("Cannot find USB Device for 1090Mhz or 978Mhz");
        }
    }

    virtual void Start(ADSB::IListener& listener) override
    {
        if (_handler978) _handler978->Start(listener);
        if (_handler1090) _handler1090->Start(listener);
    }

    virtual void Stop() override
    {
        if (_handler978) _handler978->Stop();
        if (_handler1090) _handler1090->Stop();
    }

    std::unique_ptr<MessageHandler978>  _handler978;
    std::unique_ptr<MessageHandler1090> _handler1090;
};

extern "C" void modesQueueOutput(struct modesMessage* mm)
{
    auto a = interactiveFindAircraft(mm->addr);
    if (a)
    {
        singletonListener->OnMessage(ModeMessageImpl(mm), AirCraftImpl(a));
    }
}

extern "C" void modesSendAllClients(int service, void* msg, int len)
{
}

std::unique_ptr<ADSB::IDataProvider> ADSB::CreateDump1090Provider(std::string_view const& deviceName)
{
    auto devices   = RTLSDR::GetAllDevices();
    auto index978  = std::numeric_limits<uint32_t>::max();
    auto index1090 = std::numeric_limits<uint32_t>::max();
    if (std::filesystem::exists("978000000.test.dat"))
    {
        index978 = 0;
    }
    if (std::filesystem::exists("1090000000.test.dat"))
    {
        index1090 = 0;
    }
    for (auto& d : devices)
    {
        if (std::string_view(d.serial).find("978") != std::string_view::npos)
        {
            index978 = d.index;
        }
        else if (std::string_view(d.serial).find("1090") != std::string_view::npos)
        {
            index1090 = d.index;
        }
    }
    return std::make_unique<ADSBDataProviderImpl>(index978, index1090);
}
