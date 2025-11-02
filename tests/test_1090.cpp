#include "ADSB1090.h"
#include "AircraftImpl.h"
#include "RTLSDR.h"

#include "TestUtils.h"
#include <fmt/base.h>
#include <fmt/std.h>

#include <filesystem>
#include <memory>
#include <string>

DECLARE_RESOURCE_COLLECTION(traces);

template <> struct fmt::formatter<ADSB::IAirCraft> : fmt::formatter<std::string_view>
{
    // Formats the point p using the parsed format specification (presentation)
    // stored in this formatter.
    auto format(ADSB::IAirCraft const& obj, fmt::format_context& ctx) const    // NOLINT
    {
        return fmt::format_to(ctx.out(),
                              "{:x}[{: >8}]: Pos={:+03.2f}:{:+03.2f}^{:05} Speed={:03} Count={}",
                              obj.Addr(),
                              obj.FlightNumber(),
                              (obj.Lat1E7() / ADSB::IAirCraft::LatLonPrecision),
                              (obj.Lon1E7() / ADSB::IAirCraft::LatLonPrecision),
                              obj.Altitude(),
                              obj.Speed(),
                              obj.SquakCode(),
                              obj.MessageCount()

        );
    }
};
struct Listener : ADSB::IListener
{
    void OnChanged(ADSB::IAirCraft const& a) override
    {
        auto msg = fmt::format("{}", a);
        messages.push_back(msg);
        status[a.Addr()] = msg;
    }
    std::vector<std::string>                  messages;
    std::unordered_map<uint32_t, std::string> status;
};
struct Selector : RTLSDR::IDeviceSelector
{
    [[nodiscard]] bool SelectDevice(RTLSDR::DeviceInfo const& /* d */) const override { return false; }
};

TEST_CASE("TestEmbedded", "[1090]")
{
    for (auto const res : LOAD_RESOURCE_COLLECTION(traces))
    {
        // auto fname = std::filesystem::path(res.name());
        //  REQUIRE(!fname.empty());

        Listener listener;
        auto     mgr = std::make_shared<ADSB::TrafficManager>();
        mgr->SetListener(&listener);
        Selector selector;
        auto     handler = ADSB::test::TryCreateADSB1090Handler(mgr, &selector, 1);
        handler->HandleData(res.data<uint8_t>());
    }
    // REQUIRE(!pidlfiles.empty());
    // REQUIRE_NOTHROW(RunTest(pidlfiles));
}

template <typename TLambda> static void BufferedFileRead(std::filesystem::path const& fpath, size_t replayCount, TLambda const& callback)
{
    static constexpr size_t           BufferCount  = RTLSDR::BufferCount;
    static constexpr size_t           BufferLength = RTLSDR::BufferLength;
    std::vector<std::vector<uint8_t>> buffers;
    buffers.reserve(BufferCount);
    for (size_t i = 0; i < BufferCount; i++) { buffers.emplace_back(BufferLength); }
    std::ifstream ifs(fpath, std::ios::binary | std::ios::in);
    if (!ifs.good()) { throw std::runtime_error("Cannot open test data file"); }
    size_t bufferToUse = 0;

    while (ifs.good() && replayCount > 0)
    {
        ifs.read(reinterpret_cast<char*>(buffers[bufferToUse].data()), static_cast<std::streamsize>(BufferLength));    // NOLINT
        if (!ifs.good())
        {
            ifs.close();
            replayCount--;
            if (replayCount == 0) { return; }
            ifs = std::ifstream(fpath, std::ios::binary | std::ios::in);
            continue;
        }
        callback(buffers[bufferToUse]);
        bufferToUse = (bufferToUse + 1) % BufferCount;
    }
    ifs.close();
}

void RunTestWithFile(std::filesystem::path const& fpath)
{
    using CreatorFn    = decltype(ADSB::test::TryCreateUAT978Handler);
    CreatorFn* creator = nullptr;

    if (fpath.string().find("1090") != std::string::npos) { creator = ADSB::test::TryCreateADSB1090Handler; }
    else if (fpath.string().find("978") != std::string::npos) { creator = ADSB::test::TryCreateUAT978Handler; }
    else
    {}

    if (creator == nullptr) { return; }

    fmt::print("Testing with {}\n", fpath);
    Listener listener;
    auto     mgr = std::make_shared<ADSB::TrafficManager>();
    mgr->SetListener(&listener);
    Selector selector;
    auto     handler = creator(mgr, &selector, 1);
    BufferedFileRead(fpath, 1, [&](auto const& buf) { handler->HandleData(buf); });
    fmt::print("{}\n\n", fmt::join(listener.messages, "\n"));
}

TEST_CASE("TestEnv", "[1090]")
{
    auto* dirpathstr = getenv("RTLSDR_TEST_TRACE_DIR");
    if (dirpathstr != nullptr)
    {
        auto dirpath = std::filesystem::path(dirpathstr);
        REQUIRE(std::filesystem::exists(dirpath));
        REQUIRE(std::filesystem::is_directory(dirpath));
        for (auto const& direntry : std::filesystem::directory_iterator(dirpath))
        {
            if (!direntry.is_regular_file()) { continue; }
            RunTestWithFile(direntry.path());
        }
    }
    auto* fpathstr = getenv("RTLSDR_TEST_TRACE_FILE");
    if (fpathstr != nullptr)
    {
        auto fpath = std::filesystem::path(fpathstr);
        REQUIRE(std::filesystem::exists(fpath));
        REQUIRE(!std::filesystem::is_directory(fpath));
        RunTestWithFile(fpath);
    }
}
