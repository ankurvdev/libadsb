#pragma once
#include "CommonMacros.h"

SUPPRESS_WARNINGS_START
SUPPRESS_STL_WARNINGS
#include <SetThreadName.h>
#include <rtl-sdr.h>

#include <fstream>
SUPPRESS_WARNINGS_END

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <span>
#include <thread>
#include <unordered_set>
#include <vector>

#define M_PI 3.14159265358979323846

struct RTLSDR
{
    using time_point = std::chrono::time_point<std::chrono::system_clock>;

    struct IDataHandler
    {
        virtual ~IDataHandler()                                       = default;
        virtual void HandleData(std::span<uint8_t const> const& data) = 0;
    };

    static constexpr int      AutoGain        = -100;
    static constexpr int      MaxGain         = 999999;
    static constexpr int      CorrectionPPM   = 0;
    static constexpr uint32_t SampleRate      = 0;    // Invalid
    static constexpr size_t   BufferLength    = 32 * 512;
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
        char     vendor[256];
        char     product[256];
        char     serial[256];
    };

    struct IDeviceSelector
    {
        virtual ~IDeviceSelector()                           = default;
        virtual bool SelectDevice(DeviceInfo const& d) const = 0;
    };

    private:
    struct _ManagerContext
    {
        bool          startRequested{false};
        bool          listening{false};
        rtlsdr_dev_t* dev{nullptr};
    };

    // RTLSDR is hugely single threaded
    // Cannot open devices once a device is opened
    // A manager is required for orchestration
    struct _Manager
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
        static inline std::mutex              _mgr_mutex;
        static inline std::weak_ptr<_Manager> _instance;
