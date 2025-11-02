#pragma once
#include "ADSBListener.h"

#include <memory>
#include <unordered_map>
namespace ADSB
{

struct AirCraftImpl : ADSB::IAirCraft
{
    AirCraftImpl() = default;
    [[nodiscard]] uint8_t          SourceId() const override { return sourceId; }
    [[nodiscard]] uint32_t         MessageCount() const override { return 0; }
    [[nodiscard]] uint32_t         Addr() const override { return addr; }
    [[nodiscard]] std::string_view FlightNumber() const override { return {callsign.data(), callsign.size()}; }
    [[nodiscard]] time_point       LastSeen() const override { return seen; }
    [[nodiscard]] uint32_t         SquakCode() const override { return modeA; }
    [[nodiscard]] int32_t          Altitude() const override { return altitude; }
    [[nodiscard]] uint32_t         Speed() const override { return speed; }
    [[nodiscard]] uint32_t         Heading() const override { return track; }
    [[nodiscard]] int32_t          Climb() const override { return vertRate; }
    //    virtual int32_t Lat1E7() const override { return static_cast<int32_t>(((odd_time > even_time) ? odd_lat : even_lat) * 10000000.0);
    //    } virtual int32_t Lon1E7() const override { return static_cast<int32_t>(((odd_time > even_time) ? odd_lon : even_lon) *
    //    10000000.0); }
    [[nodiscard]] int32_t Lat1E7() const override { return lat1E7; }
    [[nodiscard]] int32_t Lon1E7() const override { return lon1E7; }

    uint32_t            addr{0};
    std::array<char, 8> callsign{};
    time_point          seen{};
    uint32_t            modeA{};
    int32_t             altitude{};
    uint32_t            speed{};
    uint32_t            track{};
    int32_t             vertRate{};
    int32_t             lat1E7{};
    int32_t             lon1E7{};

    // Used for 1090 ADSB Decoding
    double     cprOddLat{};
    double     cprOddLon{};
    time_point cprOddTime{};
    double     cprEvenLat{};
    double     cprEvenLon{};
    time_point cprEvenTime{};

    uint8_t sourceId{};
};

struct TrafficManager : std::enable_shared_from_this<TrafficManager>
{
    AirCraftImpl& FindOrCreate(uint32_t addr)
    {
        auto it = aircrafts.find(addr);
        if (it != aircrafts.end()) { return *it->second.get(); }

        auto aptr       = new AirCraftImpl();
        aircrafts[addr] = std::unique_ptr<AirCraftImpl>(aptr);
        auto& a         = *aptr;
        a.addr          = addr;
        return a;
    }

    void SetListener(ADSB::IListener* l) { listener = l; }
    void NotifyChanged(AirCraftImpl const& a) const { listener->OnChanged(a); }

    std::unordered_map<uint32_t, std::unique_ptr<AirCraftImpl>> aircrafts;
    ADSB::IListener*                                            listener{nullptr};
};
}    // namespace ADSB
