#pragma once
#include "AircraftImpl.h"
#include "RTLSDR.h"

SUPPRESS_WARNINGS_START
SUPPRESS_STL_WARNINGS

#include <cmath>
#include <future>
#include <span>
#include <thread>
#include <unordered_map>
#include <vector>
SUPPRESS_WARNINGS_END

#include <string.h>

#define MODES_PREAMBLE_US 8 /* microseconds */

#define MODES_FULL_LEN (MODES_PREAMBLE_US + Message::LongMessageBits)

#define MODES_ICAO_CACHE_TTL 60 /* Time to live of cached addresses. */
#define MODES_UNIT_FEET 0
#define MODES_UNIT_METERS 1

// #define MODES_DEBUG_DEMOD (1 << 0)
#define MODES_DEBUG_DEMODERR (1 << 1)
// #define MODES_DEBUG_BADCRC (1 << 2)
// #define MODES_DEBUG_GOODCRC (1 << 3)
// #define MODES_DEBUG_NOPREAMBLE (1 << 4)
// #define MODES_DEBUG_NET (1 << 5)
// #define MODES_DEBUG_JS (1 << 6)
using time_point = std::chrono::time_point<std::chrono::system_clock>;
/* When debug is set to MODES_DEBUG_NOPREAMBLE, the first sample must be
 * at least greater than a given level for us to dump the signal. */
// #define MODES_DEBUG_NOPREAMBLE_LEVEL 25

/* The struct we use to store information about a decoded message. */
struct Message
{
    static constexpr size_t LongMessageBits  = 112;
    static constexpr size_t LongMessageBytes = LongMessageBits / 8;

    static constexpr size_t ShortMessageBits  = 56;
    static constexpr size_t ShortMessageBytes = ShortMessageBits / 8;

    /* Generic fields */
    uint8_t  msg[LongMessageBytes]; /* Binary message. */
    size_t   msgbits;               /* Number of bits in message */
    int      msgtype;               /* Downlink format # */
    int      crcok;                 /* True if CRC was valid */
    uint32_t crc;                   /* Message CRC */
    int      errorbit;              /* Bit corrected. -1 if no bit corrected. */
    int      aa1, aa2, aa3;         /* ICAO Address bytes 1 2 and 3 */
    int      phase_corrected;       /* True if phase correction was applied. */

    /* DF 11 */
    int ca; /* Responder capabilities. */

    /* DF 17 */
    int  metype; /* Extended squitter message type. */
    int  mesub;  /* Extended squitter message subtype. */
    int  heading_is_valid;
    int  heading;
    int  aircraft_type;
    int  fflag;            /* 1 = Odd, 0 = Even CPR message. */
    int  tflag;            /* UTC synchronized? */
    int  raw_latitude;     /* Non decoded latitude */
    int  raw_longitude;    /* Non decoded longitude */
    char flight[9];        /* 8 chars flight number. */
    int  ew_dir;           /* 0 = East, 1 = West. */
    int  ew_velocity;      /* E/W velocity. */
    int  ns_dir;           /* 0 = North, 1 = South. */
    int  ns_velocity;      /* N/S velocity. */
    int  vert_rate_source; /* Vertical rate source. */
    int  vert_rate_sign;   /* Vertical rate sign. */
    int  vert_rate;        /* Vertical rate. */
    int  velocity;         /* Computed from EW and NS velocity. */

    /* DF4, DF5, DF20, DF21 */
    int fs;       /* Flight status for DF4,5,20,21 */
    int dr;       /* Request extraction of downlink request. */
    int um;       /* Request extraction of downlink request. */
    int identity; /* 13 bits identity (Squawk). */

    /* Fields used by multiple message types. */
    int altitude, unit;
};

struct ADSB1090Handler : RTLSDR::IDataHandler
{
    static constexpr size_t PreambleUS = 8; /*microseconds*/

    static constexpr size_t LongMessageBits   = 112;
    static constexpr size_t ShortMessageBits  = 56;
    static constexpr size_t FullLength        = PreambleUS + LongMessageBits;
    static constexpr size_t LongMessageBytes  = LongMessageBits / 8;
    static constexpr size_t ShortMessageBytes = ShortMessageBits / 8;

    static constexpr size_t BufferLength = (RTLSDR::BufferCount * RTLSDR::BufferLength) + (FullLength - 1) * 4;

    struct Config
    {

        bool fixErrors   = 1;
        bool checkCRC    = 1;
        bool raw         = 0;
        bool onlyaddr    = 0;
        bool debug       = 0;
        bool interactive = 0;
        bool aggressive  = 0;
        bool loop        = 0;
    };
    struct DeviceSelector : RTLSDR::IDeviceSelector
    {
        virtual bool SelectDevice(RTLSDR::DeviceInfo const& d) const override
        {
            return std::string_view(d.serial).find("1090") != std::string_view::npos;
        }
    };

    static std::vector<uint16_t> _CreateLUT()
    {
        std::vector<uint16_t> lut(129 * 129 * 2);
        for (uint8_t i = 0; i <= 128; i++)
        {
            for (uint8_t q = 0; q <= 128; q++) { lut[i * 129u + q] = static_cast<uint16_t>(std::round(std::sqrt(i * i + q * q) * 360)); }
        }
        return lut;
    }

    ADSB1090Handler(std::shared_ptr<TrafficManager> trafficManager, RTLSDR::IDeviceSelector const* selector, uint8_t sourceId) :
        _trafficManager(trafficManager),
        _listener1090{selector, RTLSDR::Config{.frequency = 1090000000, .sampleRate = 2000000}},
        _sourceId(sourceId)
    {
        std::cout << "ADSB Tracker Initializing" << std::endl;
    }
    CLASS_DELETE_COPY_AND_MOVE(ADSB1090Handler);

    virtual void HandleData(std::span<uint8_t const> const& data) override
    {
        uint16_t* m = _magnitudeVector.data();
        auto*     p = data.data();

        /* Compute the magnitudo vector. It's just SQRT(I^2 + Q^2), but
         * we rescale to the 0-255 range to exploit the full resolution. */
        for (uint32_t j = 0; j < data.size(); j += 2)
        {
            int i = p[j] - 127;
            int q = p[j + 1] - 127;

            if (i < 0) i = -i;
            if (q < 0) q = -q;
            m[j / 2] = _magnitudesLookupTable[static_cast<size_t>(i * 129 + q)];
        }
        _detectModeS(m, static_cast<uint32_t>(data.size() / 2));
    }

    void Start(ADSB::IListener& /*listener*/) { _listener1090.Start(this); }
    void Stop() { _listener1090.Stop(); }

    /* Add the specified entry to the cache of recently seen ICAO addresses.
     * Note that we also add a timestamp so that we can make sure that the
     * entry is only valid for MODES_ICAO_CACHE_TTL seconds. */
    void _addRecentlySeenICAOAddr(uint32_t addr) { _icaoTimestamps[addr] = std::chrono::system_clock::now(); }

