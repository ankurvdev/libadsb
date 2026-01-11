#include "ADSB1090.h"
#include "ADSBListener.h"
#include "RTLSDR.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
// NOLINTBEGIN

extern "C" void init_fec();
extern "C" int  process_buffer(uint16_t const*, int len, uint64_t c);

ADSB::TrafficManager** ADSB::GetThreadLocalTrafficManager()
{
    SUPPRESS_WARNINGS_START
    SUPPRESS_CLANG_WARNING("-Wunique-object-duplication")
    static thread_local TrafficManager* TrafficManager;
    SUPPRESS_WARNINGS_END
    return &TrafficManager;
}

struct UAT978Handler : RTLSDR::IDataHandler, ADSB::IDataProvider
{
    friend void DumpRawMessage(char /*updown*/, uint8_t* data, int /*len*/, int /*rs_errors*/);

    UAT978Handler(std::shared_ptr<ADSB::TrafficManager> trafficManagerIn,
                  RTLSDR::IDeviceSelector const*        selectorIn,
                  ADSB::Source                          sourceIdIn) :
        trafficManager(std::move(std::move(trafficManagerIn))),
        listener978{selectorIn, RTLSDR::Config{.gain = 48, .frequency = 978000000, .sampleRate = 2083334}},
        sourceId(sourceIdIn)
    {
        std::ranges::fill(buffer, uint16_t{0u});
        std::ranges::fill(iqphase, uint16_t{0u});
        InitATan2Table();
        init_fec();
    }

    ~UAT978Handler() override = default;

    CLASS_DELETE_COPY_AND_MOVE(UAT978Handler);

    // Inherited via IDataHandler
    void HandleData(std::span<uint8_t const> const& dataBytes) override
    {
        std::span<uint16_t const> data(reinterpret_cast<uint16_t const*>(dataBytes.data()), dataBytes.size() / 2);    // NOLINT
        *ADSB::GetThreadLocalTrafficManager() = this->trafficManager.get();

        size_t j = 0;
        size_t i = used;
        while (j < data.size())
        {
            for (i = used; i < std::size(buffer) && j < data.size(); i++, j++) { buffer[i] = iqphase[data[j]]; }

            int bufferProcessed = process_buffer(buffer.data(), static_cast<int>(i), offset);
            offset              = static_cast<uint64_t>(static_cast<int64_t>(offset) + bufferProcessed);
            // Move the rest of the buffer to the start
            std::memmove(buffer.data(), buffer.data() + bufferProcessed, static_cast<size_t>(static_cast<int>(i) - bufferProcessed));
            used = static_cast<size_t>(static_cast<int>(i) - bufferProcessed);
        }
    }

    void OnDeviceStatusChanged(bool available) override { listener->OnDeviceStatusChanged(sourceId, available); }

    void Start(ADSB::IListener& listenerIn) override
    {
        listener = &listenerIn;
        listener978.Start(this);
    }
    void Stop() override
    {
        listener = nullptr;
        listener978.Stop();
    }

    void NotifySelfLocation(ADSB::IAirCraft const& /*unused*/) override {}

    void InitATan2Table()
    {
        unsigned i;
        unsigned q;
        union
        {
            uint8_t  iq[2];
            uint16_t iq16;
        } u{};

        for (i = 0; i < 256; ++i)
        {
            double dI = (i - 127.5);
            for (q = 0; q < 256; ++q)
            {
                double dQ        = (q - 127.5);
                double ang       = atan2(dQ, dI) + M_PI;    // atan2 returns [-pi..pi], normalize to [0..2*pi]
                double scaledAng = round(32768 * ang / M_PI);

                u.iq[0]         = static_cast<uint8_t>(i);
                u.iq[1]         = static_cast<uint8_t>(q);
                iqphase[u.iq16] = static_cast<uint16_t>(scaledAng < 0 ? 0 : scaledAng > 65535 ? 65535 : scaledAng);
            }
        }
    }

    ADSB::IListener* listener{nullptr};

    std::shared_ptr<ADSB::TrafficManager> trafficManager;
    RTLSDR                                listener978;
    size_t                                used   = 0;
    uint64_t                              offset = 0;
    std::array<uint16_t, 256 * 256>       buffer{};
    std::array<uint16_t, 256 * 256>       iqphase{};
    ADSB::Source                          sourceId{ADSB::Source::UAT978};
};
// NOLINTEND

std::unique_ptr<ADSB::IDataProvider> ADSB::TryCreateUAT978Handler(std::shared_ptr<ADSB::TrafficManager> const& trafficManager,
                                                                  RTLSDR::IDeviceSelector const*               selector,
                                                                  ADSB::Source                                 sourceId)
{
    return std::make_unique<UAT978Handler>(trafficManager, selector, sourceId);
}

std::unique_ptr<RTLSDR::IDataHandler> ADSB::test::TryCreateUAT978Handler(std::shared_ptr<ADSB::TrafficManager> const& trafficManager,
                                                                         RTLSDR::IDeviceSelector const*               selector,
                                                                         ADSB::Source                                 sourceId)
{
    return std::make_unique<UAT978Handler>(trafficManager, selector, sourceId);
}
