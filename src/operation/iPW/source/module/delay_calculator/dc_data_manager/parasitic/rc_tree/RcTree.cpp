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
#include "RcTree.hpp"

#include "DataManager.hpp"
#include "Database.hpp"
#include "DelayCache.hpp"
#include "OutputLoadEstimator.hpp"
#include "ParasiticDmpModel.hpp"
#include "ParasiticNet.hpp"
#include "PWHeader.hpp"

namespace ipw {

void buildParasiticResistorMap(
    ParasiticNet& parasitic_net,
    std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map
)
{
  resistor_map.clear();
  for (ParasiticResistor& parasitic_resistor : parasitic_net.get_resistor_list()) {
    resistor_map[parasitic_resistor.get_source_node()].push_back(
        std::make_pair(parasitic_resistor.get_sink_node(), parasitic_resistor.get_resistance())
    );
    resistor_map[parasitic_resistor.get_sink_node()].push_back(
        std::make_pair(parasitic_resistor.get_source_node(), parasitic_resistor.get_resistance())
    );
  }
}

void flattenRcTree(
    ParasiticNet& parasitic_net,
    std::string& source_node_name,
    AnalysisType analysis_type,
    TransType trans_type,
    std::vector<std::string>& node_name_list,
    std::vector<int32_t>& parent_idx_list,
    std::vector<double>& resistance_list,
    std::vector<double>& capacitance_list
)
{
  std::string& net_name = parasitic_net.get_net_name();
  if (!PWDCACHE.hasResistorMap(net_name)) {
    buildParasiticResistorMap(parasitic_net, PWDCACHE.resistorMap(net_name));
  }
  std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map = PWDCACHE.resistorMap(net_name);

  std::set<std::string> visited_node_set;
  node_name_list.push_back(source_node_name);
  parent_idx_list.push_back(-1);
  resistance_list.push_back(0.0);
  capacitance_list.push_back(getParasiticNodeLoad(parasitic_net, source_node_name, analysis_type, trans_type));
  visited_node_set.insert(source_node_name);

  std::vector<std::size_t> stack_node_idx_list;
  std::vector<std::size_t> stack_next_edge_idx_list;
  stack_node_idx_list.push_back(0);
  stack_next_edge_idx_list.push_back(0);
  while (!stack_node_idx_list.empty()) {
    std::size_t node_idx = stack_node_idx_list.back();
    std::string& node_name = node_name_list[node_idx];
    if (resistor_map.count(node_name) == 0) {
      stack_node_idx_list.pop_back();
      stack_next_edge_idx_list.pop_back();
      continue;
    }

    std::vector<std::pair<std::string, double>>& edge_list = resistor_map[node_name];
    std::size_t& edge_idx = stack_next_edge_idx_list.back();
    if (edge_idx >= edge_list.size()) {
      stack_node_idx_list.pop_back();
      stack_next_edge_idx_list.pop_back();
      continue;
    }

    std::pair<std::string, double>& next_node_pair = edge_list[edge_idx];
    edge_idx++;
    std::string& next_node_name = next_node_pair.first;
    if (visited_node_set.count(next_node_name) > 0) {
      continue;
    }

    visited_node_set.insert(next_node_name);
    node_name_list.push_back(next_node_name);
    parent_idx_list.push_back(static_cast<int32_t>(node_idx));
    resistance_list.push_back(next_node_pair.second * 1E-3);
    capacitance_list.push_back(getParasiticNodeLoad(parasitic_net, next_node_name, analysis_type, trans_type));
    stack_node_idx_list.push_back(node_name_list.size() - 1);
    stack_next_edge_idx_list.push_back(0);
  }
}

void buildPiModel(
    ParasiticDmpModel& dmp_model,
    std::vector<int32_t>& parent_idx_list,
    std::vector<double>& resistance_list,
    std::vector<double>& capacitance_list
)
{
  if (parent_idx_list.size() != resistance_list.size() || resistance_list.size() != capacitance_list.size()
      || capacitance_list.empty()) {
    return;
  }

  std::vector<double> admittance1_list = capacitance_list;
  std::vector<double> admittance2_list(capacitance_list.size(), 0.0);
  std::vector<double> admittance3_list(capacitance_list.size(), 0.0);
  for (std::size_t node_idx = capacitance_list.size() - 1; node_idx > 0; node_idx--) {
    int32_t parent_idx = parent_idx_list[node_idx];
    if (parent_idx < 0) {
      return;
    }
    double resistance = resistance_list[node_idx];
    double admittance1 = admittance1_list[node_idx];
    double admittance2 = admittance2_list[node_idx];
    double admittance3 = admittance3_list[node_idx];
    admittance1_list[parent_idx] += admittance1;
    admittance2_list[parent_idx] += admittance2 - resistance * admittance1 * admittance1;
    admittance3_list[parent_idx] += admittance3 - 2.0 * resistance * admittance1 * admittance2
                                    + resistance * resistance * admittance1 * admittance1 * admittance1;
  }

  double admittance1 = admittance1_list.front();
  double admittance2 = admittance2_list.front();
  double admittance3 = admittance3_list.front();
  double driver_capacitance = 0.0;
  double pi_resistance = 0.0;
  double load_capacitance = 0.0;
  if (admittance2 == 0.0 && admittance3 == 0.0) {
    load_capacitance = admittance1;
  } else {
    if (admittance3 == 0.0 || admittance2 == 0.0) {
      return;
    }
    load_capacitance = admittance2 * admittance2 / admittance3;
    driver_capacitance = admittance1 - load_capacitance;
    pi_resistance = -admittance3 * admittance3 / (admittance2 * admittance2 * admittance2);
  }
  if (!std::isfinite(driver_capacitance) || !std::isfinite(pi_resistance) || !std::isfinite(load_capacitance)
      || driver_capacitance < 0.0 || pi_resistance < 0.0 || load_capacitance < 0.0
      || driver_capacitance + load_capacitance <= 0.0) {
    return;
  }

  dmp_model.set_is_valid(true);
  dmp_model.set_driver_capacitance(driver_capacitance);
  dmp_model.set_pi_resistance(pi_resistance);
  dmp_model.set_load_capacitance(load_capacitance);
}

}  // namespace ipw