    /* Returns 1 if the specified ICAO address was seen in a DF format with
     * proper checksum (not xored with address) no more than * MODES_ICAO_CACHE_TTL
     * seconds ago. Otherwise returns 0. */
    bool _ICAOAddressWasRecentlySeen(uint32_t addr)
    {
        auto it = _icaoTimestamps.find(addr);
        return it != _icaoTimestamps.end()
               && (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - it->second).count()
                   <= MODES_ICAO_CACHE_TTL);
    }
    int           _bruteForceAP(uint8_t* msg, Message* mm);
    void          _decodeModesMessage(Message* mm, uint8_t* msg);
    void          _detectModeS(uint16_t* m, uint32_t mlen);
    AirCraftImpl& _interactiveReceiveData(Message* mm);
    AirCraftImpl& _interactiveFindOrCreateAircraft(uint32_t addr);

    void _useModesMessage(Message* mm);
    void _modesSendSBSOutput(Message* mm, AirCraftImpl& a);

    static std::unique_ptr<ADSB1090Handler>
    TryCreate(std::shared_ptr<TrafficManager> trafficManager, RTLSDR::IDeviceSelector const* selector, uint8_t sourceId)
    {
        return std::make_unique<ADSB1090Handler>(trafficManager, selector, sourceId);
    }

    std::unordered_map<uint32_t, std::chrono::system_clock::time_point> _icaoTimestamps;

    Config                _config{};
    std::vector<uint16_t> _magnitudesLookupTable = _CreateLUT();
    std::vector<uint8_t>  _data;
    std::vector<uint16_t> _magnitudeVector = std::vector<uint16_t>(BufferLength, 0xffff);

    std::shared_ptr<TrafficManager> _trafficManager;

    // DataRecorder<AirCraftImpl> _recorder;
    std::mutex        _mutex;
    std::atomic<bool> _stopRequested{false};
    DeviceSelector    _selector;
    RTLSDR            _listener1090;
    uint8_t           _sourceId{1};
    /* Statistics */
    long long _stat_valid_preamble{};
    long long _stat_demodulated{};
    long long _stat_goodcrc{};
    long long _stat_badcrc{};
    long long _stat_fixed{};
    long long _stat_single_bit_fix{};
    long long _stat_two_bits_fix{};
    long long _stat_sbs_connections{};
    long long _stat_out_of_phase{};
};

/* ===================== Mode S detection and decoding  ===================== */

/* Parity table for MODE S Messages.
 * The table contains 112 elements, every element corresponds to a bit set
 * in the message, starting from the first bit of actual data after the
 * preamble.
 *
 * For messages of 112 bit, the whole table is used.
 * For messages of 56 bits only the last 56 elements are used.
 *
 * The algorithm is as simple as xoring all the elements in this table
 * for which the corresponding bit on the message is set to 1.
 *
 * The latest 24 elements in this table are set to 0 as the checksum at the
 * end of the message should not affect the computation.
 *
 * Note: this function can be used with DF11 and DF17, other modes have
 * the CRC xored with the sender address as they are reply to interrogations,
 * but a casual listener can't split the address from the checksum.
 */
static uint32_t modes_checksum_table[112]
    = {0x3935ea, 0x1c9af5, 0xf1b77e, 0x78dbbf, 0xc397db, 0x9e31e9, 0xb0e2f0, 0x587178, 0x2c38bc, 0x161c5e, 0x0b0e2f, 0xfa7d13, 0x82c48d,
       0xbe9842, 0x5f4c21, 0xd05c14, 0x682e0a, 0x341705, 0xe5f186, 0x72f8c3, 0xc68665, 0x9cb936, 0x4e5c9b, 0xd8d449, 0x939020, 0x49c810,
       0x24e408, 0x127204, 0x093902, 0x049c81, 0xfdb444, 0x7eda22, 0x3f6d11, 0xe04c8c, 0x702646, 0x381323, 0xe3f395, 0x8e03ce, 0x4701e7,
       0xdc7af7, 0x91c77f, 0xb719bb, 0xa476d9, 0xadc168, 0x56e0b4, 0x2b705a, 0x15b82d, 0xf52612, 0x7a9309, 0xc2b380, 0x6159c0, 0x30ace0,
       0x185670, 0x0c2b38, 0x06159c, 0x030ace, 0x018567, 0xff38b7, 0x80665f, 0xbfc92b, 0xa01e91, 0xaff54c, 0x57faa6, 0x2bfd53, 0xea04ad,
       0x8af852, 0x457c29, 0xdd4410, 0x6ea208, 0x375104, 0x1ba882, 0x0dd441, 0xf91024, 0x7c8812, 0x3e4409, 0xe0d800, 0x706c00, 0x383600,
       0x1c1b00, 0x0e0d80, 0x0706c0, 0x038360, 0x01c1b0, 0x00e0d8, 0x00706c, 0x003836, 0x001c1b, 0xfff409, 0x000000, 0x000000, 0x000000,
       0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
       0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000};

static uint32_t modesChecksum(uint8_t* msg, size_t bits)
{
    uint32_t crc    = 0;
    size_t   offset = (bits == 112) ? 0u : (112u - 56u);
    for (size_t j = 0; j < bits; j++)
    {
        auto    byte    = j / 8;
        auto    bit     = j % 8;
        uint8_t bitmask = static_cast<uint8_t>(1 << (7 - bit));

        /* If bit is set, xor with corresponding table entry. */
        if (msg[byte] & bitmask) crc ^= modes_checksum_table[j + offset];
    }
    return crc; /* 24 bit checksum. */
}

/* Given the Downlink Format (DF) of the message, return the message length
 * in bits. */
static size_t modesMessageLenByType(int type)
{
    if (type == 16 || type == 17 || type == 19 || type == 20 || type == 21)
        return Message::LongMessageBits;
    else
        return Message::ShortMessageBits;
}

/* Try to fix single bit errors using the checksum. On success modifies
 * the original buffer with the fixed version, and returns the position
 * of the error bit. Otherwise if fixing failed -1 is returned. */
static int fixSingleBitErrors(uint8_t* msg, size_t bits)
{
    uint8_t aux[Message::LongMessageBits / 8];

    for (size_t j = 0; j < bits; j++)
    {
        size_t   byte    = j / 8;
        uint8_t  bitmask = static_cast<uint8_t>(1u << (7u - (j % 8)));
        uint32_t crc1, crc2;

        memcpy(aux, msg, static_cast<size_t>(bits / 8));
        aux[byte] ^= bitmask; /* Flip j-th bit. */

        crc1 = (uint32_t{aux[(bits / 8) - 3]} << 16) | (uint32_t{aux[(bits / 8) - 2]} << 8) | uint32_t{aux[(bits / 8) - 1]};
        crc2 = modesChecksum(aux, bits);

        if (crc1 == crc2)
        {
            /* The error is fixed. Overwrite the original buffer with
             * the corrected sequence, and returns the error bit
             * position. */
            memcpy(msg, aux, static_cast<size_t>(bits / 8));
            return static_cast<int>(j);
        }
    }
    return -1;
}

