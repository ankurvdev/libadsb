#include "ADSBListener.h"
#include "ADSB1090.h"
#include "AircraftImpl.h"
#include "RTLSDR.h"
#include "UAT978.h"

struct ADSBDataProviderImpl : ADSB::IDataProvider
{
    struct DeviceSerialNameSelector : RTLSDR::IDeviceSelector
    {
        DeviceSerialNameSelector(std::string_view const& serialName) : _serialName(serialName) {}

        virtual bool SelectDevice(RTLSDR::DeviceInfo const& d) const override
        {
            return std::string_view(d.serial).find(_serialName) != std::string_view::npos;
        }

        std::string_view _serialName{};
    };

    ADSBDataProviderImpl()
    {
        _handler978  = UAT978Handler::TryCreate(_trafficManager, &_rtlsdr978);
        _handler1090 = ADSB1090Handler::TryCreate(_trafficManager, &_rtlsdr1090);
    }

    CLASS_DELETE_COPY_AND_MOVE(ADSBDataProviderImpl);

    virtual void Start(ADSB::IListener& listener) override
    {
        _trafficManager->SetListener(&listener);
        if (_handler978) _handler978->Start(listener);
        if (_handler1090) _handler1090->Start(listener);
    }

    virtual void Stop() override
    {
        if (_handler978) _handler978->Stop();
        if (_handler1090) _handler1090->Stop();
    }

    virtual void NotifySelfLocation(ADSB::IAirCraft const& /*selfLoc*/) override {}

    std::shared_ptr<TrafficManager>  _trafficManager = std::make_shared<TrafficManager>();
    std::unique_ptr<UAT978Handler>   _handler978;
    std::unique_ptr<ADSB1090Handler> _handler1090;
    DeviceSerialNameSelector         _rtlsdr978{"978"};
    DeviceSerialNameSelector         _rtlsdr1090{"1090"};
};

std::unique_ptr<ADSB::IDataProvider> ADSB::CreateDump1090Provider()
{
    return std::make_unique<ADSBDataProviderImpl>();
}
