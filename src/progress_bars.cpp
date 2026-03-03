#include <progressbar/progress_bars.hpp>

#if PROGRESSBAR_ENABLED

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
  #include <io.h>
  #define PROGRESSBAR_ISATTY _isatty
  #define PROGRESSBAR_FILENO _fileno
#else
  #include <unistd.h>
  #define PROGRESSBAR_ISATTY isatty
  #define PROGRESSBAR_FILENO fileno
#endif

namespace progressbar {

struct ProgressBarsReal::Impl {
  using Clock = std::chrono::steady_clock;

  struct Bar {
    int id{};
    std::uint64_t total{};
    std::uint64_t done{};
    std::string label;
    Color color{Color::Default};
    bool complete{false};
    Clock::time_point started{};
    std::optional<Clock::time_point> completedAt{};
  };

  Options options_;
  std::optional<std::string> logFilePath_;
  std::ofstream logStream_;

  mutable std::mutex mu_;
  int nextId_{1};

  bool renderingEnabled_{true};
  bool useColor_{true};

  std::vector<int> order_;
  std::unordered_map<int, Bar> barsById_;

  // New: map string ids to underlying int ids.
  std::map<std::string, int> stringToId_;

  std::optional<int> lookupStringId(std::string_view sid) {
    auto it = stringToId_.find(std::string(sid));
    if (it == stringToId_.end()) return std::nullopt;
    return it->second;
  }

  int getOrCreateStringId(std::string_view sid, int createdIntId) {
    auto [it, inserted] = stringToId_.emplace(std::string(sid), createdIntId);
    (void)inserted;
    return it->second;
  }

  int renderedLines_{0};
  Clock::time_point lastRedraw_{};

  static bool isStderrTty() {
    return PROGRESSBAR_ISATTY(PROGRESSBAR_FILENO(stderr)) != 0;
  }

  static const char* ansiColor(Color c) {
    switch (c) {
      case Color::Red: return "\x1b[31m";
      case Color::Green: return "\x1b[32m";
      case Color::Yellow: return "\x1b[33m";
      case Color::Blue: return "\x1b[34m";
      case Color::Magenta: return "\x1b[35m";
      case Color::Cyan: return "\x1b[36m";
      case Color::White: return "\x1b[37m";
      case Color::Default: default: return "\x1b[0m";
    }
  }

  static std::chrono::milliseconds durationFor(const Bar& bar) {
    auto end = bar.completedAt.has_value() ? *bar.completedAt : Clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - bar.started);
  }

  static std::string formatDuration(std::chrono::milliseconds ms) {
    const auto total_ms = ms.count();

    const auto hrs = total_ms / 3600000;
    const auto mins = (total_ms % 3600000) / 60000;
    const auto secs = (total_ms % 60000) / 1000;
    const auto rem_ms = total_ms % 1000;

    std::ostringstream os;
    if (hrs > 0) os << hrs << "h";
    if (mins > 0 || hrs > 0) os << mins << "m";
    os << secs << "s";
    os << std::setw(3) << std::setfill('0') << rem_ms << "ms";
    return os.str();
  }

  std::string formatLine(const Bar& bar) const {
    const int width = (options_.barWidth <= 0) ? 30 : options_.barWidth;

    const double pct = (bar.total == 0) ? 1.0
                                        : static_cast<double>(bar.done) / static_cast<double>(bar.total);
    const int filled = static_cast<int>(pct * width);

    std::ostringstream os;

    if (useColor_) os << ansiColor(bar.color);

    os << bar.label << " ";
    os << "[";
    for (int i = 0; i < width; ++i) os << (i < filled ? "#" : "-");
    os << "] ";

    os << std::setw(3) << static_cast<int>(pct * 100.0) << "% ";
    os << "(" << bar.done << "/" << bar.total << ")";

    if (bar.complete) {
      if (useColor_) os << "\x1b[0m";
      os << " completed";
      os << " (" << formatDuration(durationFor(bar)) << ")";
    }

    if (useColor_) os << "\x1b[0m";
    return os.str();
  }

  Bar* findUnlocked(int id) {
    auto it = barsById_.find(id);
    if (it == barsById_.end()) return nullptr;
    return &it->second;
  }

  bool shouldRemoveCompletedUnlocked(const Bar& bar, Clock::time_point now) const {
    if (!bar.complete) return false;
    if (!options_.removeCompletedAfter.has_value()) return false;
    if (!bar.completedAt.has_value()) return false;
    return (now - *bar.completedAt) >= *options_.removeCompletedAfter;
  }

