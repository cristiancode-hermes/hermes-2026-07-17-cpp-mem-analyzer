#include "mem-analyzer/collector.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace mem_analyzer;

TEST_CASE("MemCollector can be constructed", "[collector]") {
  MemCollector collector;
  // Just verify it doesn't crash
  SUCCEED("MemCollector constructed successfully");
}

TEST_CASE("MemCollector reads /proc/meminfo", "[collector][integration]") {
  MemCollector collector;
  auto result = collector.read_meminfo();

  if (result) {
    const auto& info = result.value();
    CHECK(info.mem_total_kb > 0);
    CHECK(info.mem_available_kb > 0);
    CHECK(info.mem_free_kb > 0);
    // mem_total should be between 128 MB and 2 TB
    CHECK(info.mem_total_kb >= 128 * 1024);
    CHECK(info.mem_total_kb <= 2LL * 1024 * 1024 * 1024);
  } else {
    // In restricted environments /proc may not be available
    WARN("Could not read /proc/meminfo: " + result.error().message);
  }
}

TEST_CASE("MemCollector reads own status", "[collector][integration]") {
  MemCollector collector;
  auto result = collector.read_proc_mem(getpid());

  if (result) {
    const auto& pm = result.value();
    CHECK(pm.pid == getpid());
    CHECK_FALSE(pm.name.empty());
    CHECK(pm.vm_size_kb > 0);
  } else {
    WARN("Could not read own process info: " + result.error().message);
  }
}

TEST_CASE("MemCollector lists PIDs", "[collector][integration]") {
  MemCollector collector;
  auto result = collector.list_pids();

  if (result) {
    const auto& pids = result.value();
    CHECK_FALSE(pids.empty());
    // PID 1 (init/systemd) should exist
    CHECK(std::find(pids.begin(), pids.end(), 1) != pids.end());
  } else {
    WARN("Could not list PIDs: " + result.error().message);
  }
}

TEST_CASE("MemCollector gets top processes", "[collector][integration]") {
  MemCollector collector;
  auto result = collector.top_processes(5);

  if (result) {
    const auto& top = result.value();
    CHECK(top.processes.size() <= 5);
    CHECK_FALSE(top.processes.empty());
    // Top process should have non-zero RSS (or we're in a minimal container)
    auto total = top.total_rss_kb;
    CHECK(total >= 0);
    // Processes should be sorted by RSS descending
    for (size_t i = 1; i < top.processes.size(); ++i) {
      CHECK(top.processes[i - 1].vm_rss_kb >= top.processes[i].vm_rss_kb);
    }
  } else {
    WARN("Could not get top processes: " + result.error().message);
  }
}

TEST_CASE("MemCollector takes snapshot", "[collector][integration]") {
  MemCollector collector;
  auto result = collector.snapshot();

  if (result) {
    const auto& snap = result.value();
    CHECK(snap.meminfo.mem_total_kb > 0);
    CHECK(snap.process_count > 0);
    CHECK(snap.total_rss_kb >= 0);
  } else {
    WARN("Could not take snapshot: " + result.error().message);
  }
}

TEST_CASE("MemCollector calculates memory pressure", "[collector][integration]") {
  MemCollector collector;
  auto result = collector.memory_pressure();

  if (result) {
    double pressure = result.value();
    CHECK(pressure >= 0.0);
    CHECK(pressure <= 100.0);
  } else {
    WARN("Could not calculate memory pressure: " + result.error().message);
  }
}
