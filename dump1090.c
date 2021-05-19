#include "dump1090.h"
#include "cpu.h"

#include <stdarg.h>

struct _Modes Modes;

//
// ============================= Utility functions ==========================
//

static void log_with_timestamp(const char* format, ...) __attribute__((format(printf, 1, 2)));

static void log_with_timestamp(const char* format, ...)
{
    char      timebuf[128];
    char      msg[1024];
    time_t    now;
    struct tm local;
    va_list   ap;

    now = time(NULL);
    localtime_r(&now, &local);
    strftime(timebuf, 128, "%c %Z", &local);
    timebuf[127] = 0;

    va_start(ap, format);
    vsnprintf(msg, 1024, format, ap);
    va_end(ap);
    msg[1023] = 0;

    fprintf(stderr, "%s  %s\n", timebuf, msg);
}

//
// =============================== Initialization ===========================
//
static void modesInitConfig(void)
{
    // Default everything to zero/NULL
    memset(&Modes, 0, sizeof(Modes));

    // Now initialise things that should not be 0/NULL to their defaults
    Modes.gain                    = MODES_MAX_GAIN;
    Modes.freq                    = MODES_DEFAULT_FREQ;
    Modes.check_crc               = 1;
    Modes.fix_df                  = 1;
    Modes.net_heartbeat_interval  = MODES_NET_HEARTBEAT_INTERVAL;
    Modes.interactive_display_ttl = MODES_INTERACTIVE_DISPLAY_TTL;
    Modes.json_interval           = 1000;
    Modes.json_stats_interval     = 60000;
    Modes.json_location_accuracy  = 1;
    Modes.maxRange                = 1852 * 300;    // 300NM default max range
    Modes.mode_ac_auto            = 1;

    sdrInitConfig();
}
//
//=========================================================================
//
static void modesInit(void)
{
    int i;

    Modes.sample_rate = 2400000.0;

    // Allocate the various buffers used by Modes
    Modes.trailing_samples = (MODES_PREAMBLE_US + MODES_LONG_MSG_BITS + 16) * 1e-6 * Modes.sample_rate;

    if (((Modes.log10lut = (uint16_t*)malloc(sizeof(uint16_t) * 256 * 256)) == NULL))
    {
        fprintf(stderr, "Out of memory allocating data buffer.\n");
        exit(1);
    }

    if (!fifo_create(MODES_MAG_BUFFERS, MODES_MAG_BUF_SAMPLES + Modes.trailing_samples, Modes.trailing_samples))
    {
        fprintf(stderr, "Out of memory allocating FIFO\n");
        exit(1);
    }

    // Validate the users Lat/Lon home location inputs
    if ((Modes.fUserLat > 90.0)        // Latitude must be -90 to +90
        || (Modes.fUserLat < -90.0)    // and
        || (Modes.fUserLon > 360.0)    // Longitude must be -180 to +360
        || (Modes.fUserLon < -180.0))
    {
        Modes.fUserLat = Modes.fUserLon = 0.0;
    }
    else if (Modes.fUserLon > 180.0)
    {    // If Longitude is +180 to +360, make it -180 to 0
        Modes.fUserLon -= 360.0;
    }
    // If both Lat and Lon are 0.0 then the users location is either invalid/not-set, or (s)he's in the
    // Atlantic ocean off the west coast of Africa. This is unlikely to be correct.
    // Set the user LatLon valid flag only if either Lat or Lon are non zero. Note the Greenwich meridian
    // is at 0.0 Lon,so we must check for either fLat or fLon being non zero not both.
    // Testing the flag at runtime will be much quicker than ((fLon != 0.0) || (fLat != 0.0))
    Modes.bUserFlags &= ~MODES_USER_LATLON_VALID;
    if ((Modes.fUserLat != 0.0) || (Modes.fUserLon != 0.0))
    {
        Modes.bUserFlags |= MODES_USER_LATLON_VALID;
    }

    // Limit the maximum requested raw output size to less than one Ethernet Block
    if (Modes.net_output_flush_size > (MODES_OUT_FLUSH_SIZE))
    {
        Modes.net_output_flush_size = MODES_OUT_FLUSH_SIZE;
    }
    if (Modes.net_output_flush_interval > (MODES_OUT_FLUSH_INTERVAL))
    {
        Modes.net_output_flush_interval = MODES_OUT_FLUSH_INTERVAL;
    }
    if (Modes.net_sndbuf_size > (MODES_NET_SNDBUF_MAX))
    {
        Modes.net_sndbuf_size = MODES_NET_SNDBUF_MAX;
    }

    // Prepare the log10 lookup table: 100log10(x)
    Modes.log10lut[0] = 0;    // poorly defined..
    for (i = 1; i <= 65535; i++)
    {
        Modes.log10lut[i] = (uint16_t)round(100.0 * log10(i));
    }

    // Prepare error correction tables
    modesChecksumInit(Modes.nfix_crc);
    icaoFilterInit();
    modeACInit();

    if (Modes.show_only) icaoFilterAdd(Modes.show_only);
}

