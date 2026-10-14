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

#include "DCModel.hpp"

namespace imj {

#define MJDC (imj::DEFCutter::getInst())

class DEFCutter
{
 public:
  static void initInst();
  static DEFCutter& getInst();
  static void destroyInst();
  // function
  void cut(std::map<std::string, std::any>& config_map);

 private:
  static DEFCutter* _dc_instance;

  DEFCutter() = default;
  DEFCutter(const DEFCutter& other) = delete;
  DEFCutter(DEFCutter&& other) = delete;
  ~DEFCutter() = default;
  DEFCutter& operator=(const DEFCutter& other) = delete;
  DEFCutter& operator=(DEFCutter&& other) = delete;
  // function
  DCModel initDCModel(std::map<std::string, std::any>& config_map);
  void cutGridList(DCModel& dc_model);
  void cutGrid(DCGrid& grid, DCRect& boundary, bool is_row);
  void selectObjectList(std::vector<DCObject>& object_list, DCRect& boundary);
  void cutNetList(DCModel& dc_model);
  void buildBoundaryShapeList(DCNet& net, DCRoute& route, DCRect& input_rect, DCRect& die);
  void buildIOPinList(DCModel& dc_model);
  DCPinType getIOPinType(DCModel& dc_model, std::vector<int32_t>& net_idx_list);
  void buildFallbackShapeList(DCModel& dc_model, std::vector<int32_t>& net_idx_list, DCIOPin& pin);
  void cutObjectList(std::vector<DCObject>& object_list, DCRect& boundary);
#if 1  // output
  void writeDEF(DCModel& dc_model);
#endif
#if 1  // exhibit
  void writeReport(DCModel& dc_model);
#endif
};

}  // namespace imj