/* Similar to fixSingleBitErrors() but try every possible two bit combination.
 * This is very slow and should be tried only against DF17 messages that
 * don't pass the checksum, and only in Aggressive Mode. */
static int fixTwoBitsErrors(uint8_t* msg, size_t bits)
{
    size_t  j, i;
    uint8_t aux[Message::LongMessageBits / 8];

    for (j = 0; j < bits; j++)
    {
        auto byte1    = j / 8;
        int  bitmask1 = 1 << (7 - (j % 8));

        /* Don't check the same pairs multiple times, so i starts from j+1 */
        for (i = j + 1; i < bits; i++)
        {
            auto     byte2    = i / 8;
            int      bitmask2 = 1 << (7 - (i % 8));
            uint32_t crc1, crc2;

            memcpy(aux, msg, static_cast<size_t>(bits / 8));

            aux[byte1] ^= bitmask1; /* Flip j-th bit. */
            aux[byte2] ^= bitmask2; /* Flip i-th bit. */

            crc1 = (static_cast<uint32_t>(aux[(bits / 8) - 3]) << 16) | (static_cast<uint32_t>(aux[(bits / 8) - 2]) << 8)
                   | static_cast<uint32_t>(aux[(bits / 8) - 1]);
            crc2 = modesChecksum(aux, bits);

            if (crc1 == crc2)
            {
                /* The error is fixed. Overwrite the original buffer with
                 * the corrected sequence, and returns the error bit
                 * position. */
                memcpy(msg, aux, static_cast<size_t>(bits / 8));
                /* We return the two bits as a 16 bit integer by shifting
                 * 'i' on the left. This is possible since 'i' will always
                 * be non-zero because i starts from j+1. */
                return static_cast<int>(j | (i << 8));
            }
        }
    }
    return -1;
}

/* If the message type has the checksum xored with the ICAO address, try to
 * brute force it using a list of recently seen ICAO addresses.
 *
 * Do this in a brute-force fashion by xoring the predicted CRC with
 * the address XOR checksum field in the message. This will recover the
 * address: if we found it in our cache, we can assume the message is ok.
 *
 * This function expects mm->msgtype and mm->msgbits to be correctly
 * populated by the caller.
 *
 * On success the correct ICAO address is stored in the modesMessage
 * structure in the aa3, aa2, and aa1 fiedls.
 *
 * If the function successfully recovers a message with a correct checksum
 * it returns 1. Otherwise 0 is returned. */
int ADSB1090Handler::_bruteForceAP(uint8_t* msg, Message* mm)
{
    uint8_t aux[Message::LongMessageBits];
    int     msgtype = mm->msgtype;
    auto    msgbits = mm->msgbits;

    if (msgtype == 0 ||  /* Short air surveillance */
        msgtype == 4 ||  /* Surveillance, altitude reply */
        msgtype == 5 ||  /* Surveillance, identity reply */
        msgtype == 16 || /* Long Air-Air survillance */
        msgtype == 20 || /* Comm-A, altitude request */
        msgtype == 21 || /* Comm-A, identity request */
        msgtype == 24)   /* Comm-C ELM */
    {
        size_t lastbyte = static_cast<size_t>((msgbits / 8) - 1);

        /* Work on a copy. */
        memcpy(aux, msg, static_cast<size_t>(msgbits / 8));

        /* Compute the CRC of the message and XOR it with the AP field
         * so that we recover the address, because:
         *
         * (ADDR xor CRC) xor CRC = ADDR. */
        uint32_t crc = modesChecksum(aux, msgbits);
        aux[lastbyte] ^= crc & 0xff;
        aux[lastbyte - 1] ^= (crc >> 8) & 0xff;
        aux[lastbyte - 2] ^= (crc >> 16) & 0xff;

        /* If the obtained address exists in our cache we consider
         * the message valid. */
        uint32_t addr = uint32_t{aux[lastbyte]} | (uint32_t{aux[lastbyte - 1]} << 8) | (uint32_t{aux[lastbyte - 2]} << 16);
        if (_ICAOAddressWasRecentlySeen(addr))
        {
            mm->aa1 = aux[lastbyte - 2];
            mm->aa2 = aux[lastbyte - 1];
            mm->aa3 = aux[lastbyte];
            return 1;
        }
    }
    return 0;
}

/* Decode the 13 bit AC altitude field (in DF 20 and others).
 * Returns the altitude, and set 'unit' to either MODES_UNIT_METERS
 * or MDOES_UNIT_FEETS. */
static int decodeAC13Field(uint8_t* msg, int* unit)
{
    int m_bit = msg[3] & (1 << 6);
    int q_bit = msg[3] & (1 << 4);

    if (!m_bit)
    {
        *unit = MODES_UNIT_FEET;
        if (q_bit)
        {
            /* N is the 11 bit integer resulting from the removal of bit
             * Q and M */
            int n = ((msg[2] & 31) << 6) | ((msg[3] & 0x80) >> 2) | ((msg[3] & 0x20) >> 1) | (msg[3] & 15);
            /* The final altitude is due to the resulting number multiplied
             * by 25, minus 1000. */
            return n * 25 - 1000;
        }
        else { /* TODO: Implement altitude where Q=0 and M=0 */ }
    }
    else
    {
        *unit = MODES_UNIT_METERS;
        /* TODO: Implement altitude when meter unit is selected. */
    }
    return 0;
}

/* Decode the 12 bit AC altitude field (in DF 17 and others).
 * Returns the altitude or 0 if it can't be decoded. */
static int decodeAC12Field(uint8_t* msg, int* unit)
{
    int q_bit = msg[5] & 1;

    if (q_bit)
    {
        /* N is the 11 bit integer resulting from the removal of bit
         * Q */
        *unit = MODES_UNIT_FEET;
        int n = ((msg[5] >> 1) << 4) | ((msg[6] & 0xF0) >> 4);
        /* The final altitude is due to the resulting number multiplied
         * by 25, minus 1000. */
        return n * 25 - 1000;
    }
    else { return 0; }
}

/* Decode a raw Mode S message demodulated as a stream of bytes by
 * _detectModeS(), and split it into fields populating a modesMessage
 * structure. */
