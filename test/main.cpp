#include <csignal>
#include <gtest/gtest.h>

void signal_handler(int signal) { std::exit(0); }

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  int i = RUN_ALL_TESTS();

  std::signal(SIGILL, signal_handler);
  std::signal(SIGSEGV, signal_handler);
  std::signal(SIGABRT, signal_handler);
  return i;
}