#include "ADSB1090.h"
#include "AircraftImpl.h"
#include "RTLSDR.h"
#include "TestUtils.h"
#include <memory>

DECLARE_RESOURCE_COLLECTION(traces);

TEST_CASE("Test", "[1090]")
{
    for (auto const res : LOAD_RESOURCE_COLLECTION(traces))
    {
        // auto fname = std::filesystem::path(res.name());
        //  REQUIRE(!fname.empty());
        struct Selector : RTLSDR::IDeviceSelector
        {
            [[nodiscard]] bool SelectDevice(RTLSDR::DeviceInfo const& /* d */) const override { return false; }
        };

        struct Listener : ADSB::IListener
        {
            void OnChanged(ADSB::IAirCraft const& a) override { fmt::print("{}:{}:{}\n", a.Addr(), a.Lat1E7(), a.Lon1E7()); }
        };

        Listener listener;
        auto     mgr = std::make_shared<ADSB::TrafficManager>();
        mgr->SetListener(&listener);
        Selector selector;
        auto     handler = ADSB::test::TryCreateADSB1090Handler(mgr, &selector, 1);
        handler->HandleData(res.data<uint8_t>());
    }
    // REQUIRE(!pidlfiles.empty());
    // REQUIRE_NOTHROW(RunTest(pidlfiles));
}