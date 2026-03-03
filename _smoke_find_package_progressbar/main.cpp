#include <cstdint>
#include <thread>
#include <chrono>

#include <progressbar/progress_bars.hpp>

int main() {
  using namespace std::chrono_literals;

  progressbar::ProgressBars::Options opt;
  opt.minRedrawInterval = std::chrono::milliseconds(10);
  opt.removeCompletedAfter = std::chrono::milliseconds(200);

  progressbar::ProgressBars pbs(opt);
  int id = pbs.createProgressBar(10, "Smoke", progressbar::ProgressBars::Color::Magenta);

  for (std::uint64_t i = 0; i <= 10; ++i) {
    pbs.updateProgressBar(id, i);
    std::this_thread::sleep_for(10ms);
  }

  pbs.markProgressBarComplete(id);
  std::this_thread::sleep_for(250ms);
  return 0;
}

