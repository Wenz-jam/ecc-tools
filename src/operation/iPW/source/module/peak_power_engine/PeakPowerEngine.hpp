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
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//
// See the Mulan PSL v2 for more details.
// ***************************************************************************************
#pragma once

#include "PWHeader.hpp"
#include "VcdParser.hh"

namespace ipw {

#define PWPE (ipw::PeakPowerEngine::getInst())

class PeakPowerEngine
{
 public:
  static void initInst();
  static PeakPowerEngine& getInst();
  static void destroyInst();
  // function
  void run(const std::string& vcd_file_path);

 private:
  // self
  static PeakPowerEngine* _pe_instance;

  PeakPowerEngine() = default;
  PeakPowerEngine(const PeakPowerEngine& other) = delete;
  PeakPowerEngine(PeakPowerEngine&& other) = delete;
  ~PeakPowerEngine() = default;
  PeakPowerEngine& operator=(const PeakPowerEngine& other) = delete;
  PeakPowerEngine& operator=(PeakPowerEngine&& other) = delete;
  // function
  double check(const std::string& vcd_file_path);
  double resolveWindowLength();
  void updatePeakPower(const vcd::VcdWindowSignalActivity& window_activity);
};

}  // namespace ipw
