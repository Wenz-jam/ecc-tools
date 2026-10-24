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
//
// See the Mulan PSL v2 for more details.
// ***************************************************************************************
#include "RcTreeAnalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ista {

bool RcTreeAnalyzer::hasDisabledDriver(const std::string& name, const std::string& source) const
{
  const auto found = _database.network().nets().find(name);
  if (found == _database.network().nets().end())
    return false;
  const auto& drivers = found->second.get_driver_pin_list();
  return drivers.size() > 1
         && std::any_of(drivers.begin(), drivers.end(), [&](const auto& pin) { return pin != source && _database.graph().simValues().isDriverDisabled(pin); });
}

double RcTreeAnalyzer::balancedDelay(ParasiticNet& net, const std::string& source, std::string& sink, AnalysisType analysis, TransType edge)
{
  const auto found = _database.network().nets().find(net.get_net_name());
  if (found == _database.network().nets().end())
    return 0.;
  const auto& loads = found->second.get_load_pin_list();
  const auto count = std::count_if(loads.begin(), loads.end(), [&](const auto& pin) { return pin != source; });
  if (count == 0)
    return 0.;
  double wire_cap = 0.;
  for (const auto& [name, node] : net.get_node_map())
    wire_cap += node.get_capacitance();
  if (net.get_node_map().empty())
    wire_cap = net.get_lumped_capacitance();
  return totalResistance(net) * 1e-3 / count * (_pin_capacitance.pinCapacitance(sink, analysis, edge) + wire_cap / count);
}

double RcTreeAnalyzer::totalResistance(ParasiticNet& parasitic_net)
{
  double resistance = 0.0;
  for (ParasiticResistor& parasitic_resistor : parasitic_net.get_resistor_list()) {
    resistance += parasitic_resistor.get_resistance();
  }
  return resistance;
}

double RcTreeAnalyzer::nodeCapacitance(ParasiticNet& parasitic_net, std::string& pin_name)
{
  std::string spef_pin_name = pin_name;
  std::replace(spef_pin_name.begin(), spef_pin_name.end(), ':', '/');
  if (parasitic_net.get_node_map().count(spef_pin_name) > 0) {
    return parasitic_net.get_node_map()[spef_pin_name].get_capacitance();
  }
  if (parasitic_net.get_node_map().count(pin_name) > 0) {
    return parasitic_net.get_node_map()[pin_name].get_capacitance();
  }
  return parasitic_net.get_lumped_capacitance();
}

double RcTreeAnalyzer::nodeLoad(ParasiticNet& parasitic_net, std::string& node_name, AnalysisType analysis_type, TransType trans_type)
{
  Database& database = _database;
  double node_load = 0.0;
  if (parasitic_net.get_node_map().count(node_name) > 0) {
    node_load += parasitic_net.get_node_map()[node_name].get_capacitance();
  }

  std::string pin_name = pinName(node_name);
  if (database.network().pins().count(pin_name) > 0) {
    node_load += _pin_capacitance.pinCapacitance(pin_name, analysis_type, trans_type);
  }
  return node_load;
}

void RcTreeAnalyzer::analyze(ParasiticNet& parasitic_net, std::string& source_node_name, AnalysisType analysis_type, TransType trans_type)
{
  std::string& net_name = parasitic_net.get_net_name();
  std::string& cached_root = _parasitic_root_map_cache[net_name][analysis_type][trans_type];
  if (cached_root == source_node_name && !cached_root.empty()) {
    return;
  }
  cached_root = source_node_name;
  _parasitic_load_map_cache[net_name][analysis_type][trans_type].clear();
  _parasitic_delay_map_cache[net_name][analysis_type][trans_type].clear();
  _parasitic_impulse_map_cache[net_name][analysis_type][trans_type].clear();

  if (_parasitic_resistor_map_cache.count(net_name) == 0) {
    buildParasiticResistorMap(parasitic_net, _parasitic_resistor_map_cache[net_name]);
  }
  std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map = _parasitic_resistor_map_cache[net_name];

  std::string parent_node_name;
  std::set<std::string> load_visited_node_set;
  updateParasiticLoadMap(parasitic_net, source_node_name, parent_node_name, resistor_map, load_visited_node_set, analysis_type, trans_type);

  _parasitic_delay_map_cache[net_name][analysis_type][trans_type][source_node_name] = 0.0;
  std::set<std::string> delay_visited_node_set;
  updateParasiticDelayMap(parasitic_net, source_node_name, parent_node_name, resistor_map, delay_visited_node_set, analysis_type, trans_type);

  std::map<std::string, double> load_delay_map;
  std::set<std::string> load_delay_visited_node_set;
  updateParasiticLoadDelayMap(parasitic_net, source_node_name, parent_node_name, resistor_map, load_delay_visited_node_set, analysis_type, trans_type,
                              load_delay_map);

  std::map<std::string, double> beta_map;
  beta_map[source_node_name] = 0.0;
  std::set<std::string> impulse_visited_node_set;
  updateParasiticImpulseMap(parasitic_net, source_node_name, parent_node_name, resistor_map, impulse_visited_node_set, analysis_type, trans_type,
                            load_delay_map, beta_map);
}

