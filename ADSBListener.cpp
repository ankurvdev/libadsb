#include "ADSBListener.h"

#include <stdexcept>

// TODO : Thread safety
static ADSB::Listener* singletonListener = nullptr;

extern "C" void startlistener(const char*);
extern "C" void stoplistener();

struct ModeMessageImpl : ADSB::IModeMessage
{
    ModeMessageImpl(struct modesMessage* mm) {}
};

struct AirCraftImpl : ADSB::IAirCraft
{
    AirCraftImpl(struct aircraft* a) {}
};

void ADSB::Listener::Start()
{
    if (singletonListener != nullptr)
    {
        std::logic_error("Unfortunately we can only have a single listener in a process");
    }

    singletonListener = this;
    startlistener("0");
}

extern "C" void modesQueueOutput(struct modesMessage* mm, struct aircraft* a)
{
    singletonListener->Invoke(ModeMessageImpl(mm), AirCraftImpl(a));
}