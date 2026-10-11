// ***************************************************************************************
// Copyright (c) 2023-2025 Peng Cheng Laboratory
// Copyright (c) 2023-2025 Institute of Computing Technology, Chinese Academy of Sciences
// Copyright (c) 2023-2025 Beijing Institute of Open Source Chip
//
// iEDA is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at:
// http://license.coscl.org.cn/MulanPSL2
//
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//
// See the Mulan PSL v2 for more details.
// ***************************************************************************************
#include "PeakPowerEngine.hpp"

#include "DataManager.hpp"
#include "Logger.hpp"
#include "Monitor.hpp"
#include "PowerAnalyzer.hpp"
#include "PowerPropagator.hpp"
#include "PWHeader.hpp"

namespace ipw {

namespace {
// 窗口时长由 tick 换算而来，留一点余量，避免浮点误差把完整窗判成截断窗。
constexpr double WINDOW_LENGTH_EPSILON = 1E-9;
}  // namespace

// public

void PeakPowerEngine::initInst()
{
  if (_pe_instance == nullptr) {
    _pe_instance = new PeakPowerEngine();
  }
}

PeakPowerEngine& PeakPowerEngine::getInst()
{
  if (_pe_instance == nullptr) {
    PWLOG.error(Loc::current(), "The instance not initialized!");
  }
  return *_pe_instance;
}

void PeakPowerEngine::destroyInst()
{
  if (_pe_instance != nullptr) {
    delete _pe_instance;
    _pe_instance = nullptr;
  }
}

PeakPowerEngine* PeakPowerEngine::_pe_instance = nullptr;

// function

void PeakPowerEngine::run(const std::string& vcd_file_path)
{
  Monitor monitor;
  PWLOG.info(Loc::current(), "Starting...");

  const double window_length = check(vcd_file_path);
  if (window_length <= 0.0) {
    return;
  }
  PWDM.getDatabase().get_peak_power_summary().set_is_enabled(true);

  vcd::VcdWindowReader window_reader;
  if (!window_reader.open(vcd_file_path, window_length)) {
    PWLOG.error(Loc::current(), "Failed to open VCD file '", vcd_file_path, "'!");
    return;
  }

  std::size_t window_num = 0;
  std::size_t truncated_window_num = 0;
  while (window_reader.advance_window()) {
    const vcd::VcdWindowSignalActivity& window_activity = window_reader.get_window_activity();
    // 波形末尾不足一窗的余量里可能挤进整周期的跳变（速率被抬高），不能参与峰值竞争。
    if (window_activity.get_window_duration() < window_length - WINDOW_LENGTH_EPSILON) {
      truncated_window_num++;
      continue;
    }
    PowerPropagator::annotateVcdActivity(window_activity);
    PWPP.propagate();
    PWPA.analyze();
    updatePeakPower(window_activity);
    window_num++;
  }
  if (truncated_window_num > 0) {
    PWLOG.info(Loc::current(), "Skipped ", truncated_window_num, " truncated window(s) shorter than the ", window_length,
               " ns analysis window.");
  }
  if (window_num == 0) {
    PWLOG.error(Loc::current(), "The VCD holds no complete ", window_length, " ns window; check the waveform!");
    return;
  }

  const PeakPowerValue& total_peak_value = PWDM.getDatabase().get_peak_power_summary().get_total_peak_value();
  PWLOG.info(Loc::current(), "Analyzed ", window_num, " window(s): peak power ", total_peak_value.get_power(), " W at ",
             total_peak_value.get_begin_time(), " ns - ", total_peak_value.get_end_time(), " ns");
  PWLOG.info(Loc::current(), "Completed", monitor.getStatsInfo());
}

// private

double PeakPowerEngine::check(const std::string& vcd_file_path)
{
  if (vcd_file_path.empty()) {
    PWLOG.error(Loc::current(), "The VCD file path is empty!");
    return 0.0;
  }
  const double window_length = resolveWindowLength();
  if (window_length <= 0.0) {
    PWLOG.error(Loc::current(), "No reference clock found for peak power analysis!");
    return 0.0;
  }
  return window_length;
}

double PeakPowerEngine::resolveWindowLength()
{
  std::map<std::string, TimingClock>& clock_map = PWDM.getDatabase().get_timing_constraint().get_clock_map();
  double window_length = 0.0;
  std::string reference_clock_name;
  for (std::pair<const std::string, TimingClock>& clock_pair : clock_map) {
    const double clock_period = clock_pair.second.get_period();
    PWLOG.info(Loc::current(), "Clock '", clock_pair.first, "' period = ", clock_period, " ns");
    if (clock_period <= 0.0) {
      continue;
    }
    if (window_length <= 0.0 || clock_period < window_length) {
      window_length = clock_period;
      reference_clock_name = clock_pair.first;
    }
  }
  if (window_length > 0.0) {
    PWLOG.info(Loc::current(), "Reference clock '", reference_clock_name, "' period ", window_length,
               " ns is used as the peak analysis window.");
  }
  return window_length;
}

void PeakPowerEngine::updatePeakPower(const vcd::VcdWindowSignalActivity& window_activity)
{
  Database& database = PWDM.getDatabase();
  const double window_begin_time = window_activity.get_window_begin_time();
  const double window_end_time = window_activity.get_window_end_time();
  const std::size_t window_index = window_activity.get_window_index();

  PeakPowerSummary& peak_power_summary = database.get_peak_power_summary();
  peak_power_summary.get_total_peak_value().update(database.get_power_summary().get_total_power_value(), window_begin_time,
                                                   window_end_time, window_index);
  for (std::pair<const std::string, InstancePower>& instance_power_pair : database.get_instance_power_map()) {
    peak_power_summary.get_instance_peak_value_map()[instance_power_pair.first].update(instance_power_pair.second.get_power_value(),
                                                                                       window_begin_time, window_end_time,
                                                                                       window_index);
  }
}

}  // namespace ipw