double RcTreeAnalyzer::updateParasiticLoadMap(ParasiticNet& parasitic_net, std::string& node_name, std::string& parent_node_name,
                                              std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map,
                                              std::set<std::string>& visited_node_set, AnalysisType analysis_type, TransType trans_type)
{
  if (visited_node_set.count(node_name) > 0) {
    return 0.0;
  }
  visited_node_set.insert(node_name);

  double subtree_load = nodeLoad(parasitic_net, node_name, analysis_type, trans_type);
  if (parent_node_name.empty()) {
    std::string driver_pin = pinName(node_name);
    if (_database.network().pins().contains(driver_pin)) {
      subtree_load -= _pin_capacitance.pinCapacitance(driver_pin, analysis_type, trans_type);
    }
  }
  if (resistor_map.count(node_name) == 0) {
    _parasitic_load_map_cache[parasitic_net.get_net_name()][analysis_type][trans_type][node_name] = subtree_load;
    return subtree_load;
  }

  for (std::pair<std::string, double>& next_node_pair : resistor_map[node_name]) {
    if (next_node_pair.first == parent_node_name) {
      continue;
    }
    subtree_load += updateParasiticLoadMap(parasitic_net, next_node_pair.first, node_name, resistor_map, visited_node_set, analysis_type, trans_type);
  }
  _parasitic_load_map_cache[parasitic_net.get_net_name()][analysis_type][trans_type][node_name] = subtree_load;
  return subtree_load;
}

void RcTreeAnalyzer::updateParasiticDelayMap(ParasiticNet& parasitic_net, std::string& node_name, std::string& parent_node_name,
                                             std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map,
                                             std::set<std::string>& visited_node_set, AnalysisType analysis_type, TransType trans_type)
{
  if (visited_node_set.count(node_name) > 0) {
    return;
  }
  visited_node_set.insert(node_name);
  if (resistor_map.count(node_name) == 0) {
    return;
  }

  std::string& net_name = parasitic_net.get_net_name();
  for (std::pair<std::string, double>& next_node_pair : resistor_map[node_name]) {
    if (next_node_pair.first == parent_node_name || visited_node_set.count(next_node_pair.first) > 0) {
      continue;
    }
    double node_delay = _parasitic_delay_map_cache[net_name][analysis_type][trans_type][node_name];
    double next_node_load = _parasitic_load_map_cache[net_name][analysis_type][trans_type][next_node_pair.first];
    _parasitic_delay_map_cache[net_name][analysis_type][trans_type][next_node_pair.first] = node_delay + next_node_pair.second * next_node_load * 1E-3;
    updateParasiticDelayMap(parasitic_net, next_node_pair.first, node_name, resistor_map, visited_node_set, analysis_type, trans_type);
  }
}

double RcTreeAnalyzer::updateParasiticLoadDelayMap(ParasiticNet& parasitic_net, std::string& node_name, std::string& parent_node_name,
                                                   std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map,
                                                   std::set<std::string>& visited_node_set, AnalysisType analysis_type, TransType trans_type,
                                                   std::map<std::string, double>& load_delay_map)
{
  if (visited_node_set.count(node_name) > 0) {
    return 0.0;
  }
  visited_node_set.insert(node_name);

  double load_delay = nodeLoad(parasitic_net, node_name, analysis_type, trans_type)
                      * _parasitic_delay_map_cache[parasitic_net.get_net_name()][analysis_type][trans_type][node_name];
  if (resistor_map.count(node_name) == 0) {
    load_delay_map[node_name] = load_delay;
    return load_delay;
  }

  for (std::pair<std::string, double>& next_node_pair : resistor_map[node_name]) {
    if (next_node_pair.first == parent_node_name) {
      continue;
    }
    load_delay += updateParasiticLoadDelayMap(parasitic_net, next_node_pair.first, node_name, resistor_map, visited_node_set, analysis_type, trans_type,
                                              load_delay_map);
  }
  load_delay_map[node_name] = load_delay;
  return load_delay;
}

