#include "ADSB.h"

struct DataProviderImpl : ADSB::IDataProvider
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

    template <typename TFunc>
    DataProviderImpl(TFunc func, ADSB::Source source, std::string_view selectorName) :
        rtlsdr(selectorName), handler(func(trafficManager, &rtlsdr, source))
    {}

    ~DataProviderImpl() override = default;
    CLASS_DELETE_COPY_AND_MOVE(DataProviderImpl);

    void Start(ADSB::IListener& listener) override
    {
        trafficManager->SetListener(&listener);
        handler->Start(listener);
    }

    void Stop() override { handler->Stop(); }

    void NotifySelfLocation(ADSB::IAirCraft const& /*selfLoc*/) override {}

    std::shared_ptr<ADSB::TrafficManager> trafficManager = std::make_shared<ADSB::TrafficManager>();
    DeviceSerialNameSelector              rtlsdr;
    std::unique_ptr<ADSB::IDataProvider>  handler;
};

std::unique_ptr<ADSB::IDataProvider> ADSB::CreateADSB1090Provider()
{
    return std::make_unique<DataProviderImpl>(ADSB::TryCreateADSB1090Handler, ADSB::Source::ADSB1090, "1090");
}

std::unique_ptr<ADSB::IDataProvider> ADSB::CreateUAT978Provider()
{
    return std::make_unique<DataProviderImpl>(ADSB::TryCreateUAT978Handler, ADSB::Source::UAT978, "978");
}