void ADSB1090Handler::_decodeModesMessage(Message* mm, uint8_t* msg)
{
    uint32_t    crc2; /* Computed CRC, used to verify the message CRC. */
    char const* ais_charset = "?ABCDEFGHIJKLMNOPQRSTUVWXYZ????? ???????????????0123456789??????";

    /* Work on our local copy */
    memcpy(mm->msg, msg, Message::LongMessageBits);
    msg = mm->msg;

    /* Get the message type ASAP as other operations depend on this */
    mm->msgtype = msg[0] >> 3; /* Downlink Format */
    mm->msgbits = modesMessageLenByType(mm->msgtype);

    /* CRC is always the last three bytes. */
    mm->crc
        = (uint32_t{msg[(mm->msgbits / 8) - 3]} << 16) | (uint32_t{msg[(mm->msgbits / 8) - 2]} << 8) | uint32_t{msg[(mm->msgbits / 8) - 1]};
    crc2 = modesChecksum(msg, mm->msgbits);

    /* Check CRC and fix single bit errors using the CRC when
     * possible (DF 11 and 17). */
    mm->errorbit = -1; /* No error */
    mm->crcok    = (mm->crc == crc2);

    if (!mm->crcok && _config.fixErrors && (mm->msgtype == 11 || mm->msgtype == 17))
    {
        if ((mm->errorbit = fixSingleBitErrors(msg, mm->msgbits)) != -1)
        {
            mm->crc   = modesChecksum(msg, mm->msgbits);
            mm->crcok = 1;
        }
        else if (_config.aggressive && mm->msgtype == 17 && (mm->errorbit = fixTwoBitsErrors(msg, mm->msgbits)) != -1)
        {
            mm->crc   = modesChecksum(msg, mm->msgbits);
            mm->crcok = 1;
        }
    }

    /* Note that most of the other computation happens *after* we fix
     * the single bit errors, otherwise we would need to recompute the
     * fields again. */
    mm->ca = msg[0] & 7; /* Responder capabilities. */

    /* ICAO address */
    mm->aa1 = msg[1];
    mm->aa2 = msg[2];
    mm->aa3 = msg[3];

    /* DF 17 type (assuming this is a DF17, otherwise not used) */
    mm->metype = msg[4] >> 3; /* Extended squitter message type. */
    mm->mesub  = msg[4] & 7;  /* Extended squitter message subtype. */

    /* Fields for DF4,5,20,21 */
    mm->fs = msg[0] & 7;           /* Flight status for DF4,5,20,21 */
    mm->dr = msg[1] >> 3 & 31;     /* Request extraction of downlink request. */
    mm->um = ((msg[1] & 7) << 3) | /* Request extraction of downlink request. */
             msg[2] >> 5;

    /* In the squawk (identity) field bits are interleaved like that
     * (message bit 20 to bit 32):
     *
     * C1-A1-C2-A2-C4-A4-ZERO-B1-D1-B2-D2-B4-D4
     *
     * So every group of three bits A, B, C, D represent an integer
     * from 0 to 7.
     *
     * The actual meaning is just 4 octal numbers, but we convert it
     * into a base ten number tha happens to represent the four
     * octal numbers.
     *
     * For more info: http://en.wikipedia.org/wiki/Gillham_code */
    {
        int a, b, c, d;

        a            = ((msg[3] & 0x80) >> 5) | ((msg[2] & 0x02) >> 0) | ((msg[2] & 0x08) >> 3);
        b            = ((msg[3] & 0x02) << 1) | ((msg[3] & 0x08) >> 2) | ((msg[3] & 0x20) >> 5);
        c            = ((msg[2] & 0x01) << 2) | ((msg[2] & 0x04) >> 1) | ((msg[2] & 0x10) >> 4);
        d            = ((msg[3] & 0x01) << 2) | ((msg[3] & 0x04) >> 1) | ((msg[3] & 0x10) >> 4);
        mm->identity = a * 1000 + b * 100 + c * 10 + d;
    }

    /* DF 11 & 17: try to populate our ICAO addresses whitelist.
     * DFs with an AP field (xored addr and crc), try to decode it. */
    if (mm->msgtype != 11 && mm->msgtype != 17)
    {
        /* Check if we can check the checksum for the Downlink Formats where
         * the checksum is xored with the AirCraftImpl ICAO address. We try to
         * brute force it using a list of recently seen AirCraftImpl addresses. */
        if (_bruteForceAP(msg, mm))
        {
            /* We recovered the message, mark the checksum as valid. */
            mm->crcok = 1;
        }
        else { mm->crcok = 0; }
    }
    else
    {
        /* If this is DF 11 or DF 17 and the checksum was ok,
         * we can add this address to the list of recently seen
         * addresses. */
        if (mm->crcok && mm->errorbit == -1)
        {
            uint32_t addr = (static_cast<uint32_t>(mm->aa1) << 16) | (static_cast<uint32_t>(mm->aa2) << 8) | static_cast<uint32_t>(mm->aa3);
            _addRecentlySeenICAOAddr(addr);
        }
    }

    /* Decode 13 bit altitude for DF0, DF4, DF16, DF20 */
    if (mm->msgtype == 0 || mm->msgtype == 4 || mm->msgtype == 16 || mm->msgtype == 20) { mm->altitude = decodeAC13Field(msg, &mm->unit); }

    /* Decode extended squitter specific stuff. */
    if (mm->msgtype == 17)
    {
        /* Decode the extended squitter message. */

        if (mm->metype >= 1 && mm->metype <= 4)
        {
            /* AirCraftImpl Identification and Category */
            mm->aircraft_type = mm->metype - 1;
            mm->flight[0]     = ais_charset[msg[5] >> 2];
            mm->flight[1]     = ais_charset[((msg[5] & 3) << 4) | (msg[6] >> 4)];
            mm->flight[2]     = ais_charset[((msg[6] & 15) << 2) | (msg[7] >> 6)];
            mm->flight[3]     = ais_charset[msg[7] & 63];
            mm->flight[4]     = ais_charset[msg[8] >> 2];
            mm->flight[5]     = ais_charset[((msg[8] & 3) << 4) | (msg[9] >> 4)];
            mm->flight[6]     = ais_charset[((msg[9] & 15) << 2) | (msg[10] >> 6)];
            mm->flight[7]     = ais_charset[msg[10] & 63];
            mm->flight[8]     = '\0';
        }
        else if (mm->metype >= 9 && mm->metype <= 18)
        {
            /* Airborne position Message */
            mm->fflag         = msg[6] & (1 << 2);
            mm->tflag         = msg[6] & (1 << 3);
            mm->altitude      = decodeAC12Field(msg, &mm->unit);
            mm->raw_latitude  = ((msg[6] & 3) << 15) | (msg[7] << 7) | (msg[8] >> 1);
            mm->raw_longitude = ((msg[8] & 1) << 16) | (msg[9] << 8) | msg[10];
        }
        else if (mm->metype == 19 && mm->mesub >= 1 && mm->mesub <= 4)
        {
            /* Airborne Velocity Message */
            if (mm->mesub == 1 || mm->mesub == 2)
            {
                mm->ew_dir           = (msg[5] & 4) >> 2;
                mm->ew_velocity      = ((msg[5] & 3) << 8) | msg[6];
                mm->ns_dir           = (msg[7] & 0x80) >> 7;
                mm->ns_velocity      = ((msg[7] & 0x7f) << 3) | ((msg[8] & 0xe0) >> 5);
                mm->vert_rate_source = (msg[8] & 0x10) >> 4;
                mm->vert_rate_sign   = (msg[8] & 0x8) >> 3;
                mm->vert_rate        = ((msg[8] & 7) << 6) | ((msg[9] & 0xfc) >> 2);
                /* Compute velocity and angle from the two speed
                 * components. */
                mm->velocity = static_cast<int>(sqrt(mm->ns_velocity * mm->ns_velocity + mm->ew_velocity * mm->ew_velocity));
                if (mm->velocity)
                {
                    int    ewv = mm->ew_velocity;
                    int    nsv = mm->ns_velocity;
                    double heading;

                    if (mm->ew_dir) ewv *= -1;
                    if (mm->ns_dir) nsv *= -1;
                    heading = atan2(ewv, nsv);

                    /* Convert to degrees. */
                    mm->heading = static_cast<int>(heading * 360 / (M_PI * 2));
                    /* We don't want negative values but a 0-360 scale. */
                    if (mm->heading < 0) mm->heading += 360;
                }
                else { mm->heading = 0; }
            }
            else if (mm->mesub == 3 || mm->mesub == 4)
            {
                mm->heading_is_valid = msg[5] & (1 << 2);
                mm->heading          = static_cast<int>((360.0 / 128) * (((msg[5] & 3) << 5) | (msg[6] >> 3)));
            }
        }
    }
    mm->phase_corrected = 0; /* Set to 1 by the caller if needed. */
}

