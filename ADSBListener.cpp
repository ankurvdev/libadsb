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

struct ADSBDataProviderImpl : ADSB::IDataProvider
{
    CLASS_DELETE_COPY_AND_MOVE(ADSBDataProviderImpl);
    ADSBDataProviderImpl()
    {
        _handler978  = UAT978Handler::TryCreate(_trafficManager);
        _handler1090 = ADSB1090Handler::TryCreate(_trafficManager);
    }

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

    std::shared_ptr<TrafficManager>  _trafficManager = std::make_shared<TrafficManager>();
    std::unique_ptr<UAT978Handler>   _handler978;
    std::unique_ptr<ADSB1090Handler> _handler1090;
};

std::unique_ptr<ADSB::IDataProvider> ADSB::CreateDump1090Provider()
{
    return std::make_unique<ADSBDataProviderImpl>();
}
