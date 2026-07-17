#include "mem-analyzer/collector.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cstring>
#include <cerrno>
#include <cctype>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace mem_analyzer {

MemCollector::MemCollector() = default;

std::string MemCollector::read_proc_file(const std::string& path) const {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error(std::format("Cannot open {}: {}", path, std::strerror(errno)));
  }
  std::stringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

std::vector<std::string> MemCollector::split_lines(const std::string& data) const {
  std::vector<std::string> lines;
  std::stringstream ss(data);
  std::string line;
  while (std::getline(ss, line)) {
    lines.push_back(line);
  }
  return lines;
}

Result<MemInfo> MemCollector::read_meminfo() const {
  try {
    auto data = read_proc_file("/proc/meminfo");
    auto lines = split_lines(data);
    MemInfo info;

    for (const auto& line : lines) {
      auto colon_pos = line.find(':');
      if (colon_pos == std::string::npos) continue;

      auto key = line.substr(0, colon_pos);
      auto val_str = line.substr(colon_pos + 1);
      // Trim spaces
      val_str.erase(0, val_str.find_first_not_of(" \t"));
      // Remove " kB" suffix
      auto kb_pos = val_str.rfind(" kB");
      if (kb_pos != std::string::npos) val_str = val_str.substr(0, kb_pos);
      // Also handle empty or "(null)" values
      if (val_str.empty() || val_str == "(null)") continue;

      uint64_t val = 0;
      try { val = std::stoull(val_str); } catch (...) { continue; }

      if (key == "MemTotal")         info.mem_total_kb = val;
      else if (key == "MemFree")     info.mem_free_kb = val;
      else if (key == "MemAvailable") info.mem_available_kb = val;
      else if (key == "Buffers")     info.buffers_kb = val;
      else if (key == "Cached")      info.cached_kb = val;
      else if (key == "SwapTotal")   info.swap_total_kb = val;
      else if (key == "SwapFree")    info.swap_free_kb = val;
      else if (key == "SwapCached")  info.swap_cached_kb = val;
      else if (key == "Active")      info.active_kb = val;
      else if (key == "Inactive")    info.inactive_kb = val;
      else if (key == "Dirty")       info.dirty_kb = val;
      else if (key == "Writeback")   info.writeback_kb = val;
      else if (key == "AnonPages")   info.anon_pages_kb = val;
      else if (key == "Mapped")      info.mapped_kb = val;
      else if (key == "Slab")        info.slab_kb = val;
      else if (key == "SReclaimable") info.reclaimable_kb = val;
      else if (key == "VmallocUsed") info.vmalloc_used_kb = val;
      else if (key == "PageTables")  info.page_tables_kb = val;
      else if (key == "KernelStack") info.kernel_stack_kb = val;
      else if (key == "HugePages_Total") info.huge_pages_total = val;
      else if (key == "HugePages_Free")  info.huge_pages_free = val;
      else if (key == "Hugepagesize")    info.huge_page_size_kb = val;
    }

    return info;
  } catch (const std::exception& e) {
    return std::unexpected(Error{500, std::format("Failed to read /proc/meminfo: {}", e.what())});
  }
}

