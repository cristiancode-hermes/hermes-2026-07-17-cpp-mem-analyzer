#include "mem-analyzer/collector.hpp"
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include <thread>

using namespace mem_analyzer;

void cmd_summary(const MemCollector& collector) {
  auto info = collector.read_meminfo();
  if (!info) {
    std::println(std::cerr, "Error: {}", info.error().message);
    return;
  }

  const auto& m = info.value();
  std::println(std::cout, "┌────────────────────────────────────────────┐");
  std::println(std::cout, "│         Memory Analyzer — Summary          │");
  std::println(std::cout, "├────────────────────────────────────────────┤");
  std::println(std::cout, "│ Total:     {:>12}  {:>10} │", format_size(m.mem_total_kb), format_percent(100.0));
  std::println(std::cout, "│ Available: {:>12}  {:>10} │", format_size(m.mem_available_kb), format_percent(m.available_percent()));
  std::println(std::cout, "│ Used:      {:>12}  {:>10} │", format_size(m.mem_total_kb - m.mem_available_kb), format_percent(m.used_percent()));
  std::println(std::cout, "│ Buffers:   {:>12}             │", format_size(m.buffers_kb));
  std::println(std::cout, "│ Cached:    {:>12}             │", format_size(m.cached_kb));
  std::println(std::cout, "├────────────────────────────────────────────┤");
  std::println(std::cout, "│ Swap:      {:>12}  {:>10} │", format_size(m.swap_total_kb), format_percent(100.0));
  std::println(std::cout, "│ Swap Free: {:>12}  {:>10} │", format_size(m.swap_free_kb), format_percent(100.0 - m.swap_used_percent()));
  std::println(std::cout, "│ Swap Used: {:>12}  {:>10} │", format_size(m.swap_total_kb - m.swap_free_kb), format_percent(m.swap_used_percent()));
  std::println(std::cout, "├────────────────────────────────────────────┤");
  std::println(std::cout, "│ Active:    {:>12}    │ Anon: {:>10} │", format_size(m.active_kb), format_size(m.anon_pages_kb));
  std::println(std::cout, "│ Inactive:  {:>12}    │ Mapped: {:>8} │", format_size(m.inactive_kb), format_size(m.mapped_kb));
  std::println(std::cout, "│ Dirty:     {:>12}    │ Slab: {:>10} │", format_size(m.dirty_kb), format_size(m.slab_kb));
  std::println(std::cout, "└────────────────────────────────────────────┘");
}

void cmd_compact(const MemCollector& collector) {
  auto info = collector.read_meminfo();
  if (!info) {
    std::println(std::cerr, "Error: {}", info.error().message);
    return;
  }

  const auto& m = info.value();
  std::println(std::cout, "MEM {:.1f}% used | {} / {} | Swap {:.1f}% | Cache {} | {} procs",
    m.used_percent(),
    format_size(m.mem_total_kb - m.mem_available_kb),
    format_size(m.mem_total_kb),
    m.swap_used_percent(),
    format_size(m.cached_kb),
    "—");
}

void cmd_json(const MemCollector& collector) {
  auto snap = collector.snapshot();
  if (!snap) {
    std::println(std::cerr, "Error: {}", snap.error().message);
    return;
  }
  std::println(std::cout, "{}", snap->to_json().dump(2));
}

void cmd_top(const MemCollector& collector, int count) {
  auto top = collector.top_processes(count);
  if (!top) {
    std::println(std::cerr, "Error: {}", top.error().message);
    return;
  }

  std::println(std::cout, "┌─────────────────────────────────────────────────────────────────┐");
  std::println(std::cout, "│  Top {} processes by RSS                         Total: {:>8} │",
    top->processes.size(), format_size(top->total_rss_kb));
  std::println(std::cout, "├───────┬──────────────────────┬──────────┬──────────┬──────────┬──────┤");
  std::println(std::cout, "│  PID  │ Name                 │     RSS  │    Size  │   Swap   │State │");
  std::println(std::cout, "├───────┼──────────────────────┼──────────┼──────────┼──────────┼──────┤");
  for (const auto& p : top->processes) {
    // Split table_row into separate prints to avoid 9+ argument issue
    std::println(std::cout, "│ {:>5} │ {:<20} │ {:>8} │ {:>8} │ {:>8} │ {:<4} │",
      p.pid, p.name, format_size(p.vm_rss_kb), format_size(p.vm_size_kb),
      format_size(p.vm_swap_kb), p.state.substr(0, 4));
  }
  std::println(std::cout, "└───────┴──────────────────────┴──────────┴──────────┴──────────┴──────┘");
}

void cmd_monitor(const MemCollector& collector, int interval, int count) {
  std::println(std::cout, "Memory Monitor — refreshing every {}s ({} times)", interval, count);
  std::println(std::cout, "Press Ctrl+C to stop");
  std::println(std::cout, "");

  for (int i = 0; i < count; ++i) {
    auto info = collector.read_meminfo();
    if (!info) {
      std::println(std::cerr, "Error: {}", info.error().message);
      break;
    }

    const auto& m = info.value();
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&tt));

    // Memory pressure bar
    double pressure = m.used_percent();
    int bar_width = 30;
    int filled = static_cast<int>(pressure / 100.0 * bar_width);

    std::println(std::cout, "[{}] {:>5.1f}% ┃{}{}┃ {} / {}  ({} procs)",
      buf, pressure,
      std::string(filled, '█'),
      std::string(bar_width - filled, '░'),
      format_size(m.mem_total_kb - m.mem_available_kb),
      format_size(m.mem_total_kb),
      "—");

    if (i < count - 1) {
      std::this_thread::sleep_for(std::chrono::seconds(interval));
    }
  }
}

int main(int argc, char** argv) {
  CLI::App app{"Memory Analyzer — Linux /proc memory monitoring tool"};

  std::string command = "summary";
  app.add_option("command", command, "Command: summary, compact, json, top, monitor")
    ->default_val("summary");

  int top_count = 10;
  app.add_option("-n,--count", top_count, "Number of top processes (default: 10)")
    ->default_val(10);

  int interval = 2;
  app.add_option("-i,--interval", interval, "Monitor interval in seconds (default: 2)")
    ->default_val(2);

  int iterations = 30;
  app.add_option("-t,--iterations", iterations, "Monitor iterations (default: 30)")
    ->default_val(30);

  CLI11_PARSE(app, argc, argv);

  // Set up logger
  spdlog::set_level(spdlog::level::warn);

  MemCollector collector;

  if (command == "summary" || command == "sum") {
    cmd_summary(collector);
  } else if (command == "compact") {
    cmd_compact(collector);
  } else if (command == "json") {
    cmd_json(collector);
  } else if (command == "top") {
    cmd_top(collector, top_count);
  } else if (command == "monitor" || command == "mon") {
    cmd_monitor(collector, interval, iterations);
  } else {
    std::println(std::cerr, "Unknown command: {}", command);
    std::println(std::cerr, "Available: summary, compact, json, top, monitor");
    return 1;
  }

  return 0;
}
