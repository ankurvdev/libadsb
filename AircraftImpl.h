#pragma once
#include "ADSBListener.h"

#include <memory>
#include <unordered_map>
struct AirCraftImpl : ADSB::IAirCraft
{
    AirCraftImpl() = default;

    virtual uint32_t         MessageCount() const override { return 0; }
    virtual uint32_t         Addr() const override { return addr; }
    virtual std::string_view FlightNumber() const override { return callsign; }
    virtual time_point       LastSeen() const override { return seen; }
    virtual uint32_t         SquakCode() const override { return modeA; }
    virtual int32_t          Altitude() const override { return altitude; }
    virtual uint32_t         Speed() const override { return speed; }
    virtual uint32_t         Heading() const override { return track; }
    virtual int32_t          Climb() const override { return vert_rate; }
    virtual int32_t          Lat1E7() const override { return static_cast<int32_t>(lat * 10000000.0); }
    virtual int32_t          Lon1E7() const override { return static_cast<int32_t>(lon * 10000000.0); }

    uint32_t   addr{0};
    char       callsign[9]{};
    time_point seen{};
    uint32_t   modeA{};
    int32_t    altitude{};
    uint32_t   speed{};
    uint32_t   track{};
    int32_t    vert_rate{};
    double     lat{};
    double     lon{};
};

struct TrafficManager : std::enable_shared_from_this<TrafficManager>
{
    AirCraftImpl& FindOrCreate(uint32_t addr)
    {
        auto it = _aircrafts.find(addr);
        if (it != _aircrafts.end())
        {
            return *it->second.get();
        }

        auto aptr        = new AirCraftImpl();
        _aircrafts[addr] = std::unique_ptr<AirCraftImpl>(aptr);
        auto& a          = *aptr;
        a.addr           = addr;
        return a;
    }

    void SetListener(ADSB::IListener* l) { _listener = l; }
    void NotifyChanged(AirCraftImpl const& a) { _listener->OnChanged(a); }

    std::unordered_map<uint32_t, std::unique_ptr<AirCraftImpl>> _aircrafts;
    ADSB::IListener*                                            _listener{nullptr};
};
