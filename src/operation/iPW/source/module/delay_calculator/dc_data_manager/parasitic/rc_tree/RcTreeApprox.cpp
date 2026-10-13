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
// MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.
// ***************************************************************************************
#include "RcTreeApprox.hpp"

#include "DataManager.hpp"
#include "Database.hpp"
#include "DelayCache.hpp"
#include "OutputLoadEstimator.hpp"
#include "ParasiticNet.hpp"
#include "PWHeader.hpp"
#include "RcTree.hpp"

namespace ipw {

static double updateParasiticLoadMap(
    ParasiticNet& parasitic_net,
    std::string& node_name,
    std::string& parent_node_name,
    std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map,
    std::set<std::string>& visited_node_set,
    AnalysisType analysis_type,
    TransType trans_type
)
{
  if (visited_node_set.count(node_name) > 0) {
    return 0.0;
  }
  visited_node_set.insert(node_name);

  double subtree_load = getParasiticNodeLoad(parasitic_net, node_name, analysis_type, trans_type);
  if (resistor_map.count(node_name) == 0) {
    PWDCACHE.nodeLoadMap(parasitic_net.get_net_name(), analysis_type, trans_type)[node_name] = subtree_load;
    return subtree_load;
  }

  for (std::pair<std::string, double>& next_node_pair : resistor_map[node_name]) {
    if (next_node_pair.first == parent_node_name) {
      continue;
    }
    subtree_load += updateParasiticLoadMap(
        parasitic_net,
        next_node_pair.first,
        node_name,
        resistor_map,
        visited_node_set,
        analysis_type,
        trans_type
    );
  }
  PWDCACHE.nodeLoadMap(parasitic_net.get_net_name(), analysis_type, trans_type)[node_name] = subtree_load;
  return subtree_load;
}

static void updateParasiticDelayMap(
    ParasiticNet& parasitic_net,
    std::string& node_name,
    std::string& parent_node_name,
    std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map,
    std::set<std::string>& visited_node_set,
    AnalysisType analysis_type,
    TransType trans_type
)
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
    double node_delay = PWDCACHE.nodeDelayMap(net_name, analysis_type, trans_type)[node_name];
    double next_node_load = PWDCACHE.nodeLoadMap(net_name, analysis_type, trans_type)[next_node_pair.first];
    PWDCACHE.nodeDelayMap(net_name, analysis_type, trans_type)[next_node_pair.first]
        = node_delay + next_node_pair.second * next_node_load * 1E-3;
    updateParasiticDelayMap(
        parasitic_net,
        next_node_pair.first,
        node_name,
        resistor_map,
        visited_node_set,
        analysis_type,
        trans_type
    );
  }
}

static double updateParasiticLoadDelayMap(
    ParasiticNet& parasitic_net,
    std::string& node_name,
    std::string& parent_node_name,
    std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map,
    std::set<std::string>& visited_node_set,
    AnalysisType analysis_type,
    TransType trans_type,
    std::map<std::string, double>& load_delay_map
)
{
  if (visited_node_set.count(node_name) > 0) {
    return 0.0;
  }
  visited_node_set.insert(node_name);

  double load_delay = getParasiticNodeLoad(parasitic_net, node_name, analysis_type, trans_type)
                      * PWDCACHE.nodeDelayMap(parasitic_net.get_net_name(), analysis_type, trans_type)[node_name];
  if (resistor_map.count(node_name) == 0) {
    load_delay_map[node_name] = load_delay;
    return load_delay;
  }

  for (std::pair<std::string, double>& next_node_pair : resistor_map[node_name]) {
    if (next_node_pair.first == parent_node_name) {
      continue;
    }
    load_delay += updateParasiticLoadDelayMap(
        parasitic_net,
        next_node_pair.first,
        node_name,
        resistor_map,
        visited_node_set,
        analysis_type,
        trans_type,
        load_delay_map
    );
  }
  load_delay_map[node_name] = load_delay;
  return load_delay;
}

static void updateParasiticImpulseMap(
    ParasiticNet& parasitic_net,
    std::string& node_name,
    std::string& parent_node_name,
    std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map,
    std::set<std::string>& visited_node_set,
    AnalysisType analysis_type,
    TransType trans_type,
    std::map<std::string, double>& load_delay_map,
    std::map<std::string, double>& beta_map
)
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
      beta_map[next_node_pair.first]
          = beta_map[node_name] + next_node_pair.second * load_delay_map[next_node_pair.first] * 1E-3;
      updateParasiticImpulseMap(
          parasitic_net,
          next_node_pair.first,
          node_name,
          resistor_map,
          visited_node_set,
          analysis_type,
          trans_type,
          load_delay_map,
          beta_map
      );
    }
  }

  double node_delay = PWDCACHE.nodeDelayMap(net_name, analysis_type, trans_type)[node_name];
  double impulse = 2.0 * beta_map[node_name] - std::pow(node_delay, 2);
  PWDCACHE.nodeImpulseMap(net_name, analysis_type, trans_type)[node_name] = std::max(0.0, impulse);
}

void buildParasiticDelayMap(
    ParasiticNet& parasitic_net,
    std::string& source_node_name,
    AnalysisType analysis_type,
    TransType trans_type
)
{
  std::string& net_name = parasitic_net.get_net_name();
  if (PWDCACHE.nodeDelayMap(net_name, analysis_type, trans_type).count(source_node_name) > 0
      && PWDCACHE.nodeImpulseMap(net_name, analysis_type, trans_type).count(source_node_name) > 0) {
    return;
  }

  if (!PWDCACHE.hasResistorMap(net_name)) {
    buildParasiticResistorMap(parasitic_net, PWDCACHE.resistorMap(net_name));
  }
  std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map = PWDCACHE.resistorMap(net_name);

  std::string parent_node_name;
  std::set<std::string> load_visited_node_set;
  updateParasiticLoadMap(
      parasitic_net,
      source_node_name,
      parent_node_name,
      resistor_map,
      load_visited_node_set,
      analysis_type,
      trans_type
  );

  PWDCACHE.nodeDelayMap(net_name, analysis_type, trans_type)[source_node_name] = 0.0;
  std::set<std::string> delay_visited_node_set;
  updateParasiticDelayMap(
      parasitic_net,
      source_node_name,
      parent_node_name,
      resistor_map,
      delay_visited_node_set,
      analysis_type,
      trans_type
  );

  std::map<std::string, double> load_delay_map;
  std::set<std::string> load_delay_visited_node_set;
  updateParasiticLoadDelayMap(
      parasitic_net,
      source_node_name,
      parent_node_name,
      resistor_map,
      load_delay_visited_node_set,
      analysis_type,
      trans_type,
      load_delay_map
  );

  std::map<std::string, double> beta_map;
  beta_map[source_node_name] = 0.0;
  std::set<std::string> impulse_visited_node_set;
  updateParasiticImpulseMap(
      parasitic_net,
      source_node_name,
      parent_node_name,
      resistor_map,
      impulse_visited_node_set,
      analysis_type,
      trans_type,
      load_delay_map,
      beta_map
  );
}

}  // namespace ipw
