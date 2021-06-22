#pragma once
#include "CommonMacros.h"

#include <rtl-sdr.h>


#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

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

    static std::vector<DeviceInfo> GetAllDevices()
    {
        auto                    deviceCount = rtlsdr_get_device_count();
        std::vector<DeviceInfo> devices;
        devices.resize(deviceCount);
        for (uint32_t i = 0; i < deviceCount; i++)
        {
            auto& d = devices[i];
            d.index = i;
            rtlsdr_get_device_usb_strings(d.index, d.vendor, d.product, d.serial);
        }
        return devices;
    }

    RTLSDR(uint32_t deviceIndex, Config const& config) : _config(config)
    {
        _testDataFile = std::filesystem::absolute(std::to_string(config.frequency) + ".test.dat");
        if (std::filesystem::exists(_testDataFile))
        {
            _useTestDataFile = true;
            return;
        }

        if (rtlsdr_open(&_dev, deviceIndex) < 0) throw std::runtime_error("No supported RTLSDR devices found");

        /* Set gain, frequency, sample rate, and reset the device. */
        rtlsdr_set_tuner_gain_mode(_dev, (_config.gain == AutoGain) ? 0 : 1);
        if (_config.gain != AutoGain)
        {
            if (_config.gain == MaxGain)
            {
                /* Find the maximum gain available. */
                int gains[100];
                int numgains = rtlsdr_get_tuner_gains(_dev, gains);
                _config.gain = gains[numgains - 1];
            }

            rtlsdr_set_tuner_gain(_dev, _config.gain);
        }

        rtlsdr_set_freq_correction(_dev, CorrectionPPM);
        rtlsdr_set_agc_mode(_dev, _config.enableAGC);
        rtlsdr_set_center_freq(_dev, _config.frequency);
        rtlsdr_set_sample_rate(_dev, config.sampleRate);
        rtlsdr_reset_buffer(_dev);
        _currentGain = rtlsdr_get_tuner_gain(_dev) / 10;

        _sizeMask = _config.bufferCount - 1;
        if ((_config.bufferCount & _sizeMask) != 0)
        {
            throw std::logic_error("Buffer Count requested is not a power of 2");
        }
        _cyclicBuffer.resize(_config.bufferCount);
    }

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
            _testDataIFS.read(reinterpret_cast<char*>(buffers[bufferToUse].data()), _config.bufferLength);
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
        _handler = handler;
        _started = true;
        if (_useTestDataFile)
        {
            _producerThrd = std::thread([this]() { _TestDataReadLoop(); });
        }
        else
        {
            _producerThrd = std::thread(
                [this]() { rtlsdr_read_async(_dev, _Callback, this, _config.bufferCount, _config.bufferCount * _config.bufferLength); });
        }
        _consumerThrd = std::thread([this]() { this->_ConsumerThreadLoop(); });
    }

    void Stop()
    {
        if (!_started)
        {
            return;
        }
        _stopRequested = true;
        if (!_useTestDataFile)
        {
            rtlsdr_cancel_async(_dev);
        }

        _cvDataConsumed.notify_one();
        _cvDataAvailable.notify_one();
        _producerThrd.join();
        _consumerThrd.join();
        _stopRequested = false;
    }

    bool _HasSlot(std::unique_lock<std::mutex> const& /*lock*/) const { return ((_tail + 1) & _sizeMask) != _head; }
    bool _IsEmpty(std::unique_lock<std::mutex> const& /*lock*/) const { return _head == _tail; }

    void _OnDataAvailable(std::span<uint8_t const> const& data)
    {
        std::unique_lock lock(_mutex);
        if (!_HasSlot(lock))
        {
            _cvDataConsumed.wait(lock, [&, this]() { return _HasSlot(lock); });
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
                    _cvDataAvailable.wait(lock, [&, this]() { return !_IsEmpty(lock); });
                    continue;
                }
            }

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
        if (_dev) rtlsdr_close(_dev);
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

    int _currentGain{};

    Config        _config{};
    rtlsdr_dev_t* _dev{nullptr};

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
    std::atomic_bool        _started{false};

    std::ifstream         _testDataIFS;
    std::filesystem::path _testDataFile;
    bool                  _useTestDataFile = false;
    std::thread           _testDataThread;
};
