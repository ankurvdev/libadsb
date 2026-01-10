#pragma once

#include "AircraftImpl.h"
#include "RTLSDR.hpp"
namespace ADSB
{

std::unique_ptr<ADSB::IDataProvider> TryCreateUAT978Handler(std::shared_ptr<ADSB::TrafficManager> const& trafficManager,
                                                            RTLSDR::IDeviceSelector const*               selector,
                                                            uint8_t                                      sourceId);
std::unique_ptr<ADSB::IDataProvider> TryCreateADSB1090Handler(std::shared_ptr<ADSB::TrafficManager> const& trafficManager,
                                                              RTLSDR::IDeviceSelector const*               selector,
                                                              uint8_t                                      sourceId);
TrafficManager**                     GetThreadLocalTrafficManager();
}    // namespace ADSB

namespace ADSB::test
{
std::unique_ptr<RTLSDR::IDataHandler> TryCreateUAT978Handler(std::shared_ptr<ADSB::TrafficManager> const& trafficManager,
                                                             RTLSDR::IDeviceSelector const*               selector,
                                                             uint8_t                                      sourceId);
std::unique_ptr<RTLSDR::IDataHandler> TryCreateADSB1090Handler(std::shared_ptr<ADSB::TrafficManager> const& trafficManager,
                                                               RTLSDR::IDeviceSelector const*               selector,
                                                               uint8_t                                      sourceId);

}    // namespace ADSB::test
