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
// WHETHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.
// ***************************************************************************************
#include "OutputLoadEstimator.hpp"

#include "DelayCache.hpp"

#include "Net.hpp"
#include "RcTreeApprox.hpp"
#include "TimingConstraint.hpp"

#include "DataManager.hpp"
#include "Database.hpp"
#include "Instance.hpp"
#include "ParasiticNet.hpp"
#include "ParasiticNodeName.hpp"
#include "Pin.hpp"
#include "PWHeader.hpp"
#include "TimingCell.hpp"
#include "TimingCellPort.hpp"
#include "TimingConstraint.hpp"
#include "TimingLibrary.hpp"
#include "TimingPortConstraint.hpp"

namespace ipw {

double getPinCapacitance(std::string& pin_name, AnalysisType analysis_type, TransType trans_type)
{
  Database& database = PWDM.getDatabase();
  auto pin_iter = database.get_pin_map().find(pin_name);
  if (pin_iter == database.get_pin_map().end()) {
    return 0.0;
  }
  Pin& pin = pin_iter->second;
  if (pin.get_is_port()) {
    std::map<std::string, TimingPortConstraint>& port_constraint_map
        = database.get_timing_constraint().get_port_constraint_map();
    auto port_constraint_iter = port_constraint_map.find(pin_name);
    if (port_constraint_iter != port_constraint_map.end() && port_constraint_iter->second.get_has_load()) {
      return port_constraint_iter->second.get_load(analysis_type, trans_type);
    }
    return 0.0;
  }
  auto instance_iter = database.get_instance_map().find(pin.get_instance_name());
  if (instance_iter == database.get_instance_map().end()) {
    return 0.0;
  }
  Instance& instance = instance_iter->second;
  std::map<std::string, TimingCell>& timing_cell_map = database.get_timing_library().get_cell_map();
  auto timing_cell_iter = timing_cell_map.find(instance.get_cell_name());
  if (timing_cell_iter == timing_cell_map.end()) {
    return 0.0;
  }
  TimingCell& timing_cell = timing_cell_iter->second;
  auto port_iter = timing_cell.get_port_map().find(pin.get_pin_name());
  if (port_iter == timing_cell.get_port_map().end()) {
    return 0.0;
  }
  TimingCellPort& timing_cell_port = port_iter->second;
  auto capacitance_iter = timing_cell_port.get_trans_capacitance_map().find(analysis_type);
  if (capacitance_iter != timing_cell_port.get_trans_capacitance_map().end()) {
    auto trans_iter = capacitance_iter->second.find(trans_type);
    if (trans_iter != capacitance_iter->second.end()) {
      return trans_iter->second;
    }
  }
  return timing_cell_port.get_capacitance();
}

double getParasiticNodeLoad(
    ParasiticNet& parasitic_net,
    std::string& node_name,
    AnalysisType analysis_type,
    TransType trans_type
)
{
  double node_load = 0.0;
  auto capacitance_iter = parasitic_net.get_node_capacitance_map().find(node_name);
  if (capacitance_iter != parasitic_net.get_node_capacitance_map().end()) {
    node_load += capacitance_iter->second;
  }

  // getPinCapacitance() returns 0.0 for unknown pins, so the former pin_map
  // membership probe here was a third lookup of the same key.
  std::string pin_name = getPinNameByParasiticNodeName(node_name);
  node_load += getPinCapacitance(pin_name, analysis_type, trans_type);
  return node_load;
}

double getArcOutputLoad(Arc& arc, AnalysisType analysis_type, TransType output_trans_type)
{
  Database& database = PWDM.getDatabase();
  std::string& sink_pin_name = arc.get_sink_pin();
  auto sink_pin_iter = database.get_pin_map().find(sink_pin_name);
  if (sink_pin_iter == database.get_pin_map().end()) {
    return 0.0;
  }
  Pin& sink_pin = sink_pin_iter->second;
  if (sink_pin.get_net_name().empty()) {
    return 0.0;
  }
  auto net_iter = database.get_net_map().find(sink_pin.get_net_name());
  if (net_iter == database.get_net_map().end()) {
    return 0.0;
  }
  Net& net = net_iter->second;
  return getNetOutputLoad(sink_pin.get_net_name(), net, analysis_type, output_trans_type);
}

double getOutputPinLoad(std::string& output_pin, AnalysisType analysis_type, TransType output_trans_type)
{
  Database& database = PWDM.getDatabase();
  auto pin_iter = database.get_pin_map().find(output_pin);
  if (pin_iter == database.get_pin_map().end()) {
    return 0.0;
  }
  Pin& pin = pin_iter->second;
  if (pin.get_net_name().empty()) {
    return 0.0;
  }
  auto net_iter = database.get_net_map().find(pin.get_net_name());
  if (net_iter == database.get_net_map().end()) {
    return 0.0;
  }
  Net& net = net_iter->second;
  return getNetOutputLoad(pin.get_net_name(), net, analysis_type, output_trans_type);
}

double getNetOutputLoad(const std::string& net_name, Net& net, AnalysisType analysis_type, TransType output_trans_type)
{
  Database& database = PWDM.getDatabase();
  TimingConstraint& timing_constraint = database.get_timing_constraint();
  if (timing_constraint.has_net_load(net_name, analysis_type, output_trans_type)) {
    double output_load = timing_constraint.get_net_load(net_name, analysis_type, output_trans_type);
    for (std::string& load_pin_name : net.get_load_pin_list()) {
      output_load += getPinCapacitance(load_pin_name, analysis_type, output_trans_type);
    }
    return output_load;
  }

  double output_load = 0.0;
  auto parasitic_net_iter = database.get_parasitic_net_map().find(net_name);
  if (parasitic_net_iter != database.get_parasitic_net_map().end()) {
    ParasiticNet& parasitic_net = parasitic_net_iter->second;
    output_load = getParasiticNetOutputLoad(net, parasitic_net, analysis_type, output_trans_type);
  } else {
    for (std::string& load_pin_name : net.get_load_pin_list()) {
      output_load += getPinCapacitance(load_pin_name, analysis_type, output_trans_type);
    }
  }
  return output_load;
}

double getParasiticNetOutputLoad(
    Net& net,
    ParasiticNet& parasitic_net,
    AnalysisType analysis_type,
    TransType trans_type
)
{
  if (net.get_driver_pin_list().empty()) {
    return getParasiticTotalLoad(parasitic_net, analysis_type, trans_type);
  }
  std::string source_node_name = getParasiticNodeName(parasitic_net, net.get_driver_pin_list().back());
  if (source_node_name.empty()) {
    return getParasiticTotalLoad(parasitic_net, analysis_type, trans_type);
  }

  buildParasiticDelayMap(parasitic_net, source_node_name, analysis_type, trans_type);
  return PWDCACHE.nodeLoadMap(parasitic_net.get_net_name(), analysis_type, trans_type)[source_node_name];
}

double getParasiticTotalLoad(ParasiticNet& parasitic_net, AnalysisType analysis_type, TransType trans_type)
{
  double total_load = 0.0;
  for (std::pair<const std::string, double>& node_pair : parasitic_net.get_node_capacitance_map()) {
    std::string node_name = node_pair.first;
    total_load += getParasiticNodeLoad(parasitic_net, node_name, analysis_type, trans_type);
  }
  return total_load;
}

}  // namespace ipw
