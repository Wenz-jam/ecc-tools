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
#include <cstring>
#include <functional>
#include <string>
#include <tuple>

#include "AnalysisType.hpp"
#include "TransType.hpp"

namespace ipw {

class TimingArc;

inline std::uint64_t getDoubleBits(double value)
{
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

struct ParasiticModelKey
{
  std::string net_name;
  std::string source_node_name;
  AnalysisType analysis_type = AnalysisType::kNone;
  TransType trans_type = TransType::kNone;

  bool operator<(const ParasiticModelKey& other) const
  {
    return std::tie(net_name, source_node_name, analysis_type, trans_type)
           < std::tie(other.net_name, other.source_node_name, other.analysis_type, other.trans_type);
  }
};

struct ArcTimingKey
{
  std::string output_pin;
  const TimingArc* timing_arc = nullptr;
  AnalysisType analysis_type = AnalysisType::kNone;
  TransType trans_type = TransType::kNone;
  double input_slew = 0.0;

  bool operator<(const ArcTimingKey& other) const
  {
    if (output_pin != other.output_pin) {
      return output_pin < other.output_pin;
    }
    if (timing_arc != other.timing_arc) {
      return std::less<const TimingArc*>()(timing_arc, other.timing_arc);
    }
    if (analysis_type != other.analysis_type) {
      return analysis_type < other.analysis_type;
    }
    if (trans_type != other.trans_type) {
      return trans_type < other.trans_type;
    }
    return getDoubleBits(input_slew) < getDoubleBits(other.input_slew);
  }
};

struct ArcDriverKey
{
  std::string name;
  AnalysisType analysis_type = AnalysisType::kNone;
  TransType trans_type = TransType::kNone;
  double driver_slew = 0.0;

  bool operator<(const ArcDriverKey& other) const
  {
    if (name != other.name) {
      return name < other.name;
    }
    if (analysis_type != other.analysis_type) {
      return analysis_type < other.analysis_type;
    }
    if (trans_type != other.trans_type) {
      return trans_type < other.trans_type;
    }
    return getDoubleBits(driver_slew) < getDoubleBits(other.driver_slew);
  }
};

}  // namespace ipw
