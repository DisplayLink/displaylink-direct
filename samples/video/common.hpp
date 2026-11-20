#include <cstdlib>
#include <thread>
#include <chrono>

static void SleepSeconds(int seconds)
{
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
}
