#include "ADSBListener.h"
#include "ADSB1090.h"
#include "AircraftImpl.h"
#include "RTLSDR.h"
#include "UAT978.h"

struct ADSBDataProviderImpl : ADSB::IDataProvider
{
    struct DeviceSerialNameSelector : RTLSDR::IDeviceSelector
    {
        DeviceSerialNameSelector(std::string_view const& serialNameIn) : serialName(serialNameIn) {}

        [[nodiscard]] bool SelectDevice(RTLSDR::DeviceInfo const& d) const override
        {
            return std::string_view(std::begin(d.serial), std::end(d.serial)).find(serialName) != std::string_view::npos;
        }

        std::string_view serialName;
    };

    ADSBDataProviderImpl()
    {
        handler978  = UAT978Handler::TryCreate(trafficManager, &rtlsdr978, 1);
        handler1090 = ADSB1090Handler::TryCreate(trafficManager, &rtlsdr1090, 2);
    }
    ~ADSBDataProviderImpl() override = default;
    CLASS_DELETE_COPY_AND_MOVE(ADSBDataProviderImpl);

    void Start(ADSB::IListener& listener) override
    {
        trafficManager->SetListener(&listener);
        if (handler978) { handler978->Start(listener); }
        if (handler1090) { handler1090->Start(listener); }
    }

    void Stop() override
    {
        if (handler978) { handler978->Stop(); }
        if (handler1090) { handler1090->Stop(); }
    }

    void NotifySelfLocation(ADSB::IAirCraft const& /*selfLoc*/) override {}

    std::shared_ptr<TrafficManager>  trafficManager = std::make_shared<TrafficManager>();
    std::unique_ptr<UAT978Handler>   handler978;
    std::unique_ptr<ADSB1090Handler> handler1090;
    DeviceSerialNameSelector         rtlsdr978{"978"};
    DeviceSerialNameSelector         rtlsdr1090{"1090"};
};

std::unique_ptr<ADSB::IDataProvider> ADSB::CreateDump1090Provider()
{
    return std::make_unique<ADSBDataProviderImpl>();
}