void RcTreeAnalyzer::updateParasiticImpulseMap(ParasiticNet& parasitic_net, std::string& node_name, std::string& parent_node_name,
                                               std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map,
                                               std::set<std::string>& visited_node_set, AnalysisType analysis_type, TransType trans_type,
                                               std::map<std::string, double>& load_delay_map, std::map<std::string, double>& beta_map)
{
  if (visited_node_set.count(node_name) > 0) {
    return;
  }
  visited_node_set.insert(node_name);

  std::string& net_name = parasitic_net.get_net_name();
  if (resistor_map.count(node_name) > 0) {
    for (std::pair<std::string, double>& next_node_pair : resistor_map[node_name]) {
      if (next_node_pair.first == parent_node_name || visited_node_set.count(next_node_pair.first) > 0) {
        continue;
      }
      beta_map[next_node_pair.first] = beta_map[node_name] + next_node_pair.second * load_delay_map[next_node_pair.first] * 1E-3;
      updateParasiticImpulseMap(parasitic_net, next_node_pair.first, node_name, resistor_map, visited_node_set, analysis_type, trans_type, load_delay_map,
                                beta_map);
    }
  }

  double node_delay = _parasitic_delay_map_cache[net_name][analysis_type][trans_type][node_name];
  double impulse = 2.0 * beta_map[node_name] - std::pow(node_delay, 2);
  _parasitic_impulse_map_cache[net_name][analysis_type][trans_type][node_name] = std::max(0.0, impulse);
}

double RcTreeAnalyzer::totalLoad(ParasiticNet& parasitic_net, AnalysisType analysis_type, TransType trans_type)
{
  double total_load = 0.0;
  for (std::pair<const std::string, ParasiticNode>& node_pair : parasitic_net.get_node_map()) {
    std::string node_name = node_pair.first;
    total_load += nodeLoad(parasitic_net, node_name, analysis_type, trans_type);
  }
  return total_load;
}

void RcTreeAnalyzer::buildParasiticResistorMap(ParasiticNet& parasitic_net, std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map)
{
  resistor_map.clear();
  for (ParasiticResistor& parasitic_resistor : parasitic_net.get_resistor_list()) {
    resistor_map[parasitic_resistor.get_source_node()].push_back(std::make_pair(parasitic_resistor.get_sink_node(), parasitic_resistor.get_resistance()));
    resistor_map[parasitic_resistor.get_sink_node()].push_back(std::make_pair(parasitic_resistor.get_source_node(), parasitic_resistor.get_resistance()));
  }
}

std::string RcTreeAnalyzer::nodeName(ParasiticNet& parasitic_net, const std::string& pin_name)
{
  if (parasitic_net.get_node_map().count(pin_name) > 0) {
    return pin_name;
  }

  std::string spef_pin_name = pin_name;
  std::replace(spef_pin_name.begin(), spef_pin_name.end(), ':', '/');
  if (parasitic_net.get_node_map().count(spef_pin_name) > 0) {
    return spef_pin_name;
  }
  return "";
}

std::string RcTreeAnalyzer::pinName(std::string& node_name)
{
  Database& database = _database;
  if (database.network().pins().count(node_name) > 0) {
    return node_name;
  }

  std::string pin_name = node_name;
  // Only the instance/pin separator changes; hierarchy and escaped slashes
  // remain part of their names. Try each split against actual connectivity.
  for (size_t slash = pin_name.rfind('/'); slash != std::string::npos; slash = slash == 0 ? std::string::npos : pin_name.rfind('/', slash - 1)) {
    pin_name[slash] = ':';
    if (database.network().pins().contains(pin_name))
      return pin_name;
    pin_name[slash] = '/';
  }
  return node_name;
}

void RcTreeAnalyzer::buildTree(ParasiticNet& parasitic_net, std::string& source_node_name, AnalysisType analysis_type, TransType trans_type,
                               std::vector<std::string>& node_name_list, std::vector<int32_t>& parent_idx_list, std::vector<double>& resistance_list,
                               std::vector<double>& capacitance_list)
{
  std::string& net_name = parasitic_net.get_net_name();
  if (_parasitic_resistor_map_cache.count(net_name) == 0) {
    buildParasiticResistorMap(parasitic_net, _parasitic_resistor_map_cache[net_name]);
  }
  std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map = _parasitic_resistor_map_cache[net_name];

  std::set<std::string> visited_node_set;
  node_name_list.push_back(source_node_name);
  parent_idx_list.push_back(-1);
  resistance_list.push_back(0.0);
  capacitance_list.push_back(parasitic_net.get_node_map()[source_node_name].get_capacitance());
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
    capacitance_list.push_back(nodeLoad(parasitic_net, next_node_name, analysis_type, trans_type));
    stack_node_idx_list.push_back(node_name_list.size() - 1);
    stack_next_edge_idx_list.push_back(0);
  }
}

}  // namespace ista
