#include "ADSBListener.h"
#include "CommonMacros.h"

#include <fstream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <thread>

#include "ADSB1090.h"
#include "AircraftImpl.h"
#include "RTLSDR.h"
#include "UAT978.h"

// TODO : Thread safety
static ADSB::IListener* singletonListener = nullptr;

struct ModeMessageImpl : ADSB::IModeMessage
{
    ModeMessageImpl(struct modesMessage* /*mm*/) {}
};
#if 0
struct MessageHandler1090
{
    MessageHandler1090(uint32_t index978)
    {
        if (initlistener(index978) != 0)
        {
            throw std::runtime_error("Cannot initialize device");
        }
    }
    CLASS_DELETE_COPY_AND_MOVE(MessageHandler1090);

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
#endif

struct ADSBDataProviderImpl : ADSB::IDataProvider
{
    CLASS_DELETE_COPY_AND_MOVE(ADSBDataProviderImpl);
    ADSBDataProviderImpl()
    {
        _handler978  = UAT978Handler::TryCreate();
        _handler1090 = ADSB1090Handler::TryCreate();
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

    std::unique_ptr<UAT978Handler>   _handler978;
    std::unique_ptr<ADSB1090Handler> _handler1090;
};
#if 0
extern "C" void modesQueueOutput(struct modesMessage* mm)
{
    auto a = interactiveFindAircraft(mm->addr);
    if (a)
    {
        singletonListener->OnMessage(ModeMessageImpl(mm), AirCraftImpl(a));
    }
}

extern "C" void modesSendAllClients(int /*service*/, void* /*msg*/, int /*len*/)
{
}
#endif
std::unique_ptr<ADSB::IDataProvider> ADSB::CreateDump1090Provider(std::string_view const& /*deviceName*/)
{
    return std::make_unique<ADSBDataProviderImpl>();
}