Result<ProcMem> MemCollector::read_proc_mem(int pid) const {
  try {
    auto path = std::format("/proc/{}/status", pid);
    auto data = read_proc_file(path);
    auto lines = split_lines(data);
    ProcMem pm;
    pm.pid = pid;

    for (const auto& line : lines) {
      auto colon_pos = line.find(':');
      if (colon_pos == std::string::npos) continue;

      auto key = line.substr(0, colon_pos);
      auto val_str = line.substr(colon_pos + 1);
      val_str.erase(0, val_str.find_first_not_of(" \t"));

      if (key == "Name")         pm.name = val_str;
      else if (key == "State")   pm.state = val_str;
    }

    // Parse memory fields (VmPeak, VmSize, VmRSS, etc.)
    for (const auto& line : lines) {
      auto colon_pos = line.find(':');
      if (colon_pos == std::string::npos) continue;

      auto key = line.substr(0, colon_pos);
      auto val_str = line.substr(colon_pos + 1);
      val_str.erase(0, val_str.find_first_not_of(" \t"));

      // Extract numeric value and unit
      auto space_pos = val_str.rfind(' ');
      uint64_t val = 0;
      std::string unit = "kB";
      if (space_pos != std::string::npos) {
        try { val = std::stoull(val_str.substr(0, space_pos)); } catch (...) { continue; }
        unit = val_str.substr(space_pos + 1);
      } else {
        // Some fields like VmFlags don't have numeric values
        continue;
      }

      // Convert to kB
      if (unit == "MB") val *= 1024;
      else if (unit == "GB") val *= 1024 * 1024;
      else if (unit != "kB") continue;

      if (key == "VmPeak")   pm.vm_peak_kb = val;
      else if (key == "VmSize")  pm.vm_size_kb = val;
      else if (key == "VmRSS")   pm.vm_rss_kb = val;
      else if (key == "VmData")  pm.vm_data_kb = val;
      else if (key == "VmStk")   pm.vm_stk_kb = val;
      else if (key == "VmExe")   pm.vm_exe_kb = val;
      else if (key == "VmLib")   pm.vm_lib_kb = val;
      else if (key == "VmPTE")   pm.vm_pte_kb = val;
      else if (key == "VmPMD")   pm.vm_pmd_kb = val;
      else if (key == "VmSwap")  pm.vm_swap_kb = val;
      else if (key == "HugetlbPages") pm.hugetlb_kb = val;
    }

    return pm;
  } catch (const std::exception& e) {
    return std::unexpected(Error{500, std::format("Failed to read PID {}: {}", pid, e.what())});
  }
}

Result<std::vector<int>> MemCollector::list_pids() const {
  std::vector<int> pids;
  try {
    for (const auto& entry : fs::directory_iterator("/proc")) {
      if (!entry.is_directory()) continue;
      auto filename = entry.path().filename().string();
      if (std::all_of(filename.begin(), filename.end(), ::isdigit)) {
        pids.push_back(std::stoi(filename));
      }
    }
  } catch (const std::exception& e) {
    return std::unexpected(Error{500, std::format("Failed to list PIDs: {}", e.what())});
  }
  std::sort(pids.begin(), pids.end());
  return pids;
}

Result<TopProcesses> MemCollector::top_processes(int count) const {
  auto pid_result = list_pids();
  if (!pid_result) return std::unexpected(pid_result.error());

  std::vector<ProcMem> procs;
  for (int pid : pid_result.value()) {
    auto proc_result = read_proc_mem(pid);
    if (proc_result) {
      procs.push_back(std::move(proc_result.value()));
    }
  }

  // Sort by RSS descending
  std::sort(procs.begin(), procs.end(), [](const ProcMem& a, const ProcMem& b) {
    return a.vm_rss_kb > b.vm_rss_kb;
  });

  if (static_cast<int>(procs.size()) > count) {
    procs.resize(count);
  }

  TopProcesses tp;
  tp.processes = std::move(procs);
  tp.total_rss_kb = 0;
  for (const auto& p : tp.processes) {
    tp.total_rss_kb += p.vm_rss_kb;
  }

  return tp;
}

Result<MemSnapshot> MemCollector::snapshot() const {
  auto meminfo = read_meminfo();
  if (!meminfo) return std::unexpected(meminfo.error());

  auto pids = list_pids();
  if (!pids) return std::unexpected(pids.error());

  uint64_t total_rss = 0;
  for (int pid : pids.value()) {
    auto proc = read_proc_mem(pid);
    if (proc) total_rss += proc->vm_rss_kb;
  }

  MemSnapshot snap;
  snap.timestamp = std::chrono::system_clock::now();
  snap.meminfo = std::move(meminfo.value());
  snap.total_rss_kb = total_rss;
  snap.process_count = pids->size();
  return snap;
}