/* Return -1 if the message is out of fase left-side
 * Return  1 if the message is out of fase right-size
 * Return  0 if the message is not particularly out of phase.
 *
 * Note: this function will access m[-1], so the caller should make sure to
 * call it only if we are not at the start of the current buffer. */
static int detectOutOfPhase(uint16_t* m)
{
    if (m[3] > m[2] / 3) return 1;
    if (m[10] > m[9] / 3) return 1;
    if (m[6] > m[7] / 3) return -1;
    if (m[-1] > m[1] / 3) return -1;
    return 0;
}

/* This function does not really correct the phase of the message, it just
 * applies a transformation to the first sample representing a given bit:
 *
 * If the previous bit was one, we amplify it a bit.
 * If the previous bit was zero, we decrease it a bit.
 *
 * This simple transformation makes the message a bit more likely to be
 * correctly decoded for out of phase messages:
 *
 * When messages are out of phase there is more uncertainty in
 * sequences of the same bit multiple times, since 11111 will be
 * transmitted as continuously altering magnitude (high, low, high, low...)
 *
 * However because the message is out of phase some part of the high
 * is mixed in the low part, so that it is hard to distinguish if it is
 * a zero or a one.
 *
 * However when the message is out of phase passing from 0 to 1 or from
 * 1 to 0 happens in a very recognizable way, for instance in the 0 -> 1
 * transition, magnitude goes low, high, high, low, and one of of the
 * two middle samples the high will be *very* high as part of the previous
 * or next high signal will be mixed there.
 *
 * Applying our simple transformation we make more likely if the current
 * bit is a zero, to detect another zero. Symmetrically if it is a one
 * it will be more likely to detect a one because of the transformation.
 * In this way similar levels will be interpreted more likely in the
 * correct way. */
static void applyPhaseCorrection(uint16_t* m)
{
    m += 16; /* Skip preamble. */
    for (size_t j = 0; j < (Message::LongMessageBits - 1) * 2; j += 2)
    {
        if (m[j] > m[j + 1])
        {
            /* One */
            m[j + 2] = static_cast<uint16_t>((m[j + 2] * 5) / 4);
        }
        else
        {
            /* Zero */
            m[j + 2] = static_cast<uint16_t>((m[j + 2] * 4) / 5);
        }
    }
}

/* Detect a Mode S messages inside the magnitude buffer pointed by 'm' and of
 * size 'mlen' bytes. Every detected Mode S message is convert it into a
 * stream of bits and passed to the function to display it. */
