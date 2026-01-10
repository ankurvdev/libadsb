#pragma once
#include "CommonMacros.h"

SUPPRESS_WARNINGS_START
SUPPRESS_STL_WARNINGS
#include "SetThreadName.h"
#include <rtl-sdr.h>

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <span>
#include <thread>
#include <unordered_set>
#include <vector>
SUPPRESS_WARNINGS_END

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct RTLSDR
{
    using time_point = std::chrono::time_point<std::chrono::system_clock>;

    struct IDataHandler
    {
        IDataHandler()                                                = default;
        virtual ~IDataHandler()                                       = default;
        virtual void HandleData(std::span<uint8_t const> const& data) = 0;
        CLASS_DEFAULT_COPY_AND_MOVE(IDataHandler);
    };

    static constexpr int      AutoGain        = -100;
    static constexpr int      MaxGain         = 999999;
    static constexpr int      CorrectionPPM   = 0;
    static constexpr uint32_t SampleRate      = 0;    // Invalid
    static constexpr size_t   BufferLength    = 32u * 512u;
    static constexpr size_t   BufferCount     = 16;
    static constexpr uint32_t CenterFrequency = 0;

    struct Config
    {
        int      gain         = MaxGain;
        bool     enableAGC    = false;
        uint32_t bufferLength = BufferLength;
        uint32_t bufferCount  = BufferCount;
        uint32_t frequency    = CenterFrequency;
        uint32_t sampleRate   = SampleRate;
    };

    struct DeviceInfo
    {
        uint32_t index;
        char     vendor[256];     // NOLINT
        char     product[256];    // NOLINT
        char     serial[256];     // NOLINT
    };

    struct IDeviceSelector
    {
        IDeviceSelector()                                                  = default;
        virtual ~IDeviceSelector()                                         = default;
        [[nodiscard]] virtual bool SelectDevice(DeviceInfo const& d) const = 0;
        CLASS_DEFAULT_COPY_AND_MOVE(IDeviceSelector);
    };

    private:
    struct ManagerContext
    {
        ManagerContext()  = default;
        ~ManagerContext() = default;
        CLASS_DELETE_COPY_AND_MOVE(ManagerContext);

        std::atomic<bool> running{false};
        rtlsdr_dev_t*     dev{nullptr};
    };

    // RTLSDR is hugely single threaded
    // Cannot open devices once a device is opened
    // A manager is required for orchestration
    struct Manager
    {
        SUPPRESS_WARNINGS_START
        SUPPRESS_CLANG_WARNING("-Wexit-time-destructors")
        SUPPRESS_CLANG_WARNING("-Wglobal-constructors")
        SUPPRESS_CLANG_WARNING("-Wunique-object-duplication")
        static inline std::mutex             MgrMutex;
        static inline std::weak_ptr<Manager> Instance;
        SUPPRESS_WARNINGS_END
        static std::shared_ptr<Manager> GetInstance()
        {
            std::scoped_lock lockGuard(MgrMutex);
            if (Instance.expired())
            {
                auto ptr = std::make_shared<Manager>();
                Instance = ptr;
                return ptr;
            }
            return Instance.lock();
        }

        Manager() = default;
        CLASS_DELETE_COPY_AND_MOVE(Manager);

        // Blocking call
        void Start(RTLSDR* client)
        {
            SetThreadName("RTLSDR::ReadAsync");
            {
                std::unique_lock<std::mutex> guard(MgrMutex);
                clients.insert(client);
            }

            while (true)
            {
                SetThreadName("RTLSDR::ReadAsync");

                rtlsdr_dev_t* dev = nullptr;
                {
                    std::unique_lock<std::mutex> guard(MgrMutex);
                    if (!clients.contains(client)) { return; }
                    RequestDevice(guard);
                }
                do
                {
                    {
                        std::unique_lock<std::mutex> guard(MgrMutex);
                        if (!clients.contains(client)) { return; }
                        dev = client->_mgrctx.dev;
                    }
                    if (dev == nullptr) { std::this_thread::sleep_for(std::chrono::milliseconds{500}); }
                    else
                    {
                        client->_mgrctx.running = true;
                    }
                } while (dev == nullptr);

                rtlsdr_read_async(dev,
                                  RTLSDR::_Callback,
                                  client,
                                  client->_config.bufferCount,
                                  client->_config.bufferCount * client->_config.bufferLength);
                client->_mgrctx.running = false;
                std::unique_lock<std::mutex> guard(MgrMutex);
                if (client->_mgrctx.dev != nullptr && dev == client->_mgrctx.dev)
                {
                    rtlsdr_close(dev);
                    client->_mgrctx.dev = nullptr;
                }
            }
        }

        static void Stop(std::unique_lock<std::mutex>& /*guard*/, RTLSDR* client)
        {
            auto* dev = client->_mgrctx.dev;
            if (dev != nullptr)
            {
                while (client->_mgrctx.running && rtlsdr_cancel_async(dev) != 0) {}
                while (client->_mgrctx.running) { std::this_thread::sleep_for(std::chrono::milliseconds{1}); }
                rtlsdr_close(dev);
                client->_mgrctx.dev = nullptr;
            }
        }

        void Stop(RTLSDR* client)
        {
            std::unique_lock<std::mutex> guard(MgrMutex);
            Stop(guard, client);
            clients.erase(client);
        }

        void RequestDevice(std::unique_lock<std::mutex> const& /*guard*/)
        {
            if (clients.empty()) { return; }
            if (deviceSearching) { return; }
            deviceSearching    = true;
            deviceSearchThread = std::async([this]() { this->RequestDeviceImpl(); });
        }

        // Scenarios
        // 1. Not enough devices.
        // 2. Extra Devices
        // 3. Disconnection
        // 4. SpinDown
        // 5. OnDeviceAvailable

        void RequestDeviceImpl()
        {
            SetThreadName("RTLSDR::ReqDev");
            do
            {
                try
                {
                    // Let things stabilize a little
                    std::this_thread::sleep_for(std::chrono::milliseconds{100});
                    {
                        std::unique_lock<std::mutex> guard(MgrMutex);
                        deviceSearching = false;
                        if (clients.empty()) { return; }
                        for (auto* client : clients) { Stop(guard, client); }
                    }
                    auto deviceCountLambda = [&]() {
                        std::unique_lock<std::mutex> guard(MgrMutex);
                        return rtlsdr_get_device_count();
                    };

                    auto deviceCount = deviceCountLambda();
                    while (deviceCount == 0)
                    {

                        std::this_thread::sleep_for(std::chrono::milliseconds{500});
                        deviceCount = deviceCountLambda();
                    }

                    {
                        std::unique_lock<std::mutex> guard(MgrMutex);
                        // Sometimes only one device shows up. Wait a bit and retry
                        std::this_thread::sleep_for(std::chrono::milliseconds{500});
                        deviceCount = rtlsdr_get_device_count();
                        if (deviceCount == 0) { continue; }
                        for (uint32_t i = 0; i < deviceCount; i++)
                        {

                            DeviceInfo d{};
                            d.index = i;
                            rtlsdr_get_device_usb_strings(d.index, d.vendor, d.product, d.serial);
                            rtlsdr_dev_t* dev = nullptr;
                            if (rtlsdr_open(&dev, d.index) != 0)
                            {
                                // logging
                                continue;
                            }
                            for (auto const& client : clients)
                            {
                                if (client->_mgrctx.dev != nullptr)
                                {
                                    continue;    // from an earlier outer for loop
                                }
                                if ((client->_deviceIndex != InvalidDeviceIndex && d.index == client->_deviceIndex)
                                    || ((client->_selector != nullptr) && client->_selector->SelectDevice(d)))
                                {
                                    _OpenDevice(dev, client->_config);
                                    client->_mgrctx.dev = dev;
                                    dev                 = nullptr;
                                    break;
                                }
                            }
                            if (dev != nullptr)
                            {
                                // Couldnt match a selector. Just assign to the first client that needs a device
                                for (auto const& client : clients)
                                {
                                    if (client->_mgrctx.dev != nullptr)
                                    {
                                        continue;    // from an earlier outer for loop
                                    }
                                    _OpenDevice(dev, client->_config);
                                    client->_mgrctx.dev = dev;
                                    dev                 = nullptr;
                                    break;
                                }
                            }
                            if (dev != nullptr)
                            {
                                // Couldnt find any client needing a device. Ignore and move on
                                rtlsdr_close(dev);
                            }
                        }
                    }
                    break;
                } catch (std::exception const&) { deviceSearching = true; }
            } while (true);
        }

        static void _OpenDevice(rtlsdr_dev_t* dev, RTLSDR::Config const& config)
        {
            auto gain = config.gain;
            /* Set gain, frequency, sample rate, and reset the device. */
            rtlsdr_set_tuner_gain_mode(dev, (gain == AutoGain) ? 0 : 1);
            if (gain != AutoGain)
            {
                if (gain == MaxGain)
                {
                    /* Find the maximum gain available. */
                    int gains[100];
                    int numgains = rtlsdr_get_tuner_gains(dev, gains);
                    gain         = gains[numgains - 1];
                }

                rtlsdr_set_tuner_gain(dev, gain);
            }

            rtlsdr_set_freq_correction(dev, CorrectionPPM);
            rtlsdr_set_agc_mode(dev, static_cast<int>(config.enableAGC));
            rtlsdr_set_center_freq(dev, config.frequency);
            rtlsdr_set_sample_rate(dev, config.sampleRate);
            rtlsdr_reset_buffer(dev);
            //_currentGain = rtlsdr_get_tuner_gain(dev) / 10;
        }
        bool                        deviceSearching{false};
        std::future<void>           deviceSearchThread;
        std::unordered_set<RTLSDR*> clients;
    };

    public:
    static constexpr uint32_t InvalidDeviceIndex = std::numeric_limits<uint32_t>::max();

    RTLSDR(IDeviceSelector const* const selector, uint32_t deviceIndex, Config const& config) :
        _selector(selector), _deviceIndex(deviceIndex), _config(config), _sizeMask(_config.bufferCount - 1)
    {

        if ((_config.bufferCount & _sizeMask) != 0) { throw std::logic_error("Buffer Count requested is not a power of 2"); }
        _cyclicBuffer.resize(_config.bufferCount);

        _testDataFile = std::filesystem::absolute(std::to_string(config.frequency) + ".test.dat");

        if (std::filesystem::exists(_testDataFile))
        {
            _useTestDataFile = true;
            return;
        }
        if (_deviceIndex != InvalidDeviceIndex)
        {
            //_OpenDevice(_deviceIndex);
        }
        else if (_selector == nullptr) { throw std::invalid_argument("Either a device index or a selector required"); }
    }

    RTLSDR(uint32_t deviceIndex, Config const& config) : RTLSDR(nullptr, deviceIndex, config) {}
    RTLSDR(IDeviceSelector const* const selector, Config const& config) : RTLSDR(selector, InvalidDeviceIndex, config) {}

    CLASS_DELETE_COPY_AND_MOVE(RTLSDR);

    void _TestDataReadLoop()
    {
        std::vector<std::vector<uint8_t>> buffers;
        buffers.reserve(_config.bufferCount);
        for (size_t i = 0; i < _config.bufferCount; i++) { buffers.emplace_back(_config.bufferLength); }
        _testDataIFS = std::ifstream(_testDataFile.string(), std::ios::binary | std::ios::in);
        if (!_testDataIFS.good()) { throw std::runtime_error("Cannot open test data file"); }
        size_t bufferToUse = 0;
        while (!_stopRequested)
        {
            _testDataIFS.read(reinterpret_cast<char*>(buffers[bufferToUse].data()), static_cast<std::streamsize>(_config.bufferLength));
            if (!_testDataIFS.good())
            {
                _testDataIFS.close();
                _testDataIFS = std::ifstream(_testDataFile.string(), std::ios::binary | std::ios::in);
                if (!_testDataIFS.good()) { throw std::runtime_error("Cannot open test data file"); }
                continue;
            }
            _OnDataAvailable(buffers[bufferToUse]);
            bufferToUse = (bufferToUse + 1) % _config.bufferCount;
        }
        _testDataIFS.close();
    }

    void Start(IDataHandler* handler)
    {
        Stop();
        _stopRequested = false;
        _handler       = handler;
        if (_producerThrd.joinable()) { _producerThrd.join(); }
        if (_consumerThrd.joinable()) { _consumerThrd.join(); }

        if (_useTestDataFile)
        {
            _producerThrd = std::thread([this]() {
                SetThreadName("RTLSDR-TestDataFile-Reader");
                _TestDataReadLoop();
            });
        }
        else
        {
            _producerThrd = std::thread([this]() {
                SetThreadName("RTLSDR::ReadAsync");
                _device_manager->Start(this);
            });
        }
        _consumerThrd = std::thread([this]() {
            SetThreadName("RTLSDR::DataHandler");
            this->_ConsumerThreadLoop();
        });
    }

    void Stop()
    {
        _stopRequested = true;
        _device_manager->Stop(this);
        _cvDataConsumed.notify_all();
        _cvDataAvailable.notify_all();
        // Joining can cause hangs on repeat start and stop if the data thread is waiting on mutex
        // if (_producerThrd.joinable()) _producerThrd.join();
        // if (_consumerThrd.joinable()) _consumerThrd.join();
    }

    bool _HasSlot(std::unique_lock<std::mutex> const& /*lock*/) const { return ((_tail + 1) & _sizeMask) != _head; }
    bool _IsEmpty(std::unique_lock<std::mutex> const& /*lock*/) const { return _head == _tail; }

    void _OnDataAvailable(std::span<uint8_t const> const& data)
    {
        std::unique_lock lock(_mutex);
        if (!_HasSlot(lock))
        {
            _cvDataConsumed.wait(lock, [&, this]() { return _HasSlot(lock) || _stopRequested; });
            if (_stopRequested) { return; }
        }

        _cyclicBuffer[_tail] = {.time = time_point::clock::now(), .data = data};
        _tail                = (_tail + 1) & _sizeMask;
        _cvDataAvailable.notify_one();
    }

    auto& _GetAtHead()
    {
        std::unique_lock lock(_mutex);
        return _cyclicBuffer[_head];
    }

    void _ConsumerThreadLoop()
    {
        while (!_stopRequested)
        {
            {
                std::unique_lock lock(_mutex);
                if (_IsEmpty(lock))
                {
                    _cvDataAvailable.wait(lock, [&, this]() { return !_IsEmpty(lock) || _stopRequested; });
                    continue;
                }
            }
            if (_stopRequested) { break; }
            _handler->HandleData(_GetAtHead().data);

            {
                std::unique_lock lock(_mutex);
                _head = (_head + 1) & _sizeMask;
                _cvDataConsumed.notify_one();
            }
        }
    }

    ~RTLSDR()
    {
        Stop();
        if (_producerThrd.joinable()) { _producerThrd.join(); }
        if (_consumerThrd.joinable()) { _consumerThrd.join(); }
    }

    private:
    static void _Callback(uint8_t* buf, uint32_t len, void* ctx) noexcept
    {
        try
        {
            reinterpret_cast<RTLSDR*>(ctx)->_OnDataAvailable({buf, len});
        } catch (std::exception const& ex) { std::cerr << ex.what() << '\n'; }
    }

    std::shared_ptr<Manager> _device_manager = Manager::GetInstance();
    ManagerContext           _mgrctx;

    IDeviceSelector const* const _selector{};
    uint32_t                     _deviceIndex{InvalidDeviceIndex};
    Config                       _config{};

    struct Entry
    {
        time_point               time;
        std::span<uint8_t const> data;
    };

    std::vector<Entry> _cyclicBuffer;
    size_t             _sizeMask{0};
    size_t             _head{0};
    size_t             _tail{0};

    IDataHandler*           _handler{};
    std::thread             _producerThrd;
    std::thread             _consumerThrd;
    std::mutex              _mutex;
    std::condition_variable _cvDataAvailable;
    std::condition_variable _cvDataConsumed;
    std::atomic_bool        _stopRequested{false};

    std::ifstream         _testDataIFS;
    std::filesystem::path _testDataFile;
    bool                  _useTestDataFile = false;
};
