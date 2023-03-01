#pragma once

#include "AircraftImpl.h"
#include "RTLSDR.h"

#include <cmath>
#include <cstring>

extern "C" void        init_fec();
extern "C" int         process_buffer(uint16_t const*, size_t len, uint64_t c);
extern "C" void        make_atan2_table();
extern "C" void        convert_to_phi(uint16_t* buffer, int n);
struct UAT978Handler** GetThreadLocalUAT978Handler();

inline struct UAT978Handler** GetThreadLocalUAT978Handler()
{
    SUPPRESS_WARNINGS_START
    static thread_local struct UAT978Handler* handler;
    SUPPRESS_WARNINGS_END
    return &handler;
}

struct UAT978Handler : RTLSDR::IDataHandler
{
    friend void dump_raw_message(char /*updown*/, uint8_t* data, int /*len*/, int /*rs_errors*/);

    UAT978Handler(std::shared_ptr<TrafficManager> trafficManager, RTLSDR::IDeviceSelector const* selector, uint8_t sourceId) :
        _trafficManager(trafficManager),
        _listener978{selector, RTLSDR::Config{.gain = 48, .frequency = 978000000, .sampleRate = 2083334}},
        _sourceId(sourceId)
    {
        std::fill(std::begin(_buffer), std::end(_buffer), uint16_t{0u});
        std::fill(std::begin(_iqphase), std::end(_iqphase), uint16_t{0u});
    }

    ~UAT978Handler() override = default;

    CLASS_DELETE_COPY_AND_MOVE(UAT978Handler);

    // Inherited via IDataHandler
    virtual void HandleData(std::span<uint8_t const> const& dataBytes) override
    {
        std::span<uint16_t const> data(reinterpret_cast<uint16_t const*>(dataBytes.data()), dataBytes.size() / 2);
        *GetThreadLocalUAT978Handler() = this;

        size_t j = 0;
        size_t i = _used;
        while (j < data.size())
        {
            for (i = _used; i < std::size(_buffer) && j < data.size(); i++, j++)
            {
                _buffer[i] = _iqphase[data[j]];
            }

            int bufferProcessed = process_buffer(_buffer, i, _offset);
            _offset             = static_cast<uint64_t>(static_cast<int64_t>(_offset) + bufferProcessed);
            // Move the rest of the buffer to the start
            std::memmove(_buffer, _buffer + bufferProcessed, static_cast<size_t>(static_cast<int>(i) - bufferProcessed));
            _used = static_cast<size_t>(static_cast<int>(i) - bufferProcessed);
        }
    }

    void Start(ADSB::IListener& /*listener*/)
    {
        _InitATan2Table();
        init_fec();
        _listener978.Start(this);
    }

    void Stop() { _listener978.Stop(); }

    void _InitATan2Table()
    {
        unsigned i, q;
        union
        {
            uint8_t  iq[2];
            uint16_t iq16;
        } u;

        for (i = 0; i < 256; ++i)
        {
            double d_i = (i - 127.5);
            for (q = 0; q < 256; ++q)
            {
                double d_q        = (q - 127.5);
                double ang        = atan2(d_q, d_i) + M_PI;    // atan2 returns [-pi..pi], normalize to [0..2*pi]
                double scaled_ang = round(32768 * ang / M_PI);

                u.iq[0]          = static_cast<uint8_t>(i);
                u.iq[1]          = static_cast<uint8_t>(q);
                _iqphase[u.iq16] = static_cast<uint16_t>(scaled_ang < 0 ? 0 : scaled_ang > 65535 ? 65535 : scaled_ang);
            }
        }
    }

    std::shared_ptr<TrafficManager> _trafficManager;
    RTLSDR                          _listener978;
    size_t                          _used   = 0;
    uint64_t                        _offset = 0;
    uint16_t                        _buffer[256 * 256];
    uint16_t                        _iqphase[256 * 256];
    uint8_t                         _sourceId{2};

    static std::unique_ptr<UAT978Handler>
    TryCreate(std::shared_ptr<TrafficManager> trafficManager, RTLSDR::IDeviceSelector const* selector, uint8_t sourceId)
    {
        return std::make_unique<UAT978Handler>(trafficManager, selector, sourceId);
    }
};
