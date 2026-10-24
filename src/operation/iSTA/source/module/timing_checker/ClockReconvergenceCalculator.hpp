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

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Database.hpp"
namespace ista {
// Compares already selected launch/capture clocks. Does not choose checks or path winners.
class ClockReconvergenceCalculator
{
 public:
  explicit ClockReconvergenceCalculator(Database& database) : _database(database) {}
  double correction(const std::string& launch_pin, TransType launch_trans_type, std::string_view launch_clock, const std::string& capture_pin,
                    TransType capture_trans_type, std::string_view capture_clock, AnalysisType analysis_type, std::string& common_pin_name,
                    std::optional<TransType> launch_source = std::nullopt, std::optional<TransType> capture_source = std::nullopt);

 private:
  struct ClockPathPoint
  {
    int32_t vertex = -1;
    TransType transition;
    std::optional<TransType> source;
  };
  Database& _database;
  double getClockCommonPathDelayDelta(const ClockPathPoint& common_pin, std::string_view clock_name);
  std::vector<ClockPathPoint> getClockPathPinList(const std::string& clock_pin_name, std::string_view clock_name, AnalysisType analysis_type,
                                                  TransType trans_type, std::optional<TransType> source);
  AnalysisType getCaptureAnalysisType(AnalysisType analysis_type);
};
}  // namespace ista
