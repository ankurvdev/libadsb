#pragma once
#include <chrono>
#include <memory>
#include <string_view>

namespace ADSB
{
struct Config
{
    std::chrono::duration<std::chrono::system_clock> ttl;
};

struct IAirCraft
{
    using time_point = std::chrono::time_point<std::chrono::system_clock>;

    virtual ~IAirCraft()                          = default;
    virtual uint32_t         MessageCount() const = 0;
    virtual uint32_t         Addr() const         = 0;
    virtual std::string_view FlightNumber() const = 0;
    virtual time_point       LastSeen() const     = 0;
    virtual uint32_t         SquakCode() const    = 0;
    virtual int32_t          Altitude() const     = 0;
    virtual uint32_t         Speed() const        = 0;
    virtual uint32_t         Heading() const      = 0;
    virtual int32_t          Climb() const        = 0;
    virtual int32_t          Lat1E7() const       = 0;
    virtual int32_t          Lon1E7() const       = 0;
};

struct IListener
{
    virtual ~IListener()                     = default;
    virtual void OnChanged(IAirCraft const&) = 0;
};

struct IDataProvider
{
    virtual ~IDataProvider() = default;

    virtual void Start(IListener& listener) = 0;
    virtual void Stop()                     = 0;
};

std::unique_ptr<IDataProvider> CreateDump1090Provider(std::string_view const& deviceName);
}    // namespace ADSB
