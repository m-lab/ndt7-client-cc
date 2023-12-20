// The purpose of this file is to make sure the standalone
// header is building, so we include it directly.
#include "single_include/libndt7.hpp"
int main() {
  measurementlab::libndt7::Client client{
    measurementlab::libndt7::Settings{}
  };
  client.run();
}
