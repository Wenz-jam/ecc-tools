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

class PeakPowerSummary
{
 public:
  PeakPowerSummary() = default;
  ~PeakPowerSummary() = default;
  // getter
  bool get_is_enabled() const { return _is_enabled; }
  PeakPowerValue& get_total_peak_value() { return _total_peak_value; }
  std::map<std::string, PeakPowerValue>& get_instance_peak_value_map() { return _instance_peak_value_map; }
  const PeakPowerValue& get_total_peak_value() const { return _total_peak_value; }
  const std::map<std::string, PeakPowerValue>& get_instance_peak_value_map() const { return _instance_peak_value_map; }
  // setter
  void set_is_enabled(const bool is_enabled) { _is_enabled = is_enabled; }
  // function

 private:
  bool _is_enabled = false;
  PeakPowerValue _total_peak_value;
  std::map<std::string, PeakPowerValue> _instance_peak_value_map;
};

}  // namespace ipw