//
//=========================================================================
//
// We use a thread reading data in background, while the main thread
// handles decoding and visualization of data to the user.
//
// The reading thread calls the RTLSDR API to read data asynchronously, and
// uses a callback to populate the data buffer.
//
// A Mutex is used to avoid races with the decoding thread.
//

//
//=========================================================================
//
// We read data using a thread, so the main thread only handles decoding
// without caring about data acquisition
//

static void* readerThreadEntryPoint(void* arg)
{
    MODES_NOTUSED(arg);

    sdrRun();

    if (!Modes.exit) Modes.exit = 2;    // unexpected exit

    fifo_halt();    // wakes the main thread, if it's still waiting
    return NULL;
}
//
// ============================== Snip mode =================================
//
// Get raw IQ samples and filter everything is < than the specified level
// for more than 256 samples in order to reduce example file size
//
static void snipMode(int level)
{
    int      i, q;
    uint64_t c = 0;

    while ((i = getchar()) != EOF && (q = getchar()) != EOF)
    {
        if (abs(i - 127) < level && abs(q - 127) < level)
        {
            c++;
            if (c > MODES_PREAMBLE_SIZE) continue;
        }
        else
        {
            c = 0;
        }
        putchar(i);
        putchar(q);
    }
}
//
// ================================ Main ====================================
//
static void showVersion()
{
    printf("-----------------------------------------------------------------------------\n");
    printf("| dump1090 ModeS Receiver     %45s |\n", MODES_DUMP1090_VARIANT " " MODES_DUMP1090_VERSION);
    printf("| build options: %-58s |\n",
           ""
#ifdef ENABLE_RTLSDR
           "ENABLE_RTLSDR "
#endif
#ifdef ENABLE_BLADERF
           "ENABLE_BLADERF "
#endif
#ifdef ENABLE_HACKRF
           "ENABLE_HACKRF "
#endif
#ifdef ENABLE_LIMESDR
           "ENABLE_LIMESDR "
#endif
    );
    printf("-----------------------------------------------------------------------------\n");
}

