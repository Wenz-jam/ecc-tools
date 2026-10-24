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
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Database.hpp"
#include "PinCapacitanceCalculator.hpp"

namespace ista {
// Owns rooted RC loads, Elmore delays and impulse moments. Re-rooting invalidates root-dependent values.
class RcTreeAnalyzer
{
 public:
  RcTreeAnalyzer(Database& database, PinCapacitanceCalculator& pin_capacitance) : _database(database), _pin_capacitance(pin_capacitance) {}
  RcTreeAnalyzer(const RcTreeAnalyzer&) = delete;
  RcTreeAnalyzer& operator=(const RcTreeAnalyzer&) = delete;
  void clear()
  {
    _parasitic_resistor_map_cache.clear();
    _parasitic_load_map_cache.clear();
    _parasitic_root_map_cache.clear();
    _parasitic_delay_map_cache.clear();
    _parasitic_impulse_map_cache.clear();
  }
  double load(const std::string& net, AnalysisType analysis, TransType trans, const std::string& node)
  {
    return _parasitic_load_map_cache[net][analysis][trans][node];
  }
  std::optional<double> delay(const std::string& net, AnalysisType analysis, TransType trans, const std::string& node)
  {
    auto& values = _parasitic_delay_map_cache[net][analysis][trans];
    auto found = values.find(node);
    return found == values.end() ? std::nullopt : std::optional<double>(found->second);
  }
  std::optional<double> impulse(const std::string& net, AnalysisType analysis, TransType trans, const std::string& node)
  {
    auto& values = _parasitic_impulse_map_cache[net][analysis][trans];
    auto found = values.find(node);
    return found == values.end() ? std::nullopt : std::optional<double>(found->second);
  }
  bool hasDisabledDriver(const std::string& net, const std::string& source) const;
  double balancedDelay(ParasiticNet& net, const std::string& source, std::string& sink, AnalysisType analysis, TransType edge);
  double totalResistance(ParasiticNet& parasitic_net);
  double nodeCapacitance(ParasiticNet& parasitic_net, std::string& pin_name);
  void analyze(ParasiticNet& parasitic_net, std::string& source_node_name, AnalysisType analysis_type, TransType trans_type);
  double totalLoad(ParasiticNet& parasitic_net, AnalysisType analysis_type, TransType trans_type);
  std::string nodeName(ParasiticNet& parasitic_net, const std::string& pin_name);
  std::string pinName(std::string& node_name);
  void buildTree(ParasiticNet& parasitic_net, std::string& source_node_name, AnalysisType analysis_type, TransType trans_type,
                 std::vector<std::string>& node_name_list, std::vector<int32_t>& parent_idx_list, std::vector<double>& resistance_list,
                 std::vector<double>& capacitance_list);

 private:
  double nodeLoad(ParasiticNet& parasitic_net, std::string& node_name, AnalysisType analysis_type, TransType trans_type);
  double updateParasiticLoadMap(ParasiticNet& parasitic_net, std::string& node_name, std::string& parent_node_name,
                                std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map, std::set<std::string>& visited_node_set,
                                AnalysisType analysis_type, TransType trans_type);
  void updateParasiticDelayMap(ParasiticNet& parasitic_net, std::string& node_name, std::string& parent_node_name,
                               std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map, std::set<std::string>& visited_node_set,
                               AnalysisType analysis_type, TransType trans_type);
  double updateParasiticLoadDelayMap(ParasiticNet& parasitic_net, std::string& node_name, std::string& parent_node_name,
                                     std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map, std::set<std::string>& visited_node_set,
                                     AnalysisType analysis_type, TransType trans_type, std::map<std::string, double>& load_delay_map);
  void updateParasiticImpulseMap(ParasiticNet& parasitic_net, std::string& node_name, std::string& parent_node_name,
                                 std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map, std::set<std::string>& visited_node_set,
                                 AnalysisType analysis_type, TransType trans_type, std::map<std::string, double>& load_delay_map,
                                 std::map<std::string, double>& beta_map);
  void buildParasiticResistorMap(ParasiticNet& parasitic_net, std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map);
  Database& _database;
  PinCapacitanceCalculator& _pin_capacitance;
  std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, double>>>> _parasitic_resistor_map_cache;
  std::map<std::string, std::map<AnalysisType, std::map<TransType, std::map<std::string, double>>>> _parasitic_load_map_cache;
  std::map<std::string, std::map<AnalysisType, std::map<TransType, std::string>>> _parasitic_root_map_cache;
  std::map<std::string, std::map<AnalysisType, std::map<TransType, std::map<std::string, double>>>> _parasitic_delay_map_cache;
  std::map<std::string, std::map<AnalysisType, std::map<TransType, std::map<std::string, double>>>> _parasitic_impulse_map_cache;
};
}  // namespace ista