  void clearAllRenderedUnlocked() {
    if (!renderingEnabled_ || renderedLines_ <= 0) {
      renderedLines_ = 0;
      return;
    }

    std::cerr << "\x1b[" << renderedLines_ << "A";

    if (options_.deleteLinesOnClear) {
      for (int i = 0; i < renderedLines_; ++i) {
        std::cerr << "\x1b[1M";
      }
      std::cerr.flush();
      renderedLines_ = 0;
      return;
    }

    for (int i = 0; i < renderedLines_; ++i) {
      std::cerr << "\r\x1b[K";
      if (i + 1 < renderedLines_) std::cerr << "\n";
    }
    std::cerr << "\r";
    std::cerr.flush();

    renderedLines_ = 0;
  }

  void redrawUnlocked(bool force) {
    if (!renderingEnabled_) return;

    auto now = Clock::now();
    if (!force && options_.minRedrawInterval.count() > 0) {
      if (now - lastRedraw_ < options_.minRedrawInterval) return;
    }
    lastRedraw_ = now;

    if (options_.removeCompletedAfter.has_value()) {
      bool anyRemoved = false;
      for (std::size_t i = 0; i < order_.size();) {
        auto it = barsById_.find(order_[i]);
        if (it == barsById_.end()) {
          order_.erase(order_.begin() + static_cast<long>(i));
          anyRemoved = true;
          continue;
        }
        if (shouldRemoveCompletedUnlocked(it->second, now)) {
          barsById_.erase(it);
          order_.erase(order_.begin() + static_cast<long>(i));
          anyRemoved = true;
          continue;
        }
        ++i;
      }

      if (anyRemoved) {
        clearAllRenderedUnlocked();

        renderedLines_ = static_cast<int>(order_.size());
        for (int i = 0; i < renderedLines_; ++i) {
          std::cerr << "\n";
        }
      }
    }

    if (renderedLines_ <= 0) return;

    std::cerr << "\x1b[" << renderedLines_ << "A";
    for (int id : order_) {
      auto it = barsById_.find(id);
      if (it == barsById_.end()) continue;
      std::cerr << "\r\x1b[K" << formatLine(it->second) << "\n";
    }
    std::cerr.flush();
  }

  explicit Impl(Options options, std::optional<std::string> logFilePath)
      : options_(std::move(options)), logFilePath_(std::move(logFilePath)) {
    if (options_.onlyRenderOnTty && !isStderrTty()) renderingEnabled_ = false;
    if (options_.honorNoColorEnv && std::getenv("NO_COLOR") != nullptr) useColor_ = false;
    if (!options_.enabled) renderingEnabled_ = false;

    if (logFilePath_.has_value() && !logFilePath_->empty()) {
      logStream_.open(*logFilePath_, std::ios::out | std::ios::app);
      if (!logStream_) logFilePath_.reset();
    } else {
      logFilePath_.reset();
    }

    lastRedraw_ = Clock::now();
  }
};

ProgressBarsReal::ProgressBarsReal() : ProgressBarsReal(Options{}, std::nullopt) {}
ProgressBarsReal::ProgressBarsReal(std::string logFilePath) : ProgressBarsReal(Options{}, std::move(logFilePath)) {}
ProgressBarsReal::ProgressBarsReal(Options options) : ProgressBarsReal(std::move(options), std::nullopt) {}
ProgressBarsReal::ProgressBarsReal(Options options, std::optional<std::string> logFilePath)
    : impl_(std::make_unique<Impl>(std::move(options), std::move(logFilePath))) {}

ProgressBarsReal::~ProgressBarsReal() {
  try {
    std::lock_guard<std::mutex> lock(impl_->mu_);
    impl_->clearAllRenderedUnlocked();
  } catch (...) {
  }
}

void ProgressBarsReal::setEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(impl_->mu_);
  impl_->renderingEnabled_ = enabled && (!impl_->options_.onlyRenderOnTty || Impl::isStderrTty());
  if (!impl_->renderingEnabled_) return;
  impl_->redrawUnlocked(true);
}

bool ProgressBarsReal::isEnabled() const {
  std::lock_guard<std::mutex> lock(impl_->mu_);
  return impl_->renderingEnabled_;
}

int ProgressBarsReal::createProgressBar(std::uint64_t totalUnits, std::string label, Color color) {
  std::lock_guard<std::mutex> lock(impl_->mu_);

  const int id = impl_->nextId_++;
  Impl::Bar bar;
  bar.id = id;
  bar.total = totalUnits;
  bar.done = 0;
  bar.label = std::move(label);
  bar.color = color;
  bar.complete = false;
  bar.started = Impl::Clock::now();

  impl_->order_.push_back(id);
  impl_->barsById_.emplace(id, std::move(bar));

  if (impl_->renderingEnabled_) {
    std::cerr << "\n";
    impl_->renderedLines_++;
    impl_->redrawUnlocked(true);
  }

  return id;
}