Result<double> MemCollector::memory_pressure() const {
  auto info = read_meminfo();
  if (!info) return std::unexpected(info.error());

  if (info->mem_total_kb == 0) return 0.0;

  // Memory pressure: what percent of usable RAM is consumed
  // Uses available memory vs total as the indicator
  uint64_t used = info->mem_total_kb - info->mem_available_kb;
  return 100.0 * static_cast<double>(used) / static_cast<double>(info->mem_total_kb);
}

// ── MemInfo methods ──
double MemInfo::used_percent() const {
  if (mem_total_kb == 0) return 0.0;
  return 100.0 * (1.0 - static_cast<double>(mem_available_kb) / static_cast<double>(mem_total_kb));
}

double MemInfo::available_percent() const {
  if (mem_total_kb == 0) return 0.0;
  return 100.0 * static_cast<double>(mem_available_kb) / static_cast<double>(mem_total_kb);
}

double MemInfo::swap_used_percent() const {
  if (swap_total_kb == 0) return 0.0;
  return 100.0 * (1.0 - static_cast<double>(swap_free_kb) / static_cast<double>(swap_total_kb));
}

json MemInfo::to_json() const {
  return json{
    {"mem_total", mem_total_kb},
    {"mem_free", mem_free_kb},
    {"mem_available", mem_available_kb},
    {"used_percent", used_percent()},
    {"available_percent", available_percent()},
    {"buffers", buffers_kb},
    {"cached", cached_kb},
    {"swap_total", swap_total_kb},
    {"swap_free", swap_free_kb},
    {"swap_used_percent", swap_used_percent()},
    {"active", active_kb},
    {"inactive", inactive_kb},
    {"dirty", dirty_kb},
    {"anon_pages", anon_pages_kb},
    {"mapped", mapped_kb},
    {"slab", slab_kb},
    {"reclaimable", reclaimable_kb},
    {"vmalloc_used", vmalloc_used_kb},
    {"page_tables", page_tables_kb},
    {"kernel_stack", kernel_stack_kb},
  };
}

// ── ProcMem methods ──
json ProcMem::to_json() const {
  return json{
    {"pid", pid},
    {"name", name},
    {"state", state},
    {"vm_peak", vm_peak_kb},
    {"vm_size", vm_size_kb},
    {"vm_rss", vm_rss_kb},
    {"vm_data", vm_data_kb},
    {"vm_stk", vm_stk_kb},
    {"vm_exe", vm_exe_kb},
    {"vm_lib", vm_lib_kb},
    {"vm_pte", vm_pte_kb},
    {"vm_swap", vm_swap_kb},
  };
}

std::string ProcMem::to_table_row() const {
  return std::format("{:>6}  {:<20}  {:>8}  {:>8}  {:>8}  {}",
    pid, name, format_size(vm_rss_kb), format_size(vm_size_kb),
    format_size(vm_swap_kb), state);
}

// ── MemSnapshot methods ──
json MemSnapshot::to_json() const {
  auto tt = std::chrono::system_clock::to_time_t(timestamp);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&tt));

  return json{
    {"timestamp", buf},
    {"meminfo", meminfo.to_json()},
    {"total_rss_gb", format_size(total_rss_kb)},
    {"total_rss_kb", total_rss_kb},
    {"process_count", process_count},
    {"memory_pressure", meminfo.used_percent()},
  };
}

// ── TopProcesses methods ──
json TopProcesses::to_json() const {
  json arr = json::array();
  for (const auto& p : processes) {
    arr.push_back(p.to_json());
  }
  return json{
    {"processes", std::move(arr)},
    {"total_rss_kb", total_rss_kb},
    {"total_rss", format_size(total_rss_kb)},
    {"count", processes.size()},
  };
}

// ── Utility ──
std::string format_size(uint64_t kb) {
  if (kb >= 1024 * 1024) {
    return std::format("{:.2f} GB", static_cast<double>(kb) / (1024.0 * 1024.0));
  } else if (kb >= 1024) {
    return std::format("{:.2f} MB", static_cast<double>(kb) / 1024.0);
  }
  return std::format("{} KB", kb);
}

std::string format_percent(double pct) {
  return std::format("{:.1f}%", pct);
}

} // namespace mem_analyzer
