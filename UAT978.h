#include "RTLSDR.h"

extern "C" void init_fec();
extern "C" int  process_buffer(uint16_t const*, size_t len, int c);
extern "C" void make_atan2_table();
extern "C" void convert_to_phi(uint16_t* buffer, int n);

struct MessageHandler978 : RTLSDR::IDataHandler
{
    MessageHandler978(uint32_t index978) : _listener978{index978, RTLSDR::Config{.gain = 48, .frequency = 978000000, .sampleRate = 2083334}}
    {
    }

    // Inherited via IDataHandler
    virtual void HandleData(std::span<uint8_t const> const& dataBytes) override
    {
        std::span<uint16_t const> data(reinterpret_cast<uint16_t const*>(dataBytes.data()), dataBytes.size() / 2);

        size_t j = 0;
        size_t i = _used;
        while (j < data.size())
        {
            for (i = _used; i < std::size(_buffer) && j < data.size(); i++, j++)
            {
                _buffer[i] = _iqphase[data[j]];
            }

            auto bufferProcessed = process_buffer(_buffer, i, _offset);
            _offset+= bufferProcessed;
            // Move the rest of the buffer to the start
            memmove(_buffer, _buffer + bufferProcessed, i - bufferProcessed);
            _used = i - bufferProcessed; 
        }
    }

    void Start(ADSB::IListener& listener)
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

                u.iq[0]          = i;
                u.iq[1]          = q;
                _iqphase[u.iq16] = (scaled_ang < 0 ? 0 : scaled_ang > 65535 ? 65535 : (uint16_t)scaled_ang);
            }
        }
    }

    RTLSDR   _listener978;
    size_t   _used;
    uint64_t _offset = 0;
    uint16_t _buffer[256 * 256];
    uint16_t _iqphase[256 * 256];
};
