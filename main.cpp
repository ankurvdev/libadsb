#include "ADSBListener.h"
#include "CommonMacros.h"

#include <iostream>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace std::chrono_literals;
struct AvidTrafficData
{
};

struct ADSBTrackerImpl : ADSB::IListener
{
    struct Config
    {
        bool fixErrors    = 1;
        bool checkCRC     = 1;
        bool raw          = 0;
        bool onlyaddr     = 0;
        bool debug        = 0;
        bool ienteractive = 0;
        bool aggressive   = 0;
        bool loop         = 0;
    };

    CLASS_DELETE_COPY_AND_MOVE(ADSBTrackerImpl);

    ADSBTrackerImpl(std::string_view const& deviceName) : _dump1090Provider(ADSB::CreateDump1090Provider(deviceName))
    {
        std::cout << "ADSB Tracker Initializing" << std::endl;
        _dump1090Provider->Start(*this);
    }

    ~ADSBTrackerImpl() override { _dump1090Provider->Stop(); }
#if 0
    auto _FindOrInsert(DataSource<AvidTrafficData>::EditContext& ctx, uint32_t addr, ADSB::IAirCraft const& a)
    {
        auto it = _aircrafts.find(addr);
        if (it != _aircrafts.end())
        {
            return ctx.TXN().edit_aircrafts(size_t{it->second});
        }

        Avid::Aircraft::Data newair;
        newair.set_addr(uint32_t{addr});
        auto                flightNumber = a.FlightNumber();
        std::array<char, 6> tailNumber{' '};
        std::copy_n(flightNumber.begin(), std::min(flightNumber.length(), tailNumber.size()), tailNumber.begin());
        newair.set_tailNumber(std::move(tailNumber));
        ctx.TXN().add_aircrafts(std::move(newair));
        auto index = _aircrafts[addr] = ctx.Obj().aircrafts().size() - 1;
        return ctx.TXN().edit_aircrafts(size_t{index});
    }
#endif

    void OnMessage(ADSB::IModeMessage const& /*msg*/, ADSB::IAirCraft const& a) override
    {
#if 0
        auto addr   = a.Addr();
        auto ctx    = EditData();
        auto subctx = _FindOrInsert(ctx, addr, a);
        subctx.Visit(Avid::Aircraft::Data::FieldIndex::loc,
                     [&]([[maybe_unused]] auto& sublocctx)
                     {
                         if constexpr (std::is_same<decltype(sublocctx), Stencil::Transaction<Avid::Location::Data>&>::value)
                         {
                             sublocctx.set_lat1E7(a.Lat1E7());
                             sublocctx.set_lon1E7(a.Lon1E7());
                             sublocctx.set_altitude(a.Altitude());
                             sublocctx.set_time(a.LastSeen());
                             sublocctx.set_groundSpeed(a.Speed());
                             sublocctx.set_heading(a.Heading());
                             sublocctx.set_climb(a.Climb());
                         }
                     });
#endif
    }

    std::unordered_map<uint32_t, std::chrono::system_clock::time_point> _icaoTimestamps;
    std::unordered_map<uint32_t, size_t>                                _aircrafts;
    std::unique_ptr<ADSB::IDataProvider>                                _dump1090Provider;

    Config               _config{};
    std::vector<uint8_t> _data;

    // DataRecorder<Avid::Aircraft::Data> _recorder;
    std::mutex _mutex;
};

int main()
{
    ADSBTrackerImpl tracker("");

    std::this_thread::sleep_for(10s);
}
