#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "mem-analyzer/collector.hpp"
#include <fstream>
#include <cstring>
#include <cerrno>

using namespace mem_analyzer;

// ── Utility tests ──
TEST_CASE("format_size works correctly", "[utility]") {
  CHECK(format_size(0) == "0 KB");
  CHECK(format_size(512) == "512 KB");
  CHECK(format_size(1024) == "1.00 MB");
  CHECK(format_size(2048) == "2.00 MB");

  auto gb1 = format_size(1024 * 1024);
  CHECK(gb1 == "1.00 GB");

  auto gb15 = format_size(1536 * 1024);
  CHECK(gb15 == "1.50 GB");
}

TEST_CASE("format_percent works correctly", "[utility]") {
  CHECK(format_percent(0.0) == "0.0%");
  CHECK(format_percent(50.5) == "50.5%");
  CHECK(format_percent(100.0) == "100.0%");

  auto pct = format_percent(99.94);
  CHECK(pct == "99.9%");
  
  auto pct2 = format_percent(99.99);
  CHECK(pct2 == "100.0%");
}

// ── MemInfo methods ──
TEST_CASE("MemInfo calculations are correct", "[meminfo]") {
  MemInfo info;
  info.mem_total_kb = 16 * 1024 * 1024;  // 16 GB
  info.mem_available_kb = 8 * 1024 * 1024;  // 8 GB
  info.swap_total_kb = 2 * 1024 * 1024;  // 2 GB
  info.swap_free_kb = 1 * 1024 * 1024;   // 1 GB

  auto used = info.used_percent();
  auto avail = info.available_percent();
  auto swap = info.swap_used_percent();

  CHECK(used == Catch::Approx(50.0).epsilon(0.01));
  CHECK(avail == Catch::Approx(50.0).epsilon(0.01));
  CHECK(swap == Catch::Approx(50.0).epsilon(0.01));
}

TEST_CASE("MemInfo edge cases", "[meminfo]") {
  MemInfo info;
  auto used1 = info.used_percent();
  auto avail1 = info.available_percent();
  auto swap1 = info.swap_used_percent();

  CHECK(used1 == Catch::Approx(0.0));
  CHECK(avail1 == Catch::Approx(0.0));
  CHECK(swap1 == Catch::Approx(0.0));

  info.mem_total_kb = 1024 * 1024;
  info.mem_available_kb = 1024 * 1024;
  info.swap_total_kb = 1024 * 1024;
  info.swap_free_kb = 1024 * 1024;

  auto used2 = info.used_percent();
  auto avail2 = info.available_percent();
  auto swap2 = info.swap_used_percent();

  CHECK(used2 == Catch::Approx(0.0));
  CHECK(avail2 == Catch::Approx(100.0));
  CHECK(swap2 == Catch::Approx(0.0));
}

TEST_CASE("MemInfo to_json output", "[meminfo]") {
  MemInfo info;
  info.mem_total_kb = 8 * 1024 * 1024;
  info.mem_available_kb = 4 * 1024 * 1024;

  auto j = info.to_json();
  CHECK(j["mem_total"] == 8 * 1024 * 1024);
  CHECK(j["mem_available"] == 4 * 1024 * 1024);

  auto pct = j["used_percent"].get<double>();
  CHECK(pct == Catch::Approx(50.0).epsilon(0.01));
}

// ── ProcMem tests ──
TEST_CASE("ProcMem to_json and to_table_row", "[procmem]") {
  ProcMem pm;
  pm.pid = 1234;
  pm.name = "test-process";
  pm.vm_rss_kb = 65536;
  pm.vm_size_kb = 262144;
  pm.vm_swap_kb = 4096;

  auto j = pm.to_json();
  CHECK(j["pid"] == 1234);
  CHECK(j["name"] == "test-process");
  CHECK(j["vm_rss"] == 65536);

  auto row = pm.to_table_row();
  auto found_pid = row.find("1234") != std::string::npos;
  auto found_name = row.find("test-process") != std::string::npos;
  CHECK(found_pid);
  CHECK(found_name);
}

// ── ProcFile reading (integration) ──
TEST_CASE("/proc/self/status is readable", "[integration]") {
  std::ifstream f("/proc/self/status");
  REQUIRE(f.is_open());

  std::string line;
  bool found_name = false;
  while (std::getline(f, line)) {
    if (line.starts_with("Name:")) found_name = true;
  }
  CHECK(found_name);
}

TEST_CASE("/proc/meminfo is readable", "[integration]") {
  std::ifstream f("/proc/meminfo");
  REQUIRE(f.is_open());

  std::string content;
  std::string line;
  bool found_mem_total = false;
  while (std::getline(f, line)) {
    content += line + "\n";
    if (line.starts_with("MemTotal:")) found_mem_total = true;
  }
  CHECK(found_mem_total);
  auto size_ok = content.size() > 100;
  CHECK(size_ok);
}

// ── Benchmark ──
TEST_CASE("format_size benchmark", "[benchmark]") {
  BENCHMARK("Format 0 KB") { return format_size(0); };
  BENCHMARK("Format 1 MB") { return format_size(1024); };
  BENCHMARK("Format 1 GB") { return format_size(1024 * 1024); };
}

TEST_CASE("to_json benchmark", "[benchmark]") {
  MemInfo info;
  info.mem_total_kb = 16 * 1024 * 1024;
  info.mem_available_kb = 8 * 1024 * 1024;
  info.swap_total_kb = 2 * 1024 * 1024;
  info.swap_free_kb = 1024 * 1024;
  info.buffers_kb = 256 * 1024;
  info.cached_kb = 3 * 1024 * 1024;

  BENCHMARK("MemInfo to_json") { return info.to_json(); };
}
