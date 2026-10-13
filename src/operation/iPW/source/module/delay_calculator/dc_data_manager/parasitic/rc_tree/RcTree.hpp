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

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "AnalysisType.hpp"
#include "TransType.hpp"

namespace ipw {

class ParasiticNet;
class ParasiticDmpModel;

// RC 树的通用处理：电阻图、树展平、Pi 降阶（DM​P 与 Arnoldi 共用）
void buildParasiticResistorMap(
    ParasiticNet& parasitic_net,
    std::map<std::string, std::vector<std::pair<std::string, double>>>& resistor_map
);
void flattenRcTree(
    ParasiticNet& parasitic_net,
    std::string& source_node_name,
    AnalysisType analysis_type,
    TransType trans_type,
    std::vector<std::string>& node_name_list,
    std::vector<int32_t>& parent_idx_list,
    std::vector<double>& resistance_list,
    std::vector<double>& capacitance_list
);
void buildPiModel(
    ParasiticDmpModel& dmp_model,
    std::vector<int32_t>& parent_idx_list,
    std::vector<double>& resistance_list,
    std::vector<double>& capacitance_list
);

}  // namespace ipw
