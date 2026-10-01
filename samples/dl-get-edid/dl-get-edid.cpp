#include <dlsdk/dlsdk.h>
#include <csignal>
#include <iostream>
#include <unistd.h>
#include <chrono>

static bool g_quiet = false;


std::ostream& log()
{
  static std::ostream null(nullptr);
  if (g_quiet) {
    return null;
  } else {
    return std::cout;
  }
}

void printEdidFromDpAux(dl::sdk::DpAux& aux)
{
  const std::chrono::milliseconds timeout(1000);
  dl::sdk::DpAuxExclusiveAccess exclusiveAccess(&aux, timeout);

  const uint8_t edidAddress = 0x50;
  constexpr uint8_t EdidPageSizeBytes = 128;
  uint8_t edid[EdidPageSizeBytes] = {0};

  const auto status = aux.auxI2cReadData(edidAddress,
                                         edid,
                                         EdidPageSizeBytes,
                                         0,
                                         std::chrono::milliseconds(100));
  if (status == DLSDK_AUX_ACK) {
    log() << "Edid:" << std::endl;
    for (int i = 0; i < EdidPageSizeBytes; i++) {
      printf("%c", edid[i]);
    }
  } else {
    std::cerr << "DPaux edid read failed (" << (int)status << ")" << std::endl;
  }
}

void exit_handler(int signal) {

  log() << "exit_handler called with signal " << signal << std::endl;

  dlsdk_teardown();
  exit(-1);
}

int main(int argc, char* argv[])
{
  std::signal(SIGTERM, exit_handler);
  std::signal(SIGINT, exit_handler);

  int device = 0;
  int output = 0;
  int opt;

  while ((opt = getopt(argc, argv, "d:o:hq")) != -1) {
    switch (opt) {
    case 'q': g_quiet = true; break;
    case 'o': output = std::atoi(optarg); break;
    case 'd': device = std::atoi(optarg); break;
    case 'h':
    default:
        fprintf(
          stderr,
          "Usage: %s [-d device][-o output][-h][-q]\n"
          "\t-d <device> \t device number[0-<device_count>], default: 0\n"
          "\t-o <output> \t output number[0-<output_count>], default: 0\n"
          "\t-h \t print this help\n"
          "\t-q\t quiet mode\n",
          argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  dl::sdk::System system;

  const auto devices = system.getDevices();

  log() << "Found " << devices.size() << " devices" << std::endl;

  if (devices.size() < device + 1) {
    std::cerr << "Device no " << device << " not found. "
              << devices.size() << " devices available." << std::endl;
  } else {
    auto devicePtr = devices[device];
    log() << "Using device " << devicePtr.id() << std::endl;
    auto dp_aux = devicePtr.getDpAux(output);
    printEdidFromDpAux(dp_aux);
  }

  log() << "Finishing" << std::endl;

  return 0;
}
