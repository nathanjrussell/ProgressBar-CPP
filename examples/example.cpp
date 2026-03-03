#include <chrono>
#include <cstdint>
#include <thread>

#include <progressbar/progress_bars.hpp>

int main() {
  using namespace std::chrono_literals;

  progressbar::ProgressBars::Options options;
  options.enabled = true;
  options.minRedrawInterval = std::chrono::milliseconds(40);
  options.barWidth = 28;
  options.removeCompletedAfter = std::chrono::milliseconds(2000);

  // Demonstrates optional logging of completed task durations.
  progressbar::ProgressBars pbs(options, std::string{"progressbar-example.log"});

  const std::uint64_t total = 100;

  // ---- int-id usage (unchanged) ----
  const int a = pbs.createProgressBar(total, "Download", progressbar::ProgressBars::Color::Cyan);
  const int b = pbs.createProgressBar(total, "Extract", progressbar::ProgressBars::Color::Yellow);

  // ---- string-id usage (new) ----
  // Create using a stable string key. Returns the underlying int id.
  const int c = pbs.createProgressBar("install", total, "Install", progressbar::ProgressBars::Color::Green);
  (void)c;

  for (std::uint64_t i = 0; i <= total; ++i) {
    pbs.updateProgressBar(a, i);
    if (i % 2 == 0) pbs.updateProgressBar(b);

    // Update using the string id.
    if (i % 3 == 0) pbs.updateProgressBar("install");

    std::this_thread::sleep_for(20ms);
  }

  // Complete using a mix of int and string ids.
  pbs.markProgressBarComplete(a);

  std::this_thread::sleep_for(3000ms);

  pbs.markProgressBarComplete(b);

  // Let the completed bars auto-remove.
  std::this_thread::sleep_for(7500ms);
  pbs.markProgressBarComplete("install");
  return 0;
}