void ADSB1090Handler::_detectModeS(uint16_t* m, uint32_t mlen)
{
    uint8_t  bits[Message::LongMessageBits];
    uint8_t  msg[Message::LongMessageBits / 2];
    uint16_t aux[Message::LongMessageBits * 2];
    uint32_t j;
    int      use_correction = 0;

    /* The Mode S preamble is made of impulses of 0.5 microseconds at
     * the following time offsets:
     *
     * 0   - 0.5 usec: first impulse.
     * 1.0 - 1.5 usec: second impulse.
     * 3.5 - 4   usec: third impulse.
     * 4.5 - 5   usec: last impulse.
     *
     * Since we are sampling at 2 Mhz every sample in our magnitude vector
     * is 0.5 usec, so the preamble will look like this, assuming there is
     * an impulse at offset 0 in the array:
     *
     * 0   -----------------
     * 1   -
     * 2   ------------------
     * 3   --
     * 4   -
     * 5   --
     * 6   -
     * 7   ------------------
     * 8   --
     * 9   -------------------
     */
    for (j = 0; j < mlen - MODES_FULL_LEN * 2; j++)
    {
        int low, high, delta, errors;
        int good_message = 0;

        if (use_correction) goto good_preamble; /* We already checked it. */

        /* First check of relations between the first 10 samples
         * representing a valid preamble. We don't even investigate further
         * if this simple test is not passed. */
        if (!(m[j] > m[j + 1] && m[j + 1] < m[j + 2] && m[j + 2] > m[j + 3] && m[j + 3] < m[j] && m[j + 4] < m[j] && m[j + 5] < m[j]
              && m[j + 6] < m[j] && m[j + 7] > m[j + 8] && m[j + 8] < m[j + 9] && m[j + 9] > m[j + 6]))
        {
            // if (Modes.debug & MODES_DEBUG_NOPREAMBLE && m[j] > MODES_DEBUG_NOPREAMBLE_LEVEL)
            // dumpRawMessage("Unexpected ratio among first 10 samples", msg, m, j);
            continue;
        }

        /* The samples between the two spikes must be < than the average
         * of the high spikes level. We don't test bits too near to
         * the high levels as signals can be out of phase so part of the
         * energy can be in the near samples. */
        high = (m[j] + m[j + 2] + m[j + 7] + m[j + 9]) / 6;
        if (m[j + 4] >= high || m[j + 5] >= high)
        {
            // if (Modes.debug & MODES_DEBUG_NOPREAMBLE && m[j] > MODES_DEBUG_NOPREAMBLE_LEVEL)
            //    dumpRawMessage("Too high level in samples between 3 and 6", msg, m, j);
            continue;
        }

        /* Similarly samples in the range 11-14 must be low, as it is the
         * space between the preamble and real data. Again we don't test
         * bits too near to high levels, see above. */
        if (m[j + 11] >= high || m[j + 12] >= high || m[j + 13] >= high || m[j + 14] >= high)
        {
            // if (Modes.debug & MODES_DEBUG_NOPREAMBLE && m[j] > MODES_DEBUG_NOPREAMBLE_LEVEL)
            //    dumpRawMessage("Too high level in samples between 10 and 15", msg, m, j);
            continue;
        }
        _stat_valid_preamble++;

    good_preamble:
        /* If the previous attempt with this message failed, retry using
         * magnitude correction. */
        if (use_correction)
        {
            memcpy(aux, m + j + MODES_PREAMBLE_US * 2, sizeof(aux));
            if (j && detectOutOfPhase(m + j))
            {
                applyPhaseCorrection(m + j);
                _stat_out_of_phase++;
            }
            /* TODO ... apply other kind of corrections. */
        }

        /* Decode all the next 112 bits, regardless of the actual message
         * size. We'll check the actual message type later. */
        errors = 0;
        for (uint32_t i = 0; i < Message::LongMessageBits * 2; i += 2)
        {
            low   = m[j + i + MODES_PREAMBLE_US * 2];
            high  = m[j + i + MODES_PREAMBLE_US * 2 + 1];
            delta = low - high;
            if (delta < 0) delta = -delta;

            if (i > 0 && delta < 256) { bits[i / 2] = bits[i / 2 - 1]; }
            else if (low == high)
            {
                /* Checking if two adiacent samples have the same magnitude
                 * is an effective way to detect if it's just random noise
                 * that was detected as a valid preamble. */
                bits[i / 2] = 2; /* error */
                if (i < Message::ShortMessageBits * 2) errors++;
            }
            else if (low > high) { bits[i / 2] = 1; }
            else
            {
                /* (low < high) for exclusion  */
                bits[i / 2] = 0;
            }
        }

        /* Restore the original message if we used magnitude correction. */
        if (use_correction) memcpy(m + j + MODES_PREAMBLE_US * 2, aux, sizeof(aux));

        /* Pack bits into bytes */
        for (size_t i = 0; i < Message::LongMessageBits; i += 8)
        {
            msg[i / 8] = static_cast<uint8_t>(bits[i] << 7 | bits[i + 1] << 6 | bits[i + 2] << 5 | bits[i + 3] << 4 | bits[i + 4] << 3
                                              | bits[i + 5] << 2 | bits[i + 6] << 1 | bits[i + 7]);
        }

        int      msgtype = msg[0] >> 3;
        uint32_t msglen  = static_cast<uint32_t>(modesMessageLenByType(msgtype)) / 8;

        /* Last check, high and low bits are different enough in magnitude
         * to mark this as real message and not just noise? */
        delta = 0;
        for (size_t i = 0; i < msglen * 8 * 2; i += 2)
        {
            delta += abs(m[j + i + MODES_PREAMBLE_US * 2] - m[j + i + MODES_PREAMBLE_US * 2 + 1]);
        }
        delta /= msglen * 4;

        /* Filter for an average delta of three is small enough to let almost
         * every kind of message to pass, but high enough to filter some
         * random noise. */
        if (delta < 10 * 255)
        {
            use_correction = 0;
            continue;
        }

        /* If we reached this point, and error is zero, we are very likely
         * with a Mode S message in our hands, but it may still be broken
         * and CRC may not be correct. This is handled by the next layer. */
        if (errors == 0 || (_config.aggressive && errors < 3))
        {
            Message mm;

            /* Decode the received message and update statistics */
            _decodeModesMessage(&mm, msg);

            /* Update statistics. */
            if (mm.crcok || use_correction)
            {
                if (errors == 0) _stat_demodulated++;
                if (mm.errorbit == -1)
                {
                    if (mm.crcok)
                        _stat_goodcrc++;
                    else
                        _stat_badcrc++;
                }
                else
                {
                    _stat_badcrc++;
                    _stat_fixed++;
                    if (mm.errorbit < static_cast<int>(Message::LongMessageBits))
                        _stat_single_bit_fix++;
                    else
                        _stat_two_bits_fix++;
                }
            }
#if 0
            /* Output debug mode info if needed. */
            if (use_correction == 0)
            {
                if (Modes.debug & MODES_DEBUG_DEMOD)
                    dumpRawMessage("Demodulated with 0 errors", msg, m, j);
                else if (Modes.debug & MODES_DEBUG_BADCRC && mm.msgtype == 17 && (!mm.crcok || mm.errorbit != -1))
                    dumpRawMessage("Decoded with bad CRC", msg, m, j);
                else if (Modes.debug & MODES_DEBUG_GOODCRC && mm.crcok && mm.errorbit == -1)
                    dumpRawMessage("Decoded with good CRC", msg, m, j);
            }
#endif

            /* Skip this message if we are sure it's fine. */
            if (mm.crcok)
            {
                j += (MODES_PREAMBLE_US + (msglen * 8)) * 2;
                good_message = 1;
                if (use_correction) mm.phase_corrected = 1;
            }

            /* Pass data to the next layer */
            _useModesMessage(&mm);
        }
        else
        {
            if (_config.debug && use_correction)
            {
                // printf("The following message has %d demod errors\n", errors);
                // dumpRawMessage("Demodulated with errors", msg, m, j);
            }
        }

        /* Retry with phase correction if possible. */
        if (!good_message && !use_correction)
        {
            j--;
            use_correction = 1;
        }
        else { use_correction = 0; }
    }
}

/* When a new message is available, because it was decoded from the
 * RTL device, file, or received in the TCP input port, or any other
 * way we can receive a decoded message, we call this function in order
 * to use the message.
 *
 * Basically this function passes a raw message to the upper layers for
 * further processing and visualization. */
void ADSB1090Handler::_useModesMessage(Message* mm)
{
    if (_config.checkCRC && mm->crcok == 0) { return; }
    _interactiveReceiveData(mm);
    // uint32_t addr = (static_cast<uint32_t>(mm->aa1) << 16) | (static_cast<uint32_t>(mm->aa2) << 8) | static_cast<uint32_t>(mm->aa3);
    //_modesSendSBSOutput(mm, _interactiveFindOrCreateAircraft(addr)); /* Feed SBS output clients. */
}

/* ========================= Interactive mode =============================== */

/* Return a new AirCraftImpl structure for the interactive mode linked list
 * of aircrafts. */
AirCraftImpl& ADSB1090Handler::_interactiveFindOrCreateAircraft(uint32_t addr)
{
    return _trafficManager->FindOrCreate(addr);
}

