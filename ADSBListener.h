#pragma once
#include <chrono>
#include <functional>
#include <thread>

namespace ADSB
{
struct Config
{
    std::chrono::duration<std::chrono::system_clock> ttl;
};

struct IModeMessage
{
    virtual ~IModeMessage() = default;
};

struct IAirCraft
{
    virtual ~IAirCraft() = default;
};

struct Listener
{
    using Callback = std::function<void(IModeMessage const&, IAirCraft const&)>;

    void OnMessage(Callback&& callback) { _callback = std::move(callback); }
    void Invoke(IModeMessage const&, IAirCraft const&) const;
    void Start();
    void Stop();

    private:
    std::thread _thread;
    Callback    _callback;
};
}    // namespace ADSB