int ProgressBarsReal::createProgressBar(std::string_view stringId, std::uint64_t totalUnits, std::string label,
                                       Color color) {
  std::lock_guard<std::mutex> lock(impl_->mu_);

  // If it already exists, just return the mapped int id.
  if (auto existing = impl_->lookupStringId(stringId); existing.has_value()) {
    return *existing;
  }

  // Create a new int-based progress bar.
  const int id = impl_->nextId_++;
  Impl::Bar bar;
  bar.id = id;
  bar.total = totalUnits;
  bar.done = 0;
  bar.label = std::move(label);
  bar.color = color;
  bar.complete = false;
  bar.started = Impl::Clock::now();

  impl_->order_.push_back(id);
  impl_->barsById_.emplace(id, std::move(bar));

  impl_->getOrCreateStringId(stringId, id);

  if (impl_->renderingEnabled_) {
    std::cerr << "\n";
    impl_->renderedLines_++;
    impl_->redrawUnlocked(true);
  }

  return id;
}

void ProgressBarsReal::updateProgressBar(int pbid, std::uint64_t completedUnits) {
  std::lock_guard<std::mutex> lock(impl_->mu_);
  auto* bar = impl_->findUnlocked(pbid);
  if (!bar) return;

  if (!bar->complete) {
    bar->done = (completedUnits > bar->total) ? bar->total : completedUnits;
  }
  impl_->redrawUnlocked(false);
}

void ProgressBarsReal::updateProgressBar(int pbid) {
  std::lock_guard<std::mutex> lock(impl_->mu_);
  auto* bar = impl_->findUnlocked(pbid);
  if (!bar) return;

  if (!bar->complete && bar->done < bar->total) bar->done++;
  impl_->redrawUnlocked(false);
}

void ProgressBarsReal::updateProgressBar(std::string_view stringId, std::uint64_t completedUnits) {
  std::lock_guard<std::mutex> lock(impl_->mu_);
  auto id = impl_->lookupStringId(stringId);
  if (!id.has_value()) return;

  auto* bar = impl_->findUnlocked(*id);
  if (!bar) return;

  if (!bar->complete) {
    bar->done = (completedUnits > bar->total) ? bar->total : completedUnits;
  }
  impl_->redrawUnlocked(false);
}

void ProgressBarsReal::updateProgressBar(std::string_view stringId) {
  std::lock_guard<std::mutex> lock(impl_->mu_);
  auto id = impl_->lookupStringId(stringId);
  if (!id.has_value()) return;

  auto* bar = impl_->findUnlocked(*id);
  if (!bar) return;

  if (!bar->complete && bar->done < bar->total) bar->done++;
  impl_->redrawUnlocked(false);
}

void ProgressBarsReal::markProgressBarComplete(int pbid) {
  std::lock_guard<std::mutex> lock(impl_->mu_);
  auto* bar = impl_->findUnlocked(pbid);
  if (!bar) return;

  if (!bar->complete) {
    bar->done = bar->total;
    bar->complete = true;
    bar->completedAt = Impl::Clock::now();

    if (impl_->logStream_) {
      impl_->logStream_ << bar->label << ": " << Impl::formatDuration(Impl::durationFor(*bar)) << "\n";
      impl_->logStream_.flush();
    }
  }

  impl_->redrawUnlocked(true);
}

void ProgressBarsReal::markProgressBarComplete(std::string_view stringId) {
  std::lock_guard<std::mutex> lock(impl_->mu_);
  auto id = impl_->lookupStringId(stringId);
  if (!id.has_value()) return;

  auto* bar = impl_->findUnlocked(*id);
  if (!bar) return;

  if (!bar->complete) {
    bar->done = bar->total;
    bar->complete = true;
    bar->completedAt = Impl::Clock::now();

    if (impl_->logStream_) {
      impl_->logStream_ << bar->label << ": " << Impl::formatDuration(Impl::durationFor(*bar)) << "\n";
      impl_->logStream_.flush();
    }
  }

  impl_->redrawUnlocked(true);
}

void ProgressBarsReal::redrawNow() {
  std::lock_guard<std::mutex> lock(impl_->mu_);
  impl_->redrawUnlocked(true);
}

} // namespace progressbar

#endif // PROGRESSBAR_ENABLED