/* Always positive MOD operation, used for CPR decoding. */
static int cprModFunction(int a, int b)
{
    int res = a % b;
    if (res < 0) res += b;
    return res;
}
/* The NL function uses the precomputed table from 1090-WP-9-14 */
static int cprNLFunction(double lat)
{
    if (lat < 0) lat = -lat; /* Table is simmetric about the equator. */
    if (lat < 10.47047130) return 59;
    if (lat < 14.82817437) return 58;
    if (lat < 18.18626357) return 57;
    if (lat < 21.02939493) return 56;
    if (lat < 23.54504487) return 55;
    if (lat < 25.82924707) return 54;
    if (lat < 27.93898710) return 53;
    if (lat < 29.91135686) return 52;
    if (lat < 31.77209708) return 51;
    if (lat < 33.53993436) return 50;
    if (lat < 35.22899598) return 49;
    if (lat < 36.85025108) return 48;
    if (lat < 38.41241892) return 47;
    if (lat < 39.92256684) return 46;
    if (lat < 41.38651832) return 45;
    if (lat < 42.80914012) return 44;
    if (lat < 44.19454951) return 43;
    if (lat < 45.54626723) return 42;
    if (lat < 46.86733252) return 41;
    if (lat < 48.16039128) return 40;
    if (lat < 49.42776439) return 39;
    if (lat < 50.67150166) return 38;
    if (lat < 51.89342469) return 37;
    if (lat < 53.09516153) return 36;
    if (lat < 54.27817472) return 35;
    if (lat < 55.44378444) return 34;
    if (lat < 56.59318756) return 33;
    if (lat < 57.72747354) return 32;
    if (lat < 58.84763776) return 31;
    if (lat < 59.95459277) return 30;
    if (lat < 61.04917774) return 29;
    if (lat < 62.13216659) return 28;
    if (lat < 63.20427479) return 27;
    if (lat < 64.26616523) return 26;
    if (lat < 65.31845310) return 25;
    if (lat < 66.36171008) return 24;
    if (lat < 67.39646774) return 23;
    if (lat < 68.42322022) return 22;
    if (lat < 69.44242631) return 21;
    if (lat < 70.45451075) return 20;
    if (lat < 71.45986473) return 19;
    if (lat < 72.45884545) return 18;
    if (lat < 73.45177442) return 17;
    if (lat < 74.43893416) return 16;
    if (lat < 75.42056257) return 15;
    if (lat < 76.39684391) return 14;
    if (lat < 77.36789461) return 13;
    if (lat < 78.33374083) return 12;
    if (lat < 79.29428225) return 11;
    if (lat < 80.24923213) return 10;
    if (lat < 81.19801349) return 9;
    if (lat < 82.13956981) return 8;
    if (lat < 83.07199445) return 7;
    if (lat < 83.99173563) return 6;
    if (lat < 84.89166191) return 5;
    if (lat < 85.75541621) return 4;
    if (lat < 86.53536998) return 3;
    if (lat < 87.00000000)
        return 2;
    else
        return 1;
}
static int cprNFunction(double lat, int isodd)
{
    int nl = cprNLFunction(lat) - isodd;
    if (nl < 1) nl = 1;
    return nl;
}

static double cprDlonFunction(double lat, int isodd)
{
    return 360.0 / cprNFunction(lat, isodd);
}

/* This algorithm comes from:
 * http://www.lll.lu/~edward/edward/adsb/DecodingADSBposition.html.
 *
 *
 * A few remarks:
 * 1) 131072 is 2^17 since CPR latitude and longitude are encoded in 17 bits.
 * 2) We assume that we always received the odd packet as last packet for
 *    simplicity. This may provide a position that is less fresh of a few
 *    seconds.
 */

static void decodeCPR(AirCraftImpl& a)
{
    double       AirDlat0 = 360.0 / 60;
    const double AirDlat1 = 360.0 / 59;
    double       lat0     = a.cpr_even_lat;
    double       lat1     = a.cpr_odd_lat;
    double       lon0     = a.cpr_even_lon;
    double       lon1     = a.cpr_odd_lon;

    /* Compute the Latitude Index "j" */
    int    j     = static_cast<int>(floor(((59 * lat0 - 60 * lat1) / 131072) + 0.5));
    double rlat0 = AirDlat0 * (cprModFunction(j, 60) + lat0 / 131072);
    double rlat1 = AirDlat1 * (cprModFunction(j, 59) + lat1 / 131072);

    if (rlat0 >= 270) rlat0 -= 360;
    if (rlat1 >= 270) rlat1 -= 360;

    /* Check that both are in the same latitude zone, or abort. */
    if (cprNLFunction(rlat0) != cprNLFunction(rlat1)) return;

    double lat1E7, lon1E7;
    /* Compute ni and the longitude index m */
    if (a.cpr_even_time > a.cpr_odd_time)
    {
        /* Use even packet. */
        int ni = cprNFunction(rlat0, 0);
        int m  = static_cast<int>(floor((((lon0 * (cprNLFunction(rlat0) - 1)) - (lon1 * cprNLFunction(rlat0))) / 131072) + 0.5));
        lon1E7 = (cprDlonFunction(rlat0, 0) * (cprModFunction(m, ni) + lon0 / 131072) * 10000000);
        lat1E7 = (double{rlat0 * 10000000});
    }
    else
    {
        /* Use odd packet. */
        int ni = cprNFunction(rlat1, 1);
        int m  = static_cast<int>(floor((((lon0 * (cprNLFunction(rlat1) - 1)) - (lon1 * cprNLFunction(rlat1))) / 131072.0) + 0.5));
        lon1E7 = (cprDlonFunction(rlat1, 1) * (cprModFunction(m, ni) + lon1 / 131072) * 10000000);
        lat1E7 = (double{rlat1} * 10000000);
    }
    if (lon1E7 > 180 * 10000000) { lon1E7 -= 3600000000; }
    a.lat1E7 = static_cast<int32_t>(lat1E7);
    a.lon1E7 = static_cast<int32_t>(lon1E7);
}

