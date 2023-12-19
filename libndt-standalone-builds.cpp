// The purpose of this file is to make sure the standalone
// header is building, so we include it directly.
#include "single_include/libndt7.hpp"
int main() {
  measurement_kit::libndt::Client client{
    measurement_kit::libndt::Settings{}
  };
  client.run();
}
