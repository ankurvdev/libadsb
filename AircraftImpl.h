#pragma once
#include "ADSBListener.h"

struct AirCraftImpl : ADSB::IAirCraft
{
    AirCraftImpl() = default;

    virtual uint32_t         MessageCount() const override { return 0; }
    virtual uint32_t         Addr() const override { return addr; }
    virtual std::string_view FlightNumber() const override { return flight; }
    virtual time_point       LastSeen() const override { return seen; }
    virtual uint32_t         SquakCode() const override { return modeA; }
    virtual int32_t          Altitude() const override { return altitude; }
    virtual uint32_t         Speed() const override { return speed; }
    virtual uint32_t         Heading() const override { return track; }
    virtual int32_t          Climb() const override { return vert_rate; }
    virtual int32_t          Lat1E7() const override { return static_cast<int32_t>(lat * 10000000.0); }
    virtual int32_t          Lon1E7() const override { return static_cast<int32_t>(lon * 10000000.0); }

    uint32_t   addr{0};
    char       flight[7]{};
    time_point seen{};
    uint32_t   modeA{};
    int32_t    altitude{};
    uint32_t   speed{};
    uint32_t   track{};
    int32_t    vert_rate{};
    double     lat{};
    double     lon{};
};
