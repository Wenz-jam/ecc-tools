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
  Pin& pin = database.get_pin_map()[pin_name];
  if (pin.get_is_port()) {
    std::map<std::string, TimingPortConstraint>& port_constraint_map
        = database.get_timing_constraint().get_port_constraint_map();
    if (port_constraint_map.count(pin_name) > 0 && port_constraint_map[pin_name].get_has_load()) {
      return port_constraint_map[pin_name].get_load(analysis_type, trans_type);
    }
    return 0.0;
  }
  if (database.get_instance_map().count(pin.get_instance_name()) == 0) {
    return 0.0;
  }
  Instance& instance = database.get_instance_map()[pin.get_instance_name()];
  std::map<std::string, TimingCell>& timing_cell_map = database.get_timing_library().get_cell_map();
  if (timing_cell_map.count(instance.get_cell_name()) == 0) {
    return 0.0;
  }
  TimingCell& timing_cell = timing_cell_map[instance.get_cell_name()];
  if (timing_cell.get_port_map().count(pin.get_pin_name()) == 0) {
    return 0.0;
  }
  TimingCellPort& timing_cell_port = timing_cell.get_port_map()[pin.get_pin_name()];
  if (timing_cell_port.get_trans_capacitance_map().count(analysis_type) > 0
      && timing_cell_port.get_trans_capacitance_map()[analysis_type].count(trans_type) > 0) {
    return timing_cell_port.get_trans_capacitance_map()[analysis_type][trans_type];
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
  Database& database = PWDM.getDatabase();
  double node_load = 0.0;
  if (parasitic_net.get_node_capacitance_map().count(node_name) > 0) {
    node_load += parasitic_net.get_node_capacitance_map()[node_name];
  }

  std::string pin_name = getPinNameByParasiticNodeName(node_name);
  if (database.get_pin_map().count(pin_name) > 0) {
    node_load += getPinCapacitance(pin_name, analysis_type, trans_type);
  }
  return node_load;
}

double getArcOutputLoad(Arc& arc, AnalysisType analysis_type, TransType output_trans_type)
{
  Database& database = PWDM.getDatabase();
  std::string& sink_pin_name = arc.get_sink_pin();
  if (database.get_pin_map().count(sink_pin_name) == 0) {
    return 0.0;
  }
  Pin& sink_pin = database.get_pin_map()[sink_pin_name];
  if (sink_pin.get_net_name().empty() || database.get_net_map().count(sink_pin.get_net_name()) == 0) {
    return 0.0;
  }
  Net& net = database.get_net_map()[sink_pin.get_net_name()];
  return getNetOutputLoad(sink_pin.get_net_name(), net, analysis_type, output_trans_type);
}

double getOutputPinLoad(std::string& output_pin, AnalysisType analysis_type, TransType output_trans_type)
{
  Database& database = PWDM.getDatabase();
  if (database.get_pin_map().count(output_pin) == 0) {
    return 0.0;
  }
  Pin& pin = database.get_pin_map()[output_pin];
  if (pin.get_net_name().empty() || database.get_net_map().count(pin.get_net_name()) == 0) {
    return 0.0;
  }
  Net& net = database.get_net_map()[pin.get_net_name()];
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
  if (database.get_parasitic_net_map().count(net_name) > 0) {
    ParasiticNet& parasitic_net = database.get_parasitic_net_map()[net_name];
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
