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
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "DelayCacheKey.hpp"
#include "ParasiticArnoldiModel.hpp"
#include "ParasiticDelayResult.hpp"
#include "ParasiticDmpModel.hpp"
#include "ParasiticGraph.hpp"

namespace ipw {

#define PWDCACHE (ipw::DelayCache::threadCache())

// 延时计算的缓存全部收口在这里：
// - 每个线程一份（峰值流程按窗口并行，各线程不能共享可写缓存）
// - 结果类缓存用 getOrBuild：命中直接返回，未命中才用 builder 构造
// - RC 图（ParasiticGraph）与每 corner 的节点数值按 net / corner 存成稠密数组
class DelayCache
{
 public:
  DelayCache() = default;
  ~DelayCache() = default;

  static DelayCache& threadCache();

  void clear();

  // ===== 结果类缓存（DMP / Arnoldi）=====

  template <typename Builder>
  ParasiticDmpModel& dmpModel(const ParasiticModelKey& key, Builder&& builder)
  {
    return getOrBuild(_parasitic_dmp_model_map, key, std::forward<Builder>(builder));
  }

  template <typename Builder>
  ParasiticDelayResult& dmpTimingResult(const ArcTimingKey& key, Builder&& builder)
  {
    return getOrBuild(_parasitic_dmp_timing_result_map, key, std::forward<Builder>(builder));
  }

  void cacheDmpDriverResult(const ArcDriverKey& key, const ParasiticDelayResult& timing_result)
  {
    _parasitic_dmp_driver_result_map.insert_or_assign(key, timing_result);
  }

  ParasiticDelayResult* findDmpDriverResult(const ArcDriverKey& key)
  {
    auto iter = _parasitic_dmp_driver_result_map.find(key);
    return iter == _parasitic_dmp_driver_result_map.end() ? nullptr : &iter->second;
  }

  template <typename Builder>
  ParasiticArnoldiModel& arnoldiModel(const ParasiticModelKey& key, Builder&& builder)
  {
    return getOrBuild(_parasitic_arnoldi_model_map, key, std::forward<Builder>(builder));
  }

  template <typename Builder>
  ParasiticDelayResult& arnoldiTimingResult(const ArcTimingKey& key, Builder&& builder)
  {
    return getOrBuild(_parasitic_arnoldi_timing_result_map, key, std::forward<Builder>(builder));
  }

  void cacheArnoldiDriverResult(const ArcDriverKey& key, const ParasiticDelayResult& timing_result)
  {
    _parasitic_arnoldi_driver_result_map.insert_or_assign(key, timing_result);
  }

  ParasiticDelayResult* findArnoldiDriverResult(const ArcDriverKey& key)
  {
    auto iter = _parasitic_arnoldi_driver_result_map.find(key);
    return iter == _parasitic_arnoldi_driver_result_map.end() ? nullptr : &iter->second;
  }

  template <typename Builder>
  ParasiticDelayResult& inputPortResult(const ArcDriverKey& key, Builder&& builder)
  {
    return getOrBuild(_parasitic_input_port_result_map, key, std::forward<Builder>(builder));
  }

  // ===== 索引化的 RC 图与逐 corner 数值（按 net / 节点 id 访问）=====

  // 每节点一组数值：node_load / node_delay / node_impulse 是旧的四层嵌套 map 的等价物，
  // 但按节点 id 存放（id 由 ParasiticGraph 分配），并附带写标记位以复刻 map::count 语义。
  struct ParasiticCornerState
  {
    std::vector<double> node_load;
    std::vector<double> node_delay;
    std::vector<double> node_impulse;
    std::vector<double> load_delay;   // 中间量（每轮 build 前清零）
    std::vector<double> beta;         // 中间量（每轮 build 前清零）
    std::vector<double> node_pin_cap; // 该 corner 下节点的 pin 电容
    std::vector<uint8_t> node_delay_valid;
    std::vector<uint8_t> node_impulse_valid;
    std::vector<uint8_t> visited_scratch;  // 四个遍历共用，避免每次调用分配
    bool pin_cap_ready = false;
  };

  ParasiticGraph& parasiticGraph(const std::string& net_name) { return _parasitic_graph_map[net_name]; }

  bool hasParasiticGraph(const std::string& net_name) const { return _parasitic_graph_map.count(net_name) > 0; }

  ParasiticCornerState& parasiticCornerState(
      const std::string& net_name,
      AnalysisType analysis_type,
      TransType trans_type
  )
  {
    return _parasitic_corner_state_map[net_name][analysis_type][trans_type];
  }

 private:
  template <typename Key, typename Value, typename Builder>
  static Value& getOrBuild(std::map<Key, Value>& cache, const Key& key, Builder&& builder)
  {
    auto [iter, inserted] = cache.try_emplace(key);
    if (inserted) {
      iter->second = builder();
    }
    return iter->second;
  }

  std::map<std::string, ParasiticGraph> _parasitic_graph_map;
  std::map<std::string, std::map<AnalysisType, std::map<TransType, ParasiticCornerState>>> _parasitic_corner_state_map;
  std::map<ParasiticModelKey, ParasiticDmpModel> _parasitic_dmp_model_map;
  std::map<ArcTimingKey, ParasiticDelayResult> _parasitic_dmp_timing_result_map;
  std::map<ArcDriverKey, ParasiticDelayResult> _parasitic_dmp_driver_result_map;
  std::map<ParasiticModelKey, ParasiticArnoldiModel> _parasitic_arnoldi_model_map;
  std::map<ArcTimingKey, ParasiticDelayResult> _parasitic_arnoldi_timing_result_map;
  std::map<ArcDriverKey, ParasiticDelayResult> _parasitic_arnoldi_driver_result_map;
  std::map<ArcDriverKey, ParasiticDelayResult> _parasitic_input_port_result_map;
};

}  // namespace ipw
