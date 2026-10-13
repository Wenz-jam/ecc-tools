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

#include <unordered_map>

#include "Arc.hpp"
#include "Instance.hpp"
#include "InstancePower.hpp"
#include "Net.hpp"
#include "NetPower.hpp"
#include "ParasiticNet.hpp"
#include "PeakPowerSummary.hpp"
#include "Pin.hpp"
#include "PowerActivity.hpp"
#include "PowerSummary.hpp"
#include "PWHeader.hpp"
#include "TimingConstraint.hpp"
#include "TimingLibrary.hpp"
#include "TimingPoint.hpp"

namespace ipw {

class Database
{
 public:
  // Per-window analysis state. One instance is the main workspace, extra ones are handed to the
  // peak engine workers so that every worker writes its own activity, power and peak maps.
  struct PowerWorkspace
  {
    std::map<std::string, PowerActivity> vcd_activity_map;
    std::map<std::string, PowerActivity> power_activity_map;
    std::map<std::string, InstancePower> instance_power_map;
    std::map<std::string, NetPower> net_power_map;
    PowerSummary power_summary;
    PeakPowerSummary peak_power_summary;
  };

  Database() = default;
  ~Database() = default;
  // getter
  std::string& get_design_name() { return _design_name; }
  std::map<std::string, Instance>& get_instance_map() { return _instance_map; }
  std::map<std::string, Pin>& get_pin_map() { return _pin_map; }
  // Precomputed by GraphBuilder: does the pin drive an output port of its
  // timing cell? Power propagation probes this once per signal, so the answer
  // is cached here instead of re-walking pin/instance/cell maps every time.
  std::unordered_map<std::string, bool>& get_pin_is_output_map() { return _pin_is_output_map; }
  std::map<std::string, Net>& get_net_map() { return _net_map; }
  std::vector<Arc>& get_arc_list() { return _arc_list; }
  std::map<std::string, std::vector<std::size_t>>& get_outgoing_arc_list_map() { return _outgoing_arc_list_map; }
  std::map<std::string, std::vector<std::size_t>>& get_incoming_arc_list_map() { return _incoming_arc_list_map; }
  std::vector<std::string>& get_source_pin_list() { return _source_pin_list; }
  std::vector<std::string>& get_signal_order_list() { return _signal_order_list; }
  std::map<std::string, TimingPoint>& get_timing_point_map() { return _timing_point_map; }
  PowerWorkspace& get_workspace() { return currentWorkspace() != nullptr ? *currentWorkspace() : _workspace; }
  std::map<std::string, PowerActivity>& get_vcd_activity_map() { return get_workspace().vcd_activity_map; }
  std::map<std::string, PowerActivity>& get_power_activity_map() { return get_workspace().power_activity_map; }
  std::map<std::string, InstancePower>& get_instance_power_map() { return get_workspace().instance_power_map; }
  std::map<std::string, NetPower>& get_net_power_map() { return get_workspace().net_power_map; }
  PowerSummary& get_power_summary() { return get_workspace().power_summary; }
  PeakPowerSummary& get_peak_power_summary() { return get_workspace().peak_power_summary; }
  TimingLibrary& get_timing_library() { return _timing_library; }
  std::map<std::string, ParasiticNet>& get_parasitic_net_map() { return _parasitic_net_map; }
  TimingConstraint& get_timing_constraint() { return _timing_constraint; }
  // setter
  void set_design_name(const std::string& design_name) { _design_name = design_name; }
  void set_workspace(PowerWorkspace* workspace) { currentWorkspace() = workspace; }
  // function

 private:
  static PowerWorkspace*& currentWorkspace()
  {
    static thread_local PowerWorkspace* workspace = nullptr;
    return workspace;
  }

  std::string _design_name;
  std::map<std::string, Instance> _instance_map;
  std::map<std::string, Pin> _pin_map;
  std::unordered_map<std::string, bool> _pin_is_output_map;
  std::map<std::string, Net> _net_map;
  std::vector<Arc> _arc_list;
  std::map<std::string, std::vector<std::size_t>> _outgoing_arc_list_map;
  std::map<std::string, std::vector<std::size_t>> _incoming_arc_list_map;
  std::vector<std::string> _source_pin_list;
  std::vector<std::string> _signal_order_list;
  std::map<std::string, TimingPoint> _timing_point_map;
  PowerWorkspace _workspace;
  TimingLibrary _timing_library;
  std::map<std::string, ParasiticNet> _parasitic_net_map;
  TimingConstraint _timing_constraint;
};

}  // namespace ipw
