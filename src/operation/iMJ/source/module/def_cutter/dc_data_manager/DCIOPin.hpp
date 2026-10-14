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

#include "DCLayerRect.hpp"
#include "DCPinType.hpp"

namespace imj {

class DCIOPin
{
 public:
  DCIOPin() = default;
  ~DCIOPin() = default;
  // getter
  std::string& get_name() { return _name; }
  int32_t get_source_idx() { return _source_idx; }
  int32_t get_net_idx() { return _net_idx; }
  DCPinType get_type() { return _type; }
  std::vector<DCLayerRect>& get_shape_list() { return _shape_list; }
  // const getter
  const std::string& get_name() const { return _name; }
  int32_t get_source_idx() const { return _source_idx; }
  int32_t get_net_idx() const { return _net_idx; }
  DCPinType get_type() const { return _type; }
  const std::vector<DCLayerRect>& get_shape_list() const { return _shape_list; }
  // setter
  void set_name(const std::string& name) { _name = name; }
  void set_source_idx(int32_t source_idx) { _source_idx = source_idx; }
  void set_net_idx(int32_t net_idx) { _net_idx = net_idx; }
  void set_type(DCPinType type) { _type = type; }
  void set_shape_list(const std::vector<DCLayerRect>& shape_list) { _shape_list = shape_list; }
  // function

 private:
  std::string _name;
  int32_t _source_idx = -1;
  int32_t _net_idx = -1;
  DCPinType _type = DCPinType::kNone;
  std::vector<DCLayerRect> _shape_list;
};

}  // namespace imj