/* Receive new messages and populate the interactive mode with more info. */
AirCraftImpl& ADSB1090Handler::_interactiveReceiveData(Message* mm)
{
    uint32_t addr = static_cast<uint32_t>((mm->aa1 << 16) | (mm->aa2 << 8) | mm->aa3);

    auto  now = std::chrono::system_clock::now();
    auto& a   = _interactiveFindOrCreateAircraft(addr);
#if 0
    /* Loookup our AirCraftImpl or create a new one. */
    auto& subctx = [&](uint32_t addr) {
        auto& allaircrafts = ctx.Obj().aircrafts();
        for (size_t i = 0; i < allaircrafts.size(); i++)
        {
            if (allaircrafts[i].addr() == addr)
            {
                return ctx.TXN().edit_aircrafts(i);
            }
        }

        AirCraftImpl newaircraft{};
        newaircraft.set_addr(uint32_t{addr});
        snprintf(newaircraft.tailNumber().data(), newaircraft.tailNumber().size(), "%06x", static_cast<int>(addr));
        ctx.TXN().add_aircrafts(std::move(newaircraft));
        return ctx.TXN().edit_aircrafts(allaircrafts.size());
    }(addr);

    auto  current = subctx.Obj().activeIndex();
    auto  next    = (current + 1) % subctx.Obj().loc().size();
    auto& locctx  = subctx.edit_loc(next);
    auto& curloc  = subctx.Obj().loc()[current];
    locctx.set_seen(now);
    // a.set_messageCount(int32_t{a.messageCount() + 1});
#endif
    a.sourceId = _sourceId;
    if (mm->msgtype == 0 || mm->msgtype == 4 || mm->msgtype == 20)
    {
        a.altitude = (static_cast<int32_t>(mm->altitude));

        // locctx.set_altitude(int32_t{mm->altitude});
    }
    else if (mm->msgtype == 17)
    {
        if (mm->metype >= 1 && mm->metype <= 4)
        {
            std::copy(std::begin(mm->flight), std::end(mm->flight), std::begin(a.callsign));
            //    memcpy(a.flight().data(), mm->flight, a.flight().size());
        }
        else if (mm->metype >= 9 && mm->metype <= 18)
        {
            a.altitude = (static_cast<int32_t>(mm->altitude));
            if (mm->fflag)
            {
                a.cpr_odd_lat  = (int32_t{mm->raw_latitude});
                a.cpr_odd_lon  = (int32_t{mm->raw_longitude});
                a.cpr_odd_time = (decltype(now){now});
            }
            else
            {
                a.cpr_even_lat  = (int32_t{mm->raw_latitude});
                a.cpr_even_lon  = (int32_t{mm->raw_longitude});
                a.cpr_even_time = (decltype(now){now});
            }
            /* If the two data is less than 10 seconds apart, compute
             * the position. */
            if (std::abs(std::chrono::duration_cast<std::chrono::seconds>(a.cpr_even_time - a.cpr_odd_time).count()) <= 10)
            {
                decodeCPR(a);
            }
        }
        else if (mm->metype == 19)
        {
            if (mm->mesub == 1 || mm->mesub == 2)
            {
                a.speed = static_cast<uint32_t>(mm->velocity);
                a.track = static_cast<uint32_t>(mm->heading);
            }
        }
    }
#if 0
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
    static std::unordered_map<uint32_t, time_point> times;
#pragma clang diagnostic pop
    auto it = times.find(a.addr());

    if (it == times.end() || (now - it->second) > 10s)
    {
        times[a.addr()] = now;
        std::cout << "Addr:" << a.addr() << "\tLat:" << a.lat() << "\tLon:" << a.lon() << "\tSpeed:" << a.groundSpeed()
                  << "\tTrack:" << a.track() << "\tAlt:" << a.altitude() << std::endl;
    }

    // Notify(lock, ctx, a);

    //_recorder.Record(a, ctx);
    // for (auto& handler : handlers)
    //{
    //   handler->OnTrafficUpdate(lock, a);
    //}
#endif
    _trafficManager->NotifyChanged(a);
    return a;
}

#if 0
/* Write SBS output to TCP clients. */
void ADSB1090Handler::_modesSendSBSOutput(Message* mm, AirCraftImpl& a)
{
    char msg[256], *p = msg;
    int  emergency = 0, ground = 0, alert = 0, spi = 0;

    if (mm->msgtype == 4 || mm->msgtype == 5 || mm->msgtype == 21)
    {
        /* Node: identity is calculated/kept in base10 but is actually
         * octal (07500 is represented as 7500) */
        if (mm->identity == 7500 || mm->identity == 7600 || mm->identity == 7700) emergency = -1;
        if (mm->fs == 1 || mm->fs == 3) ground = -1;
        if (mm->fs == 2 || mm->fs == 3 || mm->fs == 4) alert = -1;
        if (mm->fs == 4 || mm->fs == 5) spi = -1;
    }
#pragma warning(push)
#pragma warning(disable : 4996)
    if (mm->msgtype == 0)
    {
        p += sprintf(p, "MSG,5,,,%02X%02X%02X,,,,,,,%d,,,,,,,,,,", mm->aa1, mm->aa2, mm->aa3, mm->altitude);
    }
    else if (mm->msgtype == 4)
    {
        p += sprintf(
            p, "MSG,5,,,%02X%02X%02X,,,,,,,%d,,,,,,,%d,%d,%d,%d", mm->aa1, mm->aa2, mm->aa3, mm->altitude, alert, emergency, spi, ground);
    }
    else if (mm->msgtype == 5)
    {
        p += sprintf(
            p, "MSG,6,,,%02X%02X%02X,,,,,,,,,,,,,%d,%d,%d,%d,%d", mm->aa1, mm->aa2, mm->aa3, mm->identity, alert, emergency, spi, ground);
    }
    else if (mm->msgtype == 11)
    {
        p += sprintf(p, "MSG,8,,,%02X%02X%02X,,,,,,,,,,,,,,,,,", mm->aa1, mm->aa2, mm->aa3);
    }
    else if (mm->msgtype == 17 && mm->metype == 4)
    {
        p += sprintf(p, "MSG,1,,,%02X%02X%02X,,,,,,%s,,,,,,,,0,0,0,0", mm->aa1, mm->aa2, mm->aa3, mm->flight);
    }
    else if (mm->msgtype == 17 && mm->metype >= 9 && mm->metype <= 18)
    {
        //        if (a.lat() == 0 && a.lon() == 0)
        //            p += sprintf(p, "MSG,3,,,%02X%02X%02X,,,,,,,%d,,,,,,,0,0,0,0", mm->aa1, mm->aa2, mm->aa3, mm->altitude);
        //       else
        p += sprintf(p,
                     "MSG,3,,,%02X%02X%02X,,,,,,,%d,,,%1.5f,%1.5f,,,"
                     "0,0,0,0",
                     mm->aa1,
                     mm->aa2,
                     mm->aa3,
                     mm->altitude,
                     a.lat,
                     a.lon);
    }
    else if (mm->msgtype == 17 && mm->metype == 19 && mm->mesub == 1)
    {
        int vr = (mm->vert_rate_sign == 0 ? 1 : -1) * (mm->vert_rate - 1) * 64;

        p += sprintf(p, "MSG,4,,,%02X%02X%02X,,,,,,,,%d,%d,,,%i,,0,0,0,0", mm->aa1, mm->aa2, mm->aa3, a.Speed(), a.Heading(), vr);
    }
    else if (mm->msgtype == 21)
    {
        p += sprintf(
            p, "MSG,6,,,%02X%02X%02X,,,,,,,,,,,,,%d,%d,%d,%d,%d", mm->aa1, mm->aa2, mm->aa3, mm->identity, alert, emergency, spi, ground);
    }
#pragma warning(pop)
    else
    {
        return;
    }

    *p++ = '\n';
    std::cout << msg << std::endl;
    // modesSendAllClients(Modes.sbsos, msg, p - msg);
}
#endif
