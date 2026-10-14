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
#include "DEFCutter.hpp"

#include "DataManager.hpp"
#include "MJInterface.hpp"
#include "Utility.hpp"

namespace imj {

void DEFCutter::initInst()
{
  if (_dc_instance == nullptr) {
    _dc_instance = new DEFCutter();
  }
}

DEFCutter& DEFCutter::getInst()
{
  if (_dc_instance == nullptr) {
    MJLOG.error(Loc::current(), "The instance not initialized!");
  }
  return *_dc_instance;
}

void DEFCutter::destroyInst()
{
  delete _dc_instance;
  _dc_instance = nullptr;
}

void DEFCutter::cut(std::map<std::string, std::any>& config_map)
{
  Monitor monitor;
  MJLOG.info(Loc::current(), "Starting...");
  DCModel dc_model = initDCModel(config_map);
  cutGridList(dc_model);
  selectObjectList(dc_model.get_instance_list(), dc_model.get_die());
  selectObjectList(dc_model.get_io_pin_list(), dc_model.get_die());
  cutNetList(dc_model);
  buildIOPinList(dc_model);
  cutObjectList(dc_model.get_blockage_list(), dc_model.get_die());
  cutObjectList(dc_model.get_fill_list(), dc_model.get_die());
  cutObjectList(dc_model.get_region_list(), dc_model.get_die());
  cutObjectList(dc_model.get_slot_list(), dc_model.get_die());
  writeDEF(dc_model);
  writeReport(dc_model);
  MJLOG.info(Loc::current(), "Completed", monitor.getStatsInfo());
}

DCModel DEFCutter::initDCModel(std::map<std::string, std::any>& config_map)
{
  DCModel dc_model;
  MJI.wrapCutDesign(dc_model);
  for (const auto& [key, value] : config_map) {
    if (key != "-rect") {
      MJLOG.error(Loc::current(), "Unsupported cut_def option: ", key);
    }
  }
  std::istringstream stream(MJUTIL.getConfigValue<std::string>(config_map, "-rect", ""));
  std::array<int32_t, 4> coord;
  for (int32_t idx = 0; idx < 4; idx++) {
    double value = 0.0;
    if (!(stream >> value) || !std::isfinite(value)) {
      MJLOG.error(Loc::current(), "cut_def -rect requires four finite micron coordinates.");
    }
    double dbu = std::round(value * dc_model.get_micron_dbu());
    if (!std::isfinite(dbu) || dbu < INT32_MIN || dbu > INT32_MAX) {
      MJLOG.error(Loc::current(), "cut_def coordinate is outside the DBU range.");
    }
    coord[idx] = static_cast<int32_t>(dbu);
  }
  std::string extra;
  if (stream >> extra) {
    MJLOG.error(Loc::current(), "cut_def -rect requires exactly four coordinates.");
  }
  DCRect rect(coord[0], coord[1], coord[2], coord[3]);
  if (!rect.isValid() || !dc_model.get_input_die().contains(rect)) {
    MJLOG.error(Loc::current(), "cut_def rectangle must have positive area and lie inside the input die.");
  }
  dc_model.set_die(rect);
  return dc_model;
}

void DEFCutter::cutGridList(DCModel& dc_model)
{
  for (DCGrid& row : dc_model.get_row_list()) {
    cutGrid(row, dc_model.get_die(), true);
  }
  for (DCGrid& track : dc_model.get_track_list()) {
    cutGrid(track, dc_model.get_die(), false);
  }
  for (DCGrid& gcell : dc_model.get_gcell_list()) {
    cutGrid(gcell, dc_model.get_die(), false);
  }
}

void DEFCutter::cutGrid(DCGrid& grid, DCRect& boundary, bool is_row)
{
  std::array<int32_t, 2> low = {boundary.get_ll_x(), boundary.get_ll_y()};
  std::array<int32_t, 2> high = {boundary.get_ur_x(), boundary.get_ur_y()};
  for (int32_t axis = 0; axis < 2; axis++) {
    if (!is_row && axis != grid.get_axis()) {
      continue;
    }
    int32_t origin = grid.get_origin()[axis];
    int32_t count = grid.get_count()[axis];
    int32_t step = grid.get_step()[axis];
    int32_t extent = grid.get_extent()[axis];
    if (count <= 0 || step < 0 || (count > 1 && step == 0)) {
      MJLOG.error(Loc::current(), "Invalid row/track grid in the input DEF.");
    }
    if (count == 1) {
      if (origin < low[axis] || static_cast<double>(origin) + extent > high[axis]) {
        grid.get_count()[axis] = 0;
      }
      continue;
    }
    double begin = std::max(0.0, std::ceil((static_cast<double>(low[axis]) - origin) / step));
    double end = std::min(static_cast<double>(count - 1), std::floor((static_cast<double>(high[axis]) - extent - origin) / step));
    if (begin > end) {
      grid.get_count()[axis] = 0;
    } else {
      grid.get_origin()[axis] = static_cast<int32_t>(origin + begin * step);
      grid.get_count()[axis] = static_cast<int32_t>(end - begin + 1);
    }
  }
}

void DEFCutter::selectObjectList(std::vector<DCObject>& object_list, DCRect& boundary)
{
  for (DCObject& object : object_list) {
    bool retained = !object.get_rect_list().empty();
    for (DCRect& rect : object.get_rect_list()) {
      retained = retained && boundary.contains(rect);
    }
    object.set_retained(retained);
  }
}

void DEFCutter::cutNetList(DCModel& dc_model)
{
  for (DCNet& net : dc_model.get_net_list()) {
    for (DCConnection& connection : net.get_connection_list()) {
      std::vector<DCObject>& objects = connection.get_is_io() ? dc_model.get_io_pin_list() : dc_model.get_instance_list();
      connection.set_retained(objects[connection.get_object_idx()].get_retained());
      if (connection.get_retained()) {
        net.set_retained(true);
      } else {
        net.set_removed_pin_num(net.get_removed_pin_num() + 1);
        for (DCLayerRect& shape : connection.get_shape_list()) {
          DCRect rect = shape.get_rect().intersect(dc_model.get_die());
          if (rect.isValid()) {
            net.get_io_shape_list().emplace_back(shape.get_layer_name(), rect);
          }
        }
      }
    }
    std::vector<DCRoute> via_metal_list;
    for (DCRoute& route : net.get_route_list()) {
      if (route.get_via_idx() >= 0) {
        route.set_retained(dc_model.get_die().contains(route.get_rect()));
        if (!route.get_retained()) {
          // A cut via is no longer a legal via. Keep its in-die routing metal as IO access shapes.
          for (DCLayerRect& shape : route.get_via_shape_list()) {
            DCRect rect = shape.get_rect().intersect(dc_model.get_die());
            if (!rect.isValid()) {
              continue;
            }
            DCRoute metal;
            metal.set_wire_idx(route.get_wire_idx());
            metal.set_segment_idx(route.get_segment_idx());
            metal.set_layer_name(shape.get_layer_name());
            metal.set_rect(rect);
            metal.set_retained(true);
            via_metal_list.push_back(metal);
            net.get_io_shape_list().emplace_back(shape.get_layer_name(), rect);
            net.set_retained(true);
          }
        }
      } else {
        DCRect input_rect = route.get_rect();
        route.set_rect(route.get_rect().intersect(dc_model.get_die()));
        route.set_retained(route.get_rect().isValid());
        if (route.get_retained()) {
          buildBoundaryShapeList(net, route, input_rect, dc_model.get_die());
        }
      }
      net.set_retained(net.get_retained() || route.get_retained());
    }
    net.get_route_list().insert(net.get_route_list().end(), via_metal_list.begin(), via_metal_list.end());
  }
}

void DEFCutter::buildBoundaryShapeList(DCNet& net, DCRoute& route, DCRect& input_rect, DCRect& die)
{
  DCRect& rect = route.get_rect();
  int32_t depth = std::min(rect.get_ur_x() - rect.get_ll_x(), rect.get_ur_y() - rect.get_ll_y());
  if (input_rect.get_ll_x() < die.get_ll_x()) {
    net.get_io_shape_list().emplace_back(route.get_layer_name(), DCRect(rect.get_ll_x(), rect.get_ll_y(), rect.get_ll_x() + depth, rect.get_ur_y()));
  }
  if (input_rect.get_ur_x() > die.get_ur_x()) {
    net.get_io_shape_list().emplace_back(route.get_layer_name(), DCRect(rect.get_ur_x() - depth, rect.get_ll_y(), rect.get_ur_x(), rect.get_ur_y()));
  }
  if (input_rect.get_ll_y() < die.get_ll_y()) {
    net.get_io_shape_list().emplace_back(route.get_layer_name(), DCRect(rect.get_ll_x(), rect.get_ll_y(), rect.get_ur_x(), rect.get_ll_y() + depth));
  }
  if (input_rect.get_ur_y() > die.get_ur_y()) {
    net.get_io_shape_list().emplace_back(route.get_layer_name(), DCRect(rect.get_ll_x(), rect.get_ur_y() - depth, rect.get_ur_x(), rect.get_ur_y()));
  }
}

void DEFCutter::buildIOPinList(DCModel& dc_model)
{
  std::set<std::string> pin_name_set;
  for (DCObject& pin : dc_model.get_io_pin_list()) {
    if (pin.get_retained()) {
      pin_name_set.insert(pin.get_name());
    }
  }
  std::map<std::string, std::vector<int32_t>> net_idx_map;
  for (int32_t idx = 0; idx < static_cast<int32_t>(dc_model.get_net_list().size()); idx++) {
    DCNet& net = dc_model.get_net_list()[idx];
    if (net.get_retained()) {
      net_idx_map[net.get_name()].push_back(idx);
    }
  }
  int32_t name_idx = 0;
  for (auto& [net_name, net_idx_list] : net_idx_map) {
    DCIOPin pin;
    pin.set_net_idx(net_idx_list.front());
    bool needs_io = false;
    for (int32_t net_idx : net_idx_list) {
      DCNet& net = dc_model.get_net_list()[net_idx];
      needs_io = needs_io || net.get_removed_pin_num() > 0 || !net.get_io_shape_list().empty();
      pin.get_shape_list().insert(pin.get_shape_list().end(), net.get_io_shape_list().begin(), net.get_io_shape_list().end());
      if (net.get_is_pg()) {
        pin.set_net_idx(net_idx);
      }
    }
    if (!needs_io) {
      continue;
    }
    pin.set_type(getIOPinType(dc_model, net_idx_list));
    // Reuse a retained top-level pin: extra access PORTs do not create another logical driver.
    for (int32_t net_idx : net_idx_list) {
      for (DCConnection& connection : dc_model.get_net_list()[net_idx].get_connection_list()) {
        if (connection.get_is_io() && connection.get_retained() && pin.get_source_idx() < 0) {
          pin.set_source_idx(connection.get_object_idx());
          pin.set_name(connection.get_pin_name());
          pin.set_type(connection.get_pin_type() == DCPinType::kNone ? DCPinType::kInout : connection.get_pin_type());
          pin.get_shape_list().insert(pin.get_shape_list().end(), connection.get_shape_list().begin(), connection.get_shape_list().end());
        }
      }
    }
    if (pin.get_shape_list().empty()) {
      buildFallbackShapeList(dc_model, net_idx_list, pin);
    }
    if (pin.get_shape_list().empty()) {
      MJLOG.error(Loc::current(), "Cannot create IO for cut net ", net_name, ": no retained routing or placed pin shape.");
    }
    std::set<std::tuple<std::string, int32_t, int32_t, int32_t, int32_t>> shape_set;
    std::vector<DCLayerRect> shape_list;
    for (DCLayerRect& shape : pin.get_shape_list()) {
      DCRect& rect = shape.get_rect();
      if (shape_set.emplace(shape.get_layer_name(), rect.get_ll_x(), rect.get_ll_y(), rect.get_ur_x(), rect.get_ur_y()).second) {
        shape_list.push_back(shape);
      }
    }
    pin.set_shape_list(shape_list);
    if (pin.get_source_idx() < 0) {
      std::string pin_name = net_name;
      while (!pin_name_set.insert(pin_name).second) {
        pin_name = net_name + "__cut_" + std::to_string(name_idx++);
      }
      pin.set_name(pin_name);
    }
    for (int32_t net_idx : net_idx_list) {
      dc_model.get_net_list()[net_idx].set_cut_pin_idx(static_cast<int32_t>(dc_model.get_cut_pin_list().size()));
    }
    dc_model.get_cut_pin_list().push_back(pin);
  }
}

DCPinType DEFCutter::getIOPinType(DCModel& dc_model, std::vector<int32_t>& net_idx_list)
{
  bool internal_driver = false;
  bool external_driver = false;
  bool external_load = false;
  bool unresolved = false;
  for (int32_t net_idx : net_idx_list) {
    DCNet& net = dc_model.get_net_list()[net_idx];
    if (net.get_is_pg()) {
      return DCPinType::kInout;
    }
    for (DCConnection& connection : net.get_connection_list()) {
      if (connection.get_pin_type() == DCPinType::kNone || connection.get_pin_type() == DCPinType::kInout) {
        unresolved = true;
        continue;
      }
      bool is_driver = connection.get_is_io() ? connection.get_pin_type() == DCPinType::kInput : connection.get_pin_type() == DCPinType::kOutput;
      internal_driver = internal_driver || (connection.get_retained() && is_driver);
      external_driver = external_driver || (!connection.get_retained() && is_driver);
      external_load = external_load || (!connection.get_retained() && !is_driver);
    }
  }
  if (unresolved || (internal_driver && external_driver)) {
    return DCPinType::kInout;
  }
  if (external_driver) {
    return DCPinType::kInput;
  }
  if (internal_driver || external_load) {
    return DCPinType::kOutput;
  }
  return DCPinType::kInout;
}

void DEFCutter::buildFallbackShapeList(DCModel& dc_model, std::vector<int32_t>& net_idx_list, DCIOPin& pin)
{
  for (int32_t net_idx : net_idx_list) {
    DCNet& net = dc_model.get_net_list()[net_idx];
    for (DCRoute& route : net.get_route_list()) {
      if (!route.get_retained()) {
        continue;
      }
      if (route.get_via_idx() < 0) {
        pin.get_shape_list().emplace_back(route.get_layer_name(), route.get_rect());
      } else {
        pin.get_shape_list().insert(pin.get_shape_list().end(), route.get_via_shape_list().begin(), route.get_via_shape_list().end());
      }
    }
  }
  if (!pin.get_shape_list().empty()) {
    return;
  }
  for (int32_t net_idx : net_idx_list) {
    for (DCConnection& connection : dc_model.get_net_list()[net_idx].get_connection_list()) {
      if (!connection.get_retained()) {
        continue;
      }
      for (DCLayerRect& shape : connection.get_shape_list()) {
        DCRect rect = shape.get_rect().intersect(dc_model.get_die());
        if (rect.isValid()) {
          pin.get_shape_list().emplace_back(shape.get_layer_name(), rect);
        }
      }
    }
  }
}

void DEFCutter::cutObjectList(std::vector<DCObject>& object_list, DCRect& boundary)
{
  for (DCObject& object : object_list) {
    std::vector<DCRect> rect_list;
    for (DCRect& rect : object.get_rect_list()) {
      // Via fills are indivisible; part_idx identifies a placement of a via fill.
      DCRect clipped = object.get_part_idx() < 0 ? rect.intersect(boundary) : boundary.contains(rect) ? rect : DCRect();
      if (clipped.isValid()) {
        rect_list.push_back(clipped);
      }
    }
    object.set_rect_list(rect_list);
    object.set_retained(!rect_list.empty());
  }
}

#if 1  // output

void DEFCutter::writeDEF(DCModel& dc_model)
{
  MJI.writeCutDEF(dc_model, MJDM.getConfig().dc_temp_directory_path + "cut.def");
}

#endif

#if 1  // exhibit

void DEFCutter::writeReport(DCModel& dc_model)
{
  std::string path = MJDM.getConfig().dc_temp_directory_path + "cut_report.rpt";
  std::ofstream* stream = MJUTIL.getOutputFileStream(path);
  *stream << "Design : " << dc_model.get_design_name() << "\nCoordinates : absolute, DBU"
          << "\nDie : " << dc_model.get_die().get_ll_x() << " " << dc_model.get_die().get_ll_y() << " " << dc_model.get_die().get_ur_x() << " "
          << dc_model.get_die().get_ur_y() << "\nOnly complete placed instances, ports and vias are retained.\n"
          << "Cut nets have IO pins on retained routing or terminal metal; each logical net reuses or adds one pin with multiple PORT shapes.\n"
          << "IO directions follow inside/outside driver roles; PG and unresolved directions use INOUT.\n";
  int32_t kept_instance_num = 0;
  for (DCObject& object : dc_model.get_instance_list()) {
    if (object.get_retained()) {
      kept_instance_num++;
    } else {
      *stream << "RemovedInstance : " << object.get_name() << "\n";
    }
  }
  int32_t kept_net_num = 0;
  int32_t cut_net_num = 0;
  for (DCNet& net : dc_model.get_net_list()) {
    if (!net.get_retained()) {
      continue;
    }
    kept_net_num++;
    if (net.get_cut_pin_idx() >= 0) {
      cut_net_num++;
      *stream << "CutNet : " << net.get_name() << " removed_pins=" << net.get_removed_pin_num() << "\n";
    }
  }
  int32_t added_pin_num = 0;
  for (DCIOPin& pin : dc_model.get_cut_pin_list()) {
    added_pin_num += pin.get_source_idx() < 0;
    *stream << "IOPin : " << pin.get_name() << " net=" << dc_model.get_net_list()[pin.get_net_idx()].get_name()
            << " direction=" << GetDCPinTypeName()(pin.get_type()) << " ports=" << pin.get_shape_list().size() << " reused=" << (pin.get_source_idx() >= 0)
            << "\n";
    for (DCLayerRect& shape : pin.get_shape_list()) {
      DCRect& rect = shape.get_rect();
      *stream << "  " << shape.get_layer_name() << " " << rect.get_ll_x() << " " << rect.get_ll_y() << " " << rect.get_ur_x() << " " << rect.get_ur_y() << "\n";
    }
  }
  *stream << "RetainedInstances : " << kept_instance_num << "/" << dc_model.get_instance_list().size() << "\nRetainedNets : " << kept_net_num << "/"
          << dc_model.get_net_list().size() << "\nCutNets : " << cut_net_num << "\nAddedIOPins : " << added_pin_num
          << "\nExtendedIOPins : " << dc_model.get_cut_pin_list().size() - added_pin_num << "\n";
  MJUTIL.closeFileStream(stream);
  MJLOG.info(Loc::current(), "Retained instances=", kept_instance_num, ", nets=", kept_net_num, ", cut nets=", cut_net_num,
             ". DEF: ", MJDM.getConfig().dc_temp_directory_path, "cut.def; report: ", path);
}

#endif

DEFCutter* DEFCutter::_dc_instance = nullptr;

}  // namespace imj
