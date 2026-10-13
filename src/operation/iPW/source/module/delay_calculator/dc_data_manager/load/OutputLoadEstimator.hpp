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

#include <string>

#include "AnalysisType.hpp"
#include "TransType.hpp"

namespace ipw {

class Arc;
class Net;
class ParasiticNet;

// 负载估算：管脚电容、网表负载与寄生网络的输出负载
double getPinCapacitance(std::string& pin_name, AnalysisType analysis_type, TransType trans_type);
double getParasiticNodeLoad(
    ParasiticNet& parasitic_net,
    std::string& node_name,
    AnalysisType analysis_type,
    TransType trans_type
);

double getArcOutputLoad(Arc& arc, AnalysisType analysis_type, TransType output_trans_type);
double getOutputPinLoad(std::string& output_pin, AnalysisType analysis_type, TransType output_trans_type);
double getNetOutputLoad(const std::string& net_name, Net& net, AnalysisType analysis_type, TransType output_trans_type);
double getParasiticNetOutputLoad(
    Net& net,
    ParasiticNet& parasitic_net,
    AnalysisType analysis_type,
    TransType trans_type
);
double getParasiticTotalLoad(ParasiticNet& parasitic_net, AnalysisType analysis_type, TransType trans_type);

}  // namespace ipw
