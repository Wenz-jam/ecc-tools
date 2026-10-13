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
#pragma once

#include "PeakPowerValue.hpp"
#include "PWHeader.hpp"

namespace ipw {

struct PeakPowerWindow
{
  std::size_t window_index = 0;
  double begin_time = 0.0;
  double end_time = 0.0;
  double power = 0.0;
};

class PeakPowerSummary
{
 public:
  PeakPowerSummary() = default;
  ~PeakPowerSummary() = default;
  // getter
  bool get_is_enabled() const { return _is_enabled; }
  PeakPowerValue& get_total_peak_value() { return _total_peak_value; }
  std::map<std::string, PeakPowerValue>& get_instance_peak_value_map() { return _instance_peak_value_map; }
  std::vector<PeakPowerWindow>& get_window_list() { return _window_list; }
  const PeakPowerValue& get_total_peak_value() const { return _total_peak_value; }
  const std::map<std::string, PeakPowerValue>& get_instance_peak_value_map() const { return _instance_peak_value_map; }
  const std::vector<PeakPowerWindow>& get_window_list() const { return _window_list; }
  // setter
  void set_is_enabled(const bool is_enabled) { _is_enabled = is_enabled; }
  // function
  // 复位峰值结果，重复运行前调用；峰值与窗口列表必须同时清，否则导出会对不上。
  void reset()
  {
    _is_enabled = false;
    _total_peak_value = PeakPowerValue();
    _instance_peak_value_map.clear();
    _window_list.clear();
  }

 private:
  bool _is_enabled = false;
  PeakPowerValue _total_peak_value;
  std::map<std::string, PeakPowerValue> _instance_peak_value_map;
  std::vector<PeakPowerWindow> _window_list;
};

}  // namespace ipw