static void showDSP()
{
    printf("  detected runtime CPU features: ");
    if (cpu_supports_avx()) printf("AVX ");
    if (cpu_supports_avx2()) printf("AVX2 ");
    if (cpu_supports_armv7_neon_vfpv4()) printf("ARMv7+NEON+VFPv4 ");
    printf("\n");

    printf("  selected DSP implementations: \n");
#define SHOW(x)                                                                       \
    do                                                                                \
    {                                                                                 \
        printf("    %-40s %s\n", #x, starch_##x##_select()->name);                    \
        printf("    %-40s %s\n", #x "_aligned", starch_##x##_aligned_select()->name); \
    } while (0)

    SHOW(magnitude_uc8);
    SHOW(magnitude_power_uc8);
    SHOW(magnitude_sc16);
    SHOW(magnitude_sc16q11);
    SHOW(mean_power_u16);

#undef SHOW

    printf("\n");
}

// Accumulate stats data from stats_current to stats_periodic, stats_alltime and stats_latest;
// reset stats_current
static void flush_stats(uint64_t now)
{
    add_stats(&Modes.stats_current, &Modes.stats_periodic, &Modes.stats_periodic);
    add_stats(&Modes.stats_current, &Modes.stats_alltime, &Modes.stats_alltime);
    add_stats(&Modes.stats_current, &Modes.stats_latest, &Modes.stats_latest);

    reset_stats(&Modes.stats_current);
    Modes.stats_current.start = Modes.stats_current.end = now;
}

//
//=========================================================================
//
// This function is called a few times every second by main in order to
// perform tasks we need to do continuously, like accepting new clients
// from the net, refreshing the screen in interactive mode, and so forth
//
static void backgroundTasks(void)
{
    static uint64_t next_stats_display;
    static uint64_t next_stats_update;
    static uint64_t next_json_stats_update;
    static uint64_t next_json, next_history;

    uint64_t now = mstime();

    if (Modes.sdr_type != SDR_IFILE)
    {
        // don't run these if processing data from a file
        icaoFilterExpire();
        trackPeriodicUpdate();
    }

    if (Modes.net)
    {
        modesNetPeriodicWork();
    }

    // Refresh screen when in interactive mode
    if (Modes.interactive)
    {
        // interactiveShowData();
    }

    // copy out reader CPU time and reset it
    sdrUpdateCPUTime(&Modes.stats_current.reader_cpu);

    // always update end time so it is current when requests arrive
    Modes.stats_current.end = mstime();

    // 1-minute stats update
    if (now >= next_stats_update)
    {
        int i;

        if (next_stats_update == 0)
        {
            next_stats_update = now + 60000;
        }
        else
        {
            flush_stats(now);    // Ensure stats_latest is up to date

            // move stats_latest into 1-min ring buffer
            Modes.stats_newest_1min                   = (Modes.stats_newest_1min + 1) % 15;
            Modes.stats_1min[Modes.stats_newest_1min] = Modes.stats_latest;
            reset_stats(&Modes.stats_latest);

            // recalculate 5-min window
            reset_stats(&Modes.stats_5min);
            for (i = 0; i < 5; ++i)
                add_stats(&Modes.stats_1min[(Modes.stats_newest_1min - i + 15) % 15], &Modes.stats_5min, &Modes.stats_5min);

            // recalculate 15-min window
            reset_stats(&Modes.stats_15min);
            for (i = 0; i < 15; ++i) add_stats(&Modes.stats_1min[i], &Modes.stats_15min, &Modes.stats_15min);

            next_stats_update += 60000;
        }
    }

    // --stats-every display
    if (Modes.stats && now >= next_stats_display)
    {
        if (next_stats_display == 0)
        {
            next_stats_display = now + Modes.stats;
        }
        else
        {
            flush_stats(now);    // Ensure stats_periodic is up to date

            display_stats(&Modes.stats_periodic);
            reset_stats(&Modes.stats_periodic);

            next_stats_display += Modes.stats;
            if (next_stats_display <= now)
            {
                /* something has gone wrong, perhaps the system clock jumped */
                next_stats_display = now + Modes.stats;
            }
        }
    }

    // json stats update
    if (Modes.json_dir && now >= next_json_stats_update)
    {
        if (next_json_stats_update == 0)
        {
            next_json_stats_update = now + Modes.json_stats_interval;
        }
        else
        {
            flush_stats(now);    // Ensure everything we'll write is up to date
            writeJsonToFile("stats.json", generateStatsJson);
            next_json_stats_update += Modes.json_stats_interval;
        }
    }

    if (Modes.json_dir && now >= next_json)
    {
        writeJsonToFile("aircraft.json", generateAircraftJson);
        next_json = now + Modes.json_interval;
    }

    if (now >= next_history)
    {
        int rewrite_receiver_json = (Modes.json_dir && Modes.json_aircraft_history[HISTORY_SIZE - 1].content == NULL);

        free(Modes.json_aircraft_history[Modes.json_aircraft_history_next].content);    // might be NULL, that's OK.
        Modes.json_aircraft_history[Modes.json_aircraft_history_next].content
            = generateAircraftJson("/data/aircraft.json", &Modes.json_aircraft_history[Modes.json_aircraft_history_next].clen);

        if (Modes.json_dir)
        {
            char filebuf[PATH_MAX];
            snprintf(filebuf, PATH_MAX, "history_%d.json", Modes.json_aircraft_history_next);
            writeJsonToFile(filebuf, generateHistoryJson);
        }

        Modes.json_aircraft_history_next = (Modes.json_aircraft_history_next + 1) % HISTORY_SIZE;

        if (rewrite_receiver_json) writeJsonToFile("receiver.json", generateReceiverJson);    // number of history entries changed

        next_history = now + HISTORY_INTERVAL;
    }
}

int startlistener(char const* const dev_name)
{
    int j;

    // Set sane defaults
    modesInitConfig();

    showVersion();
    showDSP();

    Modes.dev_name = strdup(dev_name);
    if (Modes.nfix_crc > MODES_MAX_BITERRORS) Modes.nfix_crc = MODES_MAX_BITERRORS;

    log_with_timestamp("%s %s starting up.", MODES_DUMP1090_VARIANT, MODES_DUMP1090_VERSION);
    modesInit();

    if (!sdrOpen())
    {
        exit(1);
    }

    if (Modes.net)
    {
        modesInitNet();
    }

    // init stats:
    Modes.stats_current.start = Modes.stats_current.end = Modes.stats_alltime.start = Modes.stats_alltime.end = Modes.stats_periodic.start
        = Modes.stats_periodic.end = Modes.stats_latest.start = Modes.stats_latest.end = Modes.stats_5min.start = Modes.stats_5min.end
        = Modes.stats_15min.start = Modes.stats_15min.end = mstime();

    for (j = 0; j < 15; ++j) Modes.stats_1min[j].start = Modes.stats_1min[j].end = Modes.stats_current.start;

    // write initial json files so they're not missing
    // writeJsonToFile("receiver.json", generateReceiverJson);
    // writeJsonToFile("stats.json", generateStatsJson);
    // writeJsonToFile("aircraft.json", generateAircraftJson);

    // interactiveInit();

    // If the user specifies --net-only, just run in order to serve network
    // clients without reading data from the RTL device
    if (Modes.sdr_type == SDR_NONE)
    {
        while (!Modes.exit)
        {
            struct timespec start_time;
            struct timespec slp = {0, 100 * 1000 * 1000};

            start_cpu_timing(&start_time);
            backgroundTasks();
            end_cpu_timing(&start_time, &Modes.stats_current.background_cpu);

            nanosleep(&slp, NULL);
        }
    }
    else
    {
        int watchdogCounter = 10;    // about 1 second

        // Create the thread that will read the data from the device.
        pthread_create(&Modes.reader_thread, NULL, readerThreadEntryPoint, NULL);

        while (!Modes.exit)
        {
            // get the next sample buffer off the FIFO; wait only up to 100ms
            // this is fairly aggressive as all our network I/O runs out of the background work!
            struct mag_buf* buf = fifo_dequeue(100 /* milliseconds */);
            struct timespec start_time;

            if (buf)
            {
                // Process one buffer

                start_cpu_timing(&start_time);
                demodulate2400(buf);
                if (Modes.mode_ac)
                {
                    demodulate2400AC(buf);
                }

                Modes.stats_current.samples_processed += buf->validLength - buf->overlap;
                Modes.stats_current.samples_dropped += buf->dropped;
                end_cpu_timing(&start_time, &Modes.stats_current.demod_cpu);

                // Return the buffer to the FIFO freelist for reuse
                fifo_release(buf);

                // We got something so reset the watchdog
                watchdogCounter = 10;
            }
            else
            {
                // Nothing to process this time around.
                if (--watchdogCounter <= 0)
                {
                    log_with_timestamp("No data received from the SDR for a long time, it may have wedged");
                    watchdogCounter = 600;
                }
            }

            start_cpu_timing(&start_time);
            backgroundTasks();
            end_cpu_timing(&start_time, &Modes.stats_current.background_cpu);
        }

        log_with_timestamp("Waiting for receive thread termination");
        fifo_halt();                                // Reader thread should do this anyway, but just in case..
        pthread_join(Modes.reader_thread, NULL);    // Wait on reader thread exit
    }

    // interactiveCleanup();

    // Write final stats
    flush_stats(0);
    writeJsonToFile("stats.json", generateStatsJson);
    if (Modes.stats)
    {
        display_stats(&Modes.stats_alltime);
    }

    sdrClose();
    fifo_destroy();

    if (Modes.exit == 1)
    {
        log_with_timestamp("Normal exit.");
        return 0;
    }
    else
    {
        log_with_timestamp("Abnormal exit.");
        return 1;
    }
}
//
//=========================================================================
//
char* generateReceiverJson(const char* url_path, int* len)
{
}
void writeJsonToFile(const char* file, char* (*generator)(const char*, int*))
{
}
void modesNetPeriodicWork(void)
{
}
char* generateStatsJson(const char* url_path, int* len)
{
}

char* generateAircraftJson(const char* url_path, int* len)
{
}
char* generateHistoryJson(const char* url_path, int* len)
{
}


void modesInitNet()
{
}