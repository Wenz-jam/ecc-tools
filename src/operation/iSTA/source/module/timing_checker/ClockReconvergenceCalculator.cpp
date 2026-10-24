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
#include "ClockReconvergenceCalculator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "graph/TimingStateQuery.hpp"
namespace ista {
double ClockReconvergenceCalculator::correction(const std::string& launch_pin, TransType launch_trans_type, std::string_view launch_clock,
                                                const std::string& capture_pin, TransType capture_trans_type, std::string_view capture_clock,
                                                AnalysisType analysis_type, std::string& common_pin_name, std::optional<TransType> launch_source,
                                                std::optional<TransType> capture_source)
{
  common_pin_name.clear();
  if (!_database.sdc().clockPinConstraints().empty()) {
    auto unsupported_local = [&](const std::string& pin, std::string_view name, AnalysisType mode, TransType edge, std::optional<TransType> source) {
      const auto clock = _database.sdc().get_clock_map().find(std::string(name));
      if (clock == _database.sdc().get_clock_map().end() || clock->second.get_is_propagated())
        return false;
      const auto values = clockPinValues(_database, pin, name, mode, edge, AnalysisType::kNone, source);
      const auto& sources = clock->second.get_source_list();
      return !values.propagated || (!values.propagated_pin.empty() && std::find(sources.begin(), sources.end(), values.propagated_pin) == sources.end());
    };
    // PT supports local propagated-clock CRPR only at common clock sources.
    if (unsupported_local(launch_pin, launch_clock, analysis_type, launch_trans_type, launch_source)
        || unsupported_local(capture_pin, capture_clock, getCaptureAnalysisType(analysis_type), capture_trans_type, capture_source))
      return 0.0;
  }
  AnalysisType launch_analysis_type = analysis_type;
  AnalysisType capture_analysis_type = getCaptureAnalysisType(analysis_type);
  std::vector<ClockPathPoint> launch_clock_path = getClockPathPinList(launch_pin, launch_clock, launch_analysis_type, launch_trans_type, launch_source);
  std::vector<ClockPathPoint> capture_clock_path = getClockPathPinList(capture_pin, capture_clock, capture_analysis_type, capture_trans_type, capture_source);
  common_pin_name.clear();
  ClockPathPoint launch_common_pin;
  ClockPathPoint capture_common_pin;
  std::size_t path_size = std::min(launch_clock_path.size(), capture_clock_path.size());
  for (std::size_t i = 0; i < path_size; i++) {
    if (launch_clock_path[i].vertex != capture_clock_path[i].vertex) {
      break;
    }
    launch_common_pin = launch_clock_path[i];
    capture_common_pin = capture_clock_path[i];
    common_pin_name = _database.graph().vertices()[launch_clock_path[i].vertex].get_pin_name();
  }
  if (common_pin_name.empty()) {
    return 0.0;
  }
  double launch_delay_delta = getClockCommonPathDelayDelta(launch_common_pin, launch_clock);
  double capture_delay_delta = getClockCommonPathDelayDelta(capture_common_pin, capture_clock);
  return std::min(launch_delay_delta, capture_delay_delta);
}

double ClockReconvergenceCalculator::getClockCommonPathDelayDelta(const ClockPathPoint& common_pin, std::string_view clock_name)
{
  const auto& point = _database.graph().vertices().at(common_pin.vertex).get_timing_point();
  const auto* clock = point.find_clock_state(clock_name);
  const auto* state = clock ? clock->waveform(common_pin.source) : nullptr;
  if (!state)
    return 0.;
  const auto early = state->arrival(AnalysisType::kMin, common_pin.transition);
  const auto late = state->arrival(AnalysisType::kMax, common_pin.transition);
  return early && late && std::isfinite(*early) && std::isfinite(*late) ? std::fabs(*late - *early) : 0.;
}

std::vector<ClockReconvergenceCalculator::ClockPathPoint> ClockReconvergenceCalculator::getClockPathPinList(const std::string& clock_pin_name,
                                                                                                            std::string_view clock_name,
                                                                                                            AnalysisType analysis_type, TransType trans_type,
                                                                                                            std::optional<TransType> source)
{
  Database& database = _database;
  std::vector<ClockPathPoint> clock_path_pin_list;
  auto pin = database.network().pins().find(clock_pin_name);
  int32_t vertex_idx = pin == database.network().pins().end() ? -1 : pin->second.get_load_vertex_idx();
  const auto* selected = clockPoint(database, clock_pin_name, clock_name);
  if (pin != database.network().pins().end() && selected && (vertex_idx < 0 || &database.graph().vertices()[vertex_idx].get_timing_point() != selected))
    vertex_idx = pin->second.get_driver_vertex_idx();
  TransType current_trans_type = trans_type;
  std::set<int32_t> visited;
  while (vertex_idx >= 0 && visited.insert(vertex_idx).second) {
    GraphVertex& vertex = database.graph().vertices()[vertex_idx];
    clock_path_pin_list.push_back({vertex_idx, current_trans_type, source});
    const auto* clock = vertex.get_timing_point().find_clock_state(clock_name);
    const ClockWaveform* state = clock ? clock->waveform(source) : nullptr;
    if (state == nullptr || !state->predecessor_map.contains(analysis_type) || !state->predecessor_map.at(analysis_type).contains(current_trans_type)) {
      break;
    }
    TransType predecessor_trans_type = current_trans_type;
    if (state->predecessor_trans_type_map.contains(analysis_type) && state->predecessor_trans_type_map.at(analysis_type).contains(current_trans_type)) {
      predecessor_trans_type = state->predecessor_trans_type_map.at(analysis_type).at(current_trans_type);
    }
    if (!state->predecessor_vertex_map.contains(analysis_type) || !state->predecessor_vertex_map.at(analysis_type).contains(current_trans_type)) {
      break;
    }
    vertex_idx = state->predecessor_vertex_map.at(analysis_type).at(current_trans_type);
    if (source)
      source = state->predecessorSource(analysis_type, current_trans_type, *source);
    current_trans_type = predecessor_trans_type;
  }
  std::reverse(clock_path_pin_list.begin(), clock_path_pin_list.end());
  return clock_path_pin_list;
}

AnalysisType ClockReconvergenceCalculator::getCaptureAnalysisType(AnalysisType analysis_type)
{
  if (analysis_type == AnalysisType::kMax) {
    return AnalysisType::kMin;
  }
  if (analysis_type == AnalysisType::kMin) {
    return AnalysisType::kMax;
  }
  return analysis_type;
}

}  // namespace ista
