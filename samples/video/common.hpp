#include <iostream>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>

static void ExitFail(const std::string& msg)
{
  std::cout << "Failure: " << msg << std::endl;
  std::cout << "Program terminating" << std::endl;
  exit(EXIT_FAILURE);
}

static void SleepSeconds(int seconds)
{
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
}