#pragma clang diagnostic pop
        static std::shared_ptr<_Manager> GetInstance()
        {
            std::lock_guard<std::mutex> lock_guard(_mgr_mutex);
            if (_instance.expired())
            {
                auto ptr  = std::make_shared<_Manager>();
                _instance = ptr;
                return ptr;
            }
            return _instance.lock();
        }

        _Manager() = default;
        CLASS_DELETE_COPY_AND_MOVE(_Manager);

        void Start(RTLSDR* client)
        {
            {
                std::unique_lock<std::mutex> guard(_mgr_mutex);
                client->_mgrctx.startRequested = true;
                _clients.insert(client);
            }

            while (client->_mgrctx.startRequested)
            {
                {
                    std::unique_lock<std::mutex> guard(_mgr_mutex);
                    _ResetAll(guard);
                    if (client->_mgrctx.dev != nullptr)
                    {
                        client->_mgrctx.listening = true;
                    }
                }
                if (client->_mgrctx.dev != nullptr)
                {
                    rtlsdr_read_async(client->_mgrctx.dev,
                                      client->_Callback,
                                      client,
                                      client->_config.bufferCount,
                                      client->_config.bufferCount * client->_config.bufferLength);
                    client->_mgrctx.listening = false;
                }
                else
                {
                    for (size_t i = 0; i < 100 && client->_mgrctx.startRequested; i++)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds{10});
                    }
                }
                {
                    std::unique_lock<std::mutex> guard(_mgr_mutex);
                    client->_mgrctx.listening = false;
                }
            }
        }

        void Stop(RTLSDR* client)
        {
            std::unique_lock<std::mutex> guard(_mgr_mutex);
            client->_mgrctx.startRequested = false;
            _ResetAll(guard);
            _clients.erase(client);
        }

        void _StopAll(std::unique_lock<std::mutex>& /*guard*/)
        {
            for (auto client : _clients)
            {
                if (client->_mgrctx.dev != nullptr)
                {
                    auto dev            = client->_mgrctx.dev;
                    client->_mgrctx.dev = nullptr;
                    // Could be after mutex release but before read_async
                    while (client->_mgrctx.listening && rtlsdr_cancel_async(dev) != 0)
                        ;
                    rtlsdr_close(dev);
                }
            }
        }

        void _ResetAll(std::unique_lock<std::mutex>& guard)
        {
            _StopAll(guard);
            auto deviceCount = rtlsdr_get_device_count();
            for (uint32_t i = 0; i < deviceCount; i++)
            {
                DeviceInfo d;
                d.index = i;
                rtlsdr_get_device_usb_strings(d.index, d.vendor, d.product, d.serial);
                rtlsdr_dev_t* dev;
                if (rtlsdr_open(&dev, d.index) != 0)
                {
                    // logging
                    continue;
                }
                for (auto& client : _clients)
                {
                    if (!client->_mgrctx.startRequested) continue;
                    if ((client->_deviceIndex != InvalidDeviceIndex && d.index == client->_deviceIndex)
                        || (client->_selector && client->_selector->SelectDevice(d)))
                    {
                        client->_mgrctx.dev = dev;
                        _OpenDevice(client);
                    }
                }
            }
        }

        void _OpenDevice(RTLSDR* client)
        {
            auto  dev    = client->_mgrctx.dev;
            auto& config = client->_config;
            /* Set gain, frequency, sample rate, and reset the device. */
            rtlsdr_set_tuner_gain_mode(dev, (config.gain == AutoGain) ? 0 : 1);
            if (config.gain != AutoGain)
            {
                if (config.gain == MaxGain)
                {
                    /* Find the maximum gain available. */
                    int gains[100];
                    int numgains = rtlsdr_get_tuner_gains(dev, gains);
                    config.gain  = gains[numgains - 1];
                }

                rtlsdr_set_tuner_gain(dev, config.gain);
            }

            rtlsdr_set_freq_correction(dev, CorrectionPPM);
            rtlsdr_set_agc_mode(dev, config.enableAGC);
            rtlsdr_set_center_freq(dev, config.frequency);
            rtlsdr_set_sample_rate(dev, config.sampleRate);
            rtlsdr_reset_buffer(dev);
            //_currentGain = rtlsdr_get_tuner_gain(dev) / 10;
        }

        std::unordered_set<RTLSDR*> _clients;
    };

    public:
    static constexpr uint32_t InvalidDeviceIndex = std::numeric_limits<uint32_t>::max();

    RTLSDR(IDeviceSelector const* const selector, uint32_t deviceIndex, Config const& config) :
        _selector(selector), _deviceIndex(deviceIndex), _config(config)
    {
        _sizeMask = _config.bufferCount - 1;
        if ((_config.bufferCount & _sizeMask) != 0)
        {
            throw std::logic_error("Buffer Count requested is not a power of 2");
        }
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
        else if (_selector == nullptr)
        {
            throw std::invalid_argument("Either a device index or a selector required");
        }
    }

    RTLSDR(uint32_t deviceIndex, Config const& config) : RTLSDR(nullptr, deviceIndex, config) {}
    RTLSDR(IDeviceSelector const* const selector, Config const& config) : RTLSDR(selector, InvalidDeviceIndex, config) {}

    CLASS_DELETE_COPY_AND_MOVE(RTLSDR);

    void _TestDataReadLoop()
    {
        std::vector<std::vector<uint8_t>> buffers;
        for (size_t i = 0; i < _config.bufferCount; i++)
        {
            buffers.push_back(std::vector<uint8_t>(_config.bufferLength));
        }
        _testDataIFS = std::ifstream(_testDataFile.string(), std::ios::binary | std::ios::in);
        if (!_testDataIFS.good())
        {
            throw std::runtime_error("Cannot open test data file");
        }
        size_t bufferToUse = 0;
        while (!_stopRequested)
        {
            _testDataIFS.read(reinterpret_cast<char*>(buffers[bufferToUse].data()), static_cast<std::streamsize>(_config.bufferLength));
            if (!_testDataIFS.good())
            {
                _testDataIFS.close();
                _testDataIFS = std::ifstream(_testDataFile.string(), std::ios::binary | std::ios::in);
                if (!_testDataIFS.good())
                {
                    throw std::runtime_error("Cannot open test data file");
                }
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
        if (_producerThrd.joinable()) _producerThrd.join();
        if (_consumerThrd.joinable()) _consumerThrd.join();
        if (_useTestDataFile)
        {
            _producerThrd = std::thread(
                [this]()
                {
                    SetThreadName("RTLSDR-TestDataFile-Reader");
                    _TestDataReadLoop();
                });
        }
        else
        {
            _producerThrd = std::thread(
                [this]()
                {
                    SetThreadName("RTLSDR-Device-Reader");
                    _device_manager->Start(this);
                });
        }
        _consumerThrd = std::thread(
            [this]()
            {
                SetThreadName("RTLSDR-Data-Processor");
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
            if (_stopRequested) return;
        }

        _cyclicBuffer[_tail] = {time_point::clock::now(), data};
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
            if (_stopRequested) break;
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
        if (_producerThrd.joinable()) _producerThrd.join();
        if (_consumerThrd.joinable()) _consumerThrd.join();
    }

    private:
    static void _Callback(uint8_t* buf, uint32_t len, void* ctx) noexcept
    {
        try
        {
            reinterpret_cast<RTLSDR*>(ctx)->_OnDataAvailable({buf, len});
        }
        catch (std::exception const& ex)
        {
            std::cerr << ex.what() << std::endl;
        }
    }

    std::shared_ptr<_Manager> _device_manager = _Manager::GetInstance();
    _ManagerContext           _mgrctx;

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

    IDataHandler*           _handler;
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
