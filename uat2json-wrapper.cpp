extern "C"
{

#include "dump978/uat.h"
#include "dump978/uat_decode.h"
    void dump_raw_message(char updown, uint8_t* data, int len, int rs_errors);
}
#include "AircraftImpl.h"
#include <iostream>

std::shared_ptr<TrafficManager>& GetThreadLocalTrafficManager();

void dump_raw_message(char /*updown*/, uint8_t* data, int /*len*/, int /*rs_errors*/)
{
    struct uat_adsb_mdb mdb;

    uat_decode_adsb_mdb(data, &mdb);
    auto  manager  = GetThreadLocalTrafficManager();
    auto& aircraft = manager->FindOrCreate(mdb.address);
    if (mdb.has_ms)
    {
        /*
        *
    fprintf(to,
            "MS:\n"
            " Emitter category:  %s\n"
            " Callsign:          %s%s\n"
            " Emergency status:  %s\n"
            " UAT version:       %u\n"
            " SIL:               %u\n"
            " Transmit MSO:      %u\n"
            " NACp:              %u\n"
            " NACv:              %u\n"
            " NICbaro:           %u\n"
            " Capabilities:      %s%s\n"
            " Active modes:      %s%s%s\n"
            " Target track type: %s\n",
            emitter_category_names[mdb->emitter_category],
            mdb->callsign_type == CS_SQUAWK ? "squawk " : "",
            mdb->callsign_type == CS_INVALID ? "unavailable" : mdb->callsign,
            emergency_status_names[mdb->emergency_status],
            mdb->uat_version,
            mdb->sil,
            mdb->transmit_mso,
            mdb->nac_p,
            mdb->nac_v,
            mdb->nic_baro,
            mdb->has_cdti ? "CDTI " : "", mdb->has_acas ? "ACAS " : "",
            mdb->acas_ra_active ? "ACASRA " : "", mdb->ident_active ? "IDENT " : "", mdb->atc_services ? "ATC " : "",
            mdb->heading_type == HT_MAGNETIC ? "magnetic heading" : "true heading");
        */
        if (mdb.callsign_type == CS_CALLSIGN)
        {
            std::copy(std::begin(mdb.callsign), std::end(mdb.callsign), std::begin(aircraft.callsign));
        }
        if (mdb.callsign_type == CS_SQUAWK)
        {
            std::cerr << "Squawk:" << aircraft.callsign << std::endl;
            // std::copy(std::begin(mdb.callsign), std::end(mdb.callsign), std::begin(aircraft.callsign));
        }
    }
    if (mdb.has_sv)
    {
        if (mdb.position_valid)
        {
            aircraft.lat1E7 = mdb.lat * 10000000;
            aircraft.lon1E7 = mdb.lon * 10000000;
        }
        if (mdb.speed_valid) aircraft.speed = mdb.speed;
        // if (mdb->ns_vel_valid) fprintf(to, " N/S velocity: %d kt\n", mdb->ns_vel);
        // if (mdb->ew_vel_valid) fprintf(to, " E/W velocity: %d kt\n", mdb->ew_vel);

        switch (mdb.altitude_type)
        {
        case ALT_BARO: aircraft.altitude = mdb.altitude; break;
        case ALT_GEO: aircraft.altitude = mdb.altitude; break;
        case ALT_INVALID:
        default: break;
        }

        switch (mdb.track_type)
        {
        case TT_TRACK: aircraft.track = mdb.track; break;
        case TT_MAG_HEADING: aircraft.track = mdb.track; break;
        case TT_TRUE_HEADING: aircraft.track = mdb.track; break;
        case TT_INVALID:
        default: break;
        }

        switch (mdb.vert_rate_source)
        {
        case ALT_BARO: aircraft.vert_rate = mdb.vert_rate; break;
        case ALT_GEO: aircraft.vert_rate = mdb.vert_rate; break;
        case ALT_INVALID:
        default: break;
        }
        /*
        if (mdb->dimensions_valid)
            fprintf(to,
                    " Dimensions: %.1fm L x %.1fm W%s\n",
                    mdb->length,
                    mdb->width,
                    mdb->position_offset ? " (position offset applied)" : "");

        fprintf(to, " UTC coupling: %s\nTIS-B site ID: %u\n", mdb->utc_coupled ? "yes" : "no", mdb->tisb_site_id);
        */
    }

    if (mdb.has_auxsv)
    {
        switch (mdb.sec_altitude_type)
        {
        case ALT_BARO: /*mdb.sec_altitude;*/ break;
        case ALT_GEO: /*mdb.sec_altitude;*/ break;
        case ALT_INVALID:
        default: break;
        }
    }
    aircraft.sourceId = 2u;
    manager->NotifyChanged(aircraft);
}
