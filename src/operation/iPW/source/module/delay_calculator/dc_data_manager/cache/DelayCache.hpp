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

namespace ipw {

#define PWDCACHE (ipw::DelayCache::threadCache())

// 延时计算的缓存全部收口在这里：
// - 每个线程一份（峰值流程按窗口并行，各线程不能共享可写缓存）
// - 结果类缓存用 getOrBuild：命中直接返回，未命中才用 builder 构造
// - 老 RC 树遍历的四张表按 net / 节点直接取内层 map（树递归需要按节点反复读写）
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

  // ===== 老 RC 树遍历缓存（按 net / 节点访问）=====

  std::map<std::string, std::vector<std::pair<std::string, double>>>& resistorMap(const std::string& net_name)
  {
    return _parasitic_resistor_map[net_name];
  }

  bool hasResistorMap(const std::string& net_name) const { return _parasitic_resistor_map.count(net_name) > 0; }

  std::map<std::string, double>& nodeLoadMap(
      const std::string& net_name,
      AnalysisType analysis_type,
      TransType trans_type
  )
  {
    return _parasitic_node_load_map[net_name][analysis_type][trans_type];
  }

  std::map<std::string, double>& nodeDelayMap(
      const std::string& net_name,
      AnalysisType analysis_type,
      TransType trans_type
  )
  {
    return _parasitic_node_delay_map[net_name][analysis_type][trans_type];
  }

  std::map<std::string, double>& nodeImpulseMap(
      const std::string& net_name,
      AnalysisType analysis_type,
      TransType trans_type
  )
  {
    return _parasitic_node_impulse_map[net_name][analysis_type][trans_type];
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

  std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, double>>>> _parasitic_resistor_map;
  std::map<std::string, std::map<AnalysisType, std::map<TransType, std::map<std::string, double>>>>
      _parasitic_node_load_map;
  std::map<std::string, std::map<AnalysisType, std::map<TransType, std::map<std::string, double>>>>
      _parasitic_node_delay_map;
  std::map<std::string, std::map<AnalysisType, std::map<TransType, std::map<std::string, double>>>>
      _parasitic_node_impulse_map;
  std::map<ParasiticModelKey, ParasiticDmpModel> _parasitic_dmp_model_map;
  std::map<ArcTimingKey, ParasiticDelayResult> _parasitic_dmp_timing_result_map;
  std::map<ArcDriverKey, ParasiticDelayResult> _parasitic_dmp_driver_result_map;
  std::map<ParasiticModelKey, ParasiticArnoldiModel> _parasitic_arnoldi_model_map;
  std::map<ArcTimingKey, ParasiticDelayResult> _parasitic_arnoldi_timing_result_map;
  std::map<ArcDriverKey, ParasiticDelayResult> _parasitic_arnoldi_driver_result_map;
  std::map<ArcDriverKey, ParasiticDelayResult> _parasitic_input_port_result_map;
};

}  // namespace ipw
