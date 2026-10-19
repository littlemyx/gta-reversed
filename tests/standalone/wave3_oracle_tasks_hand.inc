// ORACLE-W3-TASKS: hand rows (functions the generator cannot call: free functions, template classes, out-parameters of special types). Included by wave3_oracle_tasks.inc.
#include "Tasks/TaskTypes/TaskComplexSunbathe.h"
#include "Weather.h"
#include "Clock.h"

static void TestW3TasksHand() {
    using namespace w2; using namespace w3t;
    // ---- CanSunbathe 0x632140: free function, hour of the day and the new weather type (exe globals; the harness restores the data image after the case)
    Run("CanSunbathe 0x632140", [&](Rng& r, std::string& d) {
        g_tpost = [](Rng& rr) {
            CClock::ms_nGameClockHours = (uint8)(rr.below(4) ? 8 + rr.below(12) : rr.below(24));
            CWeather::NewWeatherType = (eWeatherType)(rr.below(5) ? rr.below(23) : rr.below(40));
        };
        return W3TaskCase(r, d, 0, {}, [&](void*) -> uint64_t { return CanSunbathe() ? 1 : 0; }, [&](void*) -> uint64_t { return (uint8_t)oracle::Fn<int __cdecl()>(0x632140)() ? 1 : 0; });
    }, 200);
}
