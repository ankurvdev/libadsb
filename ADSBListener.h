#pragma once
#include "CommonMacros.h"

SUPPRESS_WARNINGS_START
SUPPRESS_STL_WARNINGS
#include <chrono>
SUPPRESS_WARNINGS_END

#include <memory>
#include <string_view>

namespace ADSB
{

enum class Source : uint8_t
{
    UAT978        = 1u,
    ADSB1090      = 2u,
    FlightRadar24 = 4u,
};

struct Config
{
    std::chrono::duration<std::chrono::system_clock> ttl;
};

struct IAirCraft
{
    static constexpr double LatLonPrecision = 10000000.;

    using time_point = std::chrono::time_point<std::chrono::system_clock>;

    IAirCraft()          = default;
    virtual ~IAirCraft() = default;

    CLASS_DEFAULT_COPY_AND_MOVE(IAirCraft);

    [[nodiscard]] virtual Source           SourceId() const     = 0;
    [[nodiscard]] virtual uint32_t         MessageCount() const = 0;
    [[nodiscard]] virtual uint32_t         Addr() const         = 0;
    [[nodiscard]] virtual std::string_view FlightNumber() const = 0;
    [[nodiscard]] virtual time_point       LastSeen() const     = 0;
    [[nodiscard]] virtual uint32_t         SquakCode() const    = 0;
    [[nodiscard]] virtual int32_t          Altitude() const     = 0;
    [[nodiscard]] virtual uint32_t         Speed() const        = 0;
    [[nodiscard]] virtual uint32_t         Heading() const      = 0;
    [[nodiscard]] virtual int32_t          Climb() const        = 0;
    [[nodiscard]] virtual int32_t          Lat1E7() const       = 0;
    [[nodiscard]] virtual int32_t          Lon1E7() const       = 0;
};

struct IListener
{
    IListener()          = default;
    virtual ~IListener() = default;
    CLASS_DEFAULT_COPY_AND_MOVE(IListener);

    virtual void OnChanged(IAirCraft const&)                            = 0;
    virtual void OnDeviceStatusChanged(Source sourceId, bool available) = 0;
};

struct IDataProvider
{
    IDataProvider()          = default;
    virtual ~IDataProvider() = default;
    CLASS_DEFAULT_COPY_AND_MOVE(IDataProvider);

    virtual void Start(IListener& listener) = 0;
    virtual void Stop()                     = 0;

    virtual void NotifySelfLocation(IAirCraft const&) = 0;
};

std::unique_ptr<IDataProvider> CreateADSB1090Provider();
std::unique_ptr<IDataProvider> CreateUAT978Provider();
std::unique_ptr<IDataProvider> CreateFlightRadar24();

}    // namespace ADSB
