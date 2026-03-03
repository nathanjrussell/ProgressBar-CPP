#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#ifndef PROGRESSBAR_ENABLED
  #define PROGRESSBAR_ENABLED 1
#endif

namespace progressbar {

class ProgressBarsNoop {
public:
  enum class Color { Default, Red, Green, Yellow, Blue, Magenta, Cyan, White };

  struct Options {
    bool enabled = true;
    bool onlyRenderOnTty = true;
    bool honorNoColorEnv = true;
    std::chrono::milliseconds minRedrawInterval{50};
    int barWidth = 30;
    std::optional<std::chrono::milliseconds> removeCompletedAfter{std::nullopt};
    bool deleteLinesOnClear = true;
  };

  ProgressBarsNoop() = default;
  explicit ProgressBarsNoop(std::string) {}
  explicit ProgressBarsNoop(Options) {}
  ProgressBarsNoop(Options, std::optional<std::string>) {}

  int createProgressBar(std::uint64_t, std::string, Color = Color::Default) { return -1; }
  int createProgressBar(std::string_view, std::uint64_t, std::string, Color = Color::Default) { return -1; } // NOLINT

  void updateProgressBar(int, std::uint64_t) {}
  void updateProgressBar(int) {}
  void updateProgressBar(std::string_view, std::uint64_t) {}
  void updateProgressBar(std::string_view) {}

  void markProgressBarComplete(int) {}
  void markProgressBarComplete(std::string_view) {}

  void redrawNow() {}
};

#if PROGRESSBAR_ENABLED

class ProgressBarsReal {
public:
  enum class Color { Default, Red, Green, Yellow, Blue, Magenta, Cyan, White };

  struct Options {
    bool enabled = true;
    bool onlyRenderOnTty = true;
    bool honorNoColorEnv = true;
    std::chrono::milliseconds minRedrawInterval{50};
    int barWidth = 30;
    std::optional<std::chrono::milliseconds> removeCompletedAfter{std::nullopt};
    bool deleteLinesOnClear = true;
  };

  ProgressBarsReal();
  explicit ProgressBarsReal(std::string logFilePath);
  explicit ProgressBarsReal(Options options);
  ProgressBarsReal(Options options, std::optional<std::string> logFilePath);

  ~ProgressBarsReal();

  void setEnabled(bool enabled);
  bool isEnabled() const;

  int createProgressBar(std::uint64_t totalUnits, std::string label, Color color = Color::Default);
  int createProgressBar(std::string_view stringId, std::uint64_t totalUnits, std::string label,
                        Color color = Color::Default);

  void updateProgressBar(int pbid, std::uint64_t completedUnits);
  void updateProgressBar(int pbid);
  void updateProgressBar(std::string_view stringId, std::uint64_t completedUnits);
  void updateProgressBar(std::string_view stringId);

  void markProgressBarComplete(int pbid);
  void markProgressBarComplete(std::string_view stringId);

  void redrawNow();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#endif // PROGRESSBAR_ENABLED

#if PROGRESSBAR_ENABLED
using ProgressBars = ProgressBarsReal;
#else
using ProgressBars = ProgressBarsNoop;
#endif

} // namespace progressbar
