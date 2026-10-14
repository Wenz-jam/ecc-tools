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
#include "MJInterface.hpp"

#include "AntennaChecker.hpp"
#include "DEFCutter.hpp"
#include "DataManager.hpp"
#include "DefFlattener.hpp"
#include "DriverAnalyzer.hpp"
#include "FillerInserter.hpp"
#include "IdbDesign.h"
#include "IdbLayout.h"
#include "Logger.hpp"
#include "MetalInserter.hpp"
#include "Monitor.hpp"
#include "Utility.hpp"
#include "def_write.h"
#include "idm.h"

namespace imj {

// public

void MJInterface::destroyInst()
{
  if (_mj_interface_instance != nullptr) {
    delete _mj_interface_instance;
    _mj_interface_instance = nullptr;
  }
}

#if 1  // 外部调用MJ的API

#if 1  // iMJ

void MJInterface::initMJ(std::map<std::string, std::any> config_map)
{
  Logger::initInst();
  // clang-format off
  MJLOG.info(Loc::current(), ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
  MJLOG.info(Loc::current(), "___________  ___________   _____________________________________ ");
  MJLOG.info(Loc::current(), "___(_)__   |/  /_____  /   __  ___/__  __/__    |__  __ \\__  __/");
  MJLOG.info(Loc::current(), "__  /__  /|_/ /___ _  /    _____ \\__  /  __  /| |_  /_/ /_  /   ");
  MJLOG.info(Loc::current(), "_  / _  /  / / / /_/ /     ____/ /_  /   _  ___ |  _, _/_  /     ");
  MJLOG.info(Loc::current(), "/_/  /_/  /_/  \\____/      /____/ /_/    /_/  |_/_/ |_| /_/     ");
  MJLOG.info(Loc::current(), ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
  // clang-format on
  MJLOG.printLogFilePath();
  //////////////////////////////////////////////////////
  //////////////////////////////////////////////////////
  //////////////////////////////////////////////////////
  Monitor monitor;
  MJLOG.info(Loc::current(), "Starting...");

  DataManager::initInst();
  MJDM.input(config_map);

  MJLOG.info(Loc::current(), "Completed", monitor.getStatsInfo());
}

void MJInterface::insertFiller(std::map<std::string, std::any> config_map)
{
  FillerInserter::initInst();
  MJFI.insert(config_map);
  FillerInserter::destroyInst();
}

void MJInterface::insertMetal(std::map<std::string, std::any> config_map)
{
  MetalInserter::initInst();
  MJMI.insert(config_map);
  MetalInserter::destroyInst();
}

void MJInterface::checkAntenna(std::map<std::string, std::any> config_map)
{
  AntennaChecker::initInst();
  MJAC.check(config_map);
  AntennaChecker::destroyInst();
}

void MJInterface::flattenDef(std::map<std::string, std::any> config_map)
{
  DefFlattener::initInst();
  MJDF.flatten(config_map);
  DefFlattener::destroyInst();
}

void MJInterface::analyzeDriver()
{
  DriverAnalyzer::initInst();
  MJDA.analyze();
  DriverAnalyzer::destroyInst();
}

void MJInterface::cutDEF(std::map<std::string, std::any> config_map)
{
  DEFCutter::initInst();
  MJDC.cut(config_map);
  DEFCutter::destroyInst();
}

void MJInterface::destroyMJ()
{
  Monitor monitor;
  MJLOG.info(Loc::current(), "Starting...");

  MJDM.output();
  DataManager::destroyInst();

  MJLOG.info(Loc::current(), "Completed", monitor.getStatsInfo());

  MJLOG.printLogFilePath();
  // clang-format off
  MJLOG.info(Loc::current(), ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
  MJLOG.info(Loc::current(), "___________  ___________   _____________________   _____________________  __ ");
  MJLOG.info(Loc::current(), "___(_)__   |/  /_____  /   ___  ____/___  _/__  | / /___  _/_  ___/__  / / / ");
  MJLOG.info(Loc::current(), "__  /__  /|_/ /___ _  /    __  /_    __  / __   |/ / __  / _____ \\__  /_/ / ");
  MJLOG.info(Loc::current(), "_  / _  /  / / / /_/ /     _  __/   __/ /  _  /|  / __/ /  ____/ /_  __  /   ");
  MJLOG.info(Loc::current(), "/_/  /_/  /_/  \\____/      /_/      /___/  /_/ |_/  /___/  /____/ /_/ /_/   ");
  MJLOG.info(Loc::current(), ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
  // clang-format on
  Logger::destroyInst();
}

#endif

#endif

#if 1  // MJ调用外部的API

#if 1  // TopData

#if 1  // input

void MJInterface::input(std::map<std::string, std::any>& config_map)
{
  wrapConfig(config_map);
}

void MJInterface::wrapConfig(std::map<std::string, std::any>& config_map)
{
  MJDM.getConfig().temp_directory_path = MJUTIL.getConfigValue<std::string>(config_map, "-temp_directory_path", "./mj_temp_directory");
}

void MJInterface::wrapDriverDesign(DAModel& da_model)
{
  idb::IdbDesign* design = dmInst->get_idb_design();
  if (design == nullptr) {
    MJLOG.error(Loc::current(), "No design loaded for driver analysis.");
  }
  da_model.set_design_name(design->get_design_name());
  std::set<std::string> pg_net_name_set = getDriverPGNetNameSet(design);
  da_model.set_excluded_pg_net_num(static_cast<int32_t>(pg_net_name_set.size()));
  std::map<std::string, int32_t> net_idx_map;
  for (int32_t special = 0; special < 2; special++) {
    int32_t count = special ? design->get_special_net_list()->get_num() : design->get_net_list()->get_num();
    for (int32_t idx = 0; idx < count; idx++) {
      idb::IdbNet* net = special ? nullptr : design->get_net_list()->get_net_list()[idx];
      idb::IdbSpecialNet* pg_net = special ? design->get_special_net_list()->get_net_list()[idx] : nullptr;
      std::string net_name = special ? pg_net->get_net_name() : net->get_net_name();
      if (pg_net_name_set.contains(net_name)) {
        continue;
      }
      DANet da_net;
      da_net.set_name(net_name);
      for (int32_t io = 0; io < 2; io++) {
        std::vector<idb::IdbPin*> pin_list
            = special ? getSpecialNetPinList(design, pg_net, io != 0) : (io ? net->get_io_pins() : net->get_instance_pin_list())->get_pin_list();
        for (idb::IdbPin* pin : pin_list) {
          da_net.get_pin_list().push_back(wrapDriverPin(pin, io != 0));
        }
      }
      if (net_idx_map.contains(da_net.get_name())) {
        std::vector<DAPin>& pin_list = da_model.get_net_list()[net_idx_map[da_net.get_name()]].get_pin_list();
        pin_list.insert(pin_list.end(), da_net.get_pin_list().begin(), da_net.get_pin_list().end());
      } else {
        net_idx_map[da_net.get_name()] = static_cast<int32_t>(da_model.get_net_list().size());
        da_model.get_net_list().push_back(da_net);
      }
    }
  }
}

std::set<std::string> MJInterface::getDriverPGNetNameSet(idb::IdbDesign* design)
{
  std::set<std::string> pg_net_name_set;
  // Classify the whole logical net before wrapping either its NETS or SPECIALNETS record.
  for (int32_t special = 0; special < 2; special++) {
    int32_t count = special ? design->get_special_net_list()->get_num() : design->get_net_list()->get_num();
    for (int32_t idx = 0; idx < count; idx++) {
      idb::IdbNet* net = special ? nullptr : design->get_net_list()->get_net_list()[idx];
      idb::IdbSpecialNet* special_net = special ? design->get_special_net_list()->get_net_list()[idx] : nullptr;
      idb::IdbConnectType type = special ? special_net->get_connect_type() : net->get_connect_type();
      bool is_pg = type == idb::IdbConnectType::kPower || type == idb::IdbConnectType::kGround;
      for (int32_t io = 0; io < 2 && !is_pg; io++) {
        std::vector<idb::IdbPin*> pin_list
            = special ? getSpecialNetPinList(design, special_net, io != 0) : (io ? net->get_io_pins() : net->get_instance_pin_list())->get_pin_list();
        for (idb::IdbPin* pin : pin_list) {
          if (pin->get_term() != nullptr
              && (pin->get_term()->get_type() == idb::IdbConnectType::kPower || pin->get_term()->get_type() == idb::IdbConnectType::kGround)) {
            is_pg = true;
            break;
          }
        }
      }
      if (is_pg) {
        pg_net_name_set.insert(special ? special_net->get_net_name() : net->get_net_name());
      }
    }
  }
  return pg_net_name_set;
}

std::vector<idb::IdbPin*> MJInterface::getSpecialNetPinList(idb::IdbDesign* design, idb::IdbSpecialNet* net, bool is_io)
{
  std::vector<idb::IdbPin*> pin_list = (is_io ? net->get_io_pins() : net->get_instance_pin_list())->get_pin_list();
  if (!is_io && !net->get_pin_string_list().empty()) {
    std::set<idb::IdbPin*> pin_set(pin_list.begin(), pin_list.end());
    for (idb::IdbInstance* instance : design->get_instance_list()->get_instance_list()) {
      for (const std::string& term_name : net->get_pin_string_list()) {
        idb::IdbPin* pin = instance->get_pin_by_term(term_name);
        if (pin != nullptr && pin_set.insert(pin).second) {
          pin_list.push_back(pin);
        }
      }
    }
  }
  return pin_list;
}

DAPin MJInterface::wrapDriverPin(idb::IdbPin* pin, bool is_io)
{
  DAPin da_pin;
  da_pin.set_pin_name(pin->get_pin_name());
  da_pin.set_is_io(is_io);
  if (!is_io && pin->get_instance() != nullptr) {
    da_pin.set_instance_name(pin->get_instance()->get_name());
    idb::IdbCellMaster* cell_master = pin->get_instance()->get_cell_master();
    da_pin.set_is_pad(cell_master != nullptr && cell_master->is_pad());
  }
  idb::IdbConnectDirection direction = pin->get_term() == nullptr ? idb::IdbConnectDirection::kNone : pin->get_term()->get_direction();
  if (direction == idb::IdbConnectDirection::kInput) {
    da_pin.set_type(is_io ? DAPinType::kDriver : DAPinType::kLoad);
  } else if (direction == idb::IdbConnectDirection::kOutput || direction == idb::IdbConnectDirection::kOutputTriState) {
    da_pin.set_type(is_io ? DAPinType::kLoad : DAPinType::kDriver);
  } else if (direction == idb::IdbConnectDirection::kInOut) {
    da_pin.set_type(DAPinType::kInout);
  }
  return da_pin;
}

void MJInterface::wrapCutDesign(DCModel& dc_model)
{
  if (dmInst->get_idb_design() == nullptr || dmInst->get_idb_layout() == nullptr) {
    MJLOG.error(Loc::current(), "Load LEF and DEF before cut_def.");
  }
  dc_model.set_design_name(dmInst->get_idb_design()->get_design_name());
  wrapCutFloorplan(dc_model);
  wrapCutObjectList(dc_model);
  wrapCutNetList(dc_model);
}

void MJInterface::wrapCutFloorplan(DCModel& dc_model)
{
  idb::IdbLayout* layout = dmInst->get_idb_layout();
  dc_model.set_micron_dbu(layout->get_units()->get_micron_dbu());
  dc_model.set_input_die(wrapCutRect(layout->get_die()->get_bounding_box()));
  if (dc_model.get_micron_dbu() <= 0 || !dc_model.get_input_die().isValid() || layout->get_die()->get_points().size() != 2) {
    MJLOG.error(Loc::current(), "cut_def requires a rectangular DIEAREA and valid database units.");
  }
  int32_t idx = 0;
  for (idb::IdbRow* row : layout->get_rows()->get_row_list()) {
    DCGrid grid;
    grid.set_source_idx(idx++);
    grid.set_origin({row->get_original_coordinate()->get_x(), row->get_original_coordinate()->get_y()});
    grid.set_count({row->get_row_num_x(), row->get_row_num_y()});
    grid.set_step({row->get_step_x(), row->get_step_y()});
    grid.set_extent({row->get_site()->get_width(), row->get_site()->get_height()});
    idb::IdbOrient orient = row->get_orient();
    if (orient == idb::IdbOrient::kW_R90 || orient == idb::IdbOrient::kE_R270 || orient == idb::IdbOrient::kFE_MY90 || orient == idb::IdbOrient::kFW_MX90) {
      std::swap(grid.get_extent()[0], grid.get_extent()[1]);
    }
    dc_model.get_row_list().push_back(grid);
  }
  idx = 0;
  for (idb::IdbTrackGrid* track : layout->get_track_grid_list()->get_track_grid_list()) {
    DCGrid grid;
    grid.set_source_idx(idx++);
    int32_t axis = track->get_track()->get_direction() == idb::IdbTrackDirection::kDirectionX ? 0 : 1;
    grid.set_axis(axis);
    grid.get_origin()[axis] = track->get_track()->get_start();
    grid.get_count()[axis] = track->get_track_num();
    grid.get_step()[axis] = track->get_track()->get_pitch();
    dc_model.get_track_list().push_back(grid);
  }
  idx = 0;
  for (idb::IdbGCellGrid* gcell : layout->get_gcell_grid_list()->get_gcell_grid_list()) {
    DCGrid grid;
    grid.set_source_idx(idx++);
    int32_t axis = gcell->get_direction() == idb::IdbTrackDirection::kDirectionX ? 0 : 1;
    grid.set_axis(axis);
    grid.get_origin()[axis] = gcell->get_start();
    grid.get_count()[axis] = gcell->get_num();
    grid.get_step()[axis] = gcell->get_space();
    dc_model.get_gcell_list().push_back(grid);
  }
}

DCRect MJInterface::wrapCutRect(idb::IdbRect* rect)
{
  return DCRect(rect->get_low_x(), rect->get_low_y(), rect->get_high_x(), rect->get_high_y());
}

void MJInterface::wrapCutObjectList(DCModel& dc_model)
{
  idb::IdbDesign* design = dmInst->get_idb_design();
  int32_t idx = 0;
  for (idb::IdbInstance* instance : design->get_instance_list()->get_instance_list()) {
    DCObject object;
    object.set_source_idx(idx++);
    object.set_name(instance->get_name());
    if (instance->get_status() == idb::IdbPlacementStatus::kPlaced || instance->get_status() == idb::IdbPlacementStatus::kFixed
        || instance->get_status() == idb::IdbPlacementStatus::kCover) {
      object.get_rect_list().push_back(wrapCutRect(instance->get_bounding_box()));
    }
    dc_model.get_instance_list().push_back(object);
  }
  idx = 0;
  for (idb::IdbPin* pin : design->get_io_pin_list()->get_pin_list()) {
    DCObject object;
    object.set_source_idx(idx++);
    object.set_name(pin->get_pin_name());
    for (DCLayerRect& shape : wrapCutPinShapeList(pin)) {
      object.get_rect_list().push_back(shape.get_rect());
    }
    dc_model.get_io_pin_list().push_back(object);
  }
  idx = 0;
  for (idb::IdbBlockage* blockage : design->get_blockage_list()->get_blockage_list()) {
    DCObject object;
    object.set_source_idx(idx++);
    for (idb::IdbRect* rect : blockage->get_rect_list()) {
      object.get_rect_list().push_back(wrapCutRect(rect));
    }
    dc_model.get_blockage_list().push_back(object);
  }
  idx = 0;
  for (idb::IdbFill* fill : design->get_fill_list()->get_fill_list()) {
    if (fill->get_type() == idb::IdbFill::kLayer) {
      DCObject object;
      object.set_source_idx(idx);
      for (idb::IdbRect* rect : fill->get_layer()->get_rect_list()) {
        object.get_rect_list().push_back(wrapCutRect(rect));
      }
      dc_model.get_fill_list().push_back(object);
    } else if (fill->get_type() == idb::IdbFill::kVia) {
      int32_t part_idx = 0;
      for (idb::IdbCoordinate<int32_t>* coord : fill->get_via()->get_coordinate_list()) {
        DCObject object;
        object.set_source_idx(idx);
        object.set_part_idx(part_idx++);
        std::unique_ptr<idb::IdbVia> via(fill->get_via()->get_via()->clone());
        via->set_coordinate(coord->get_x(), coord->get_y());
        object.get_rect_list().push_back(wrapCutViaRect(via.get()));
        dc_model.get_fill_list().push_back(object);
      }
    }
    idx++;
  }
  idx = 0;
  for (idb::IdbRegion* region : design->get_region_list()->get_region_list()) {
    DCObject object;
    object.set_source_idx(idx++);
    for (idb::IdbRect* rect : region->get_boundary()) {
      object.get_rect_list().push_back(wrapCutRect(rect));
    }
    dc_model.get_region_list().push_back(object);
  }
  idx = 0;
  for (idb::IdbSlot* slot : design->get_slot_list()->get_slot_list()) {
    DCObject object;
    object.set_source_idx(idx++);
    for (idb::IdbRect* rect : slot->get_rect_list()) {
      object.get_rect_list().push_back(wrapCutRect(rect));
    }
    dc_model.get_slot_list().push_back(object);
  }
}

std::vector<DCLayerRect> MJInterface::wrapCutPinShapeList(idb::IdbPin* pin)
{
  std::vector<DCLayerRect> shape_list;
  if (pin->get_term() == nullptr) {
    return shape_list;
  }
  std::vector<idb::IdbPlacementStatus> status_list;
  if (pin->is_io_pin()) {
    if (pin->get_term()->is_port_exist()) {
      for (idb::IdbPort* port : pin->get_term()->get_port_list()) {
        status_list.push_back(port->get_placement_status());
      }
    } else {
      status_list.push_back(pin->get_term()->get_placement_status());
    }
  } else if (pin->get_instance() != nullptr) {
    status_list.push_back(pin->get_instance()->get_status());
  }
  if (status_list.empty()) {
    return shape_list;
  }
  for (idb::IdbPlacementStatus status : status_list) {
    if (status != idb::IdbPlacementStatus::kPlaced && status != idb::IdbPlacementStatus::kFixed && status != idb::IdbPlacementStatus::kCover) {
      return shape_list;
    }
  }
  for (idb::IdbLayerShape* shape : pin->get_port_box_list()) {
    if (shape->get_layer() == nullptr || !shape->get_layer()->is_routing()) {
      continue;
    }
    for (idb::IdbRect* rect : shape->get_rect_list()) {
      shape_list.emplace_back(shape->get_layer()->get_name(), wrapCutRect(rect));
    }
  }
  for (idb::IdbVia* via : pin->get_via_list()) {
    std::vector<DCLayerRect> via_shape_list = wrapCutViaShapeList(via);
    shape_list.insert(shape_list.end(), via_shape_list.begin(), via_shape_list.end());
  }
  return shape_list;
}

std::vector<DCLayerRect> MJInterface::wrapCutViaShapeList(idb::IdbVia* via)
{
  std::vector<DCLayerRect> shape_list;
  idb::IdbLayerShape bottom = via->get_bottom_layer_shape();
  idb::IdbLayerShape top = via->get_top_layer_shape();
  for (idb::IdbLayerShape* shape : {&bottom, &top}) {
    for (idb::IdbRect* rect : shape->get_rect_list()) {
      shape_list.emplace_back(shape->get_layer()->get_name(), wrapCutRect(rect));
    }
  }
  return shape_list;
}

DCRect MJInterface::wrapCutViaRect(idb::IdbVia* via)
{
  idb::IdbRect bottom = via->get_bottom_bounding_box();
  idb::IdbRect top = via->get_top_bounding_box();
  idb::IdbRect cut = via->get_cut_bounding_box();
  return DCRect(std::min({bottom.get_low_x(), top.get_low_x(), cut.get_low_x()}), std::min({bottom.get_low_y(), top.get_low_y(), cut.get_low_y()}),
                std::max({bottom.get_high_x(), top.get_high_x(), cut.get_high_x()}), std::max({bottom.get_high_y(), top.get_high_y(), cut.get_high_y()}));
}

void MJInterface::wrapCutNetList(DCModel& dc_model)
{
  idb::IdbDesign* design = dmInst->get_idb_design();
  std::unordered_map<idb::IdbInstance*, int32_t> instance_idx_map;
  std::unordered_map<idb::IdbPin*, int32_t> io_idx_map;
  for (DCObject& object : dc_model.get_instance_list()) {
    instance_idx_map[design->get_instance_list()->get_instance_list()[object.get_source_idx()]] = object.get_source_idx();
  }
  for (DCObject& object : dc_model.get_io_pin_list()) {
    io_idx_map[design->get_io_pin_list()->get_pin_list()[object.get_source_idx()]] = object.get_source_idx();
  }
  for (int32_t special = 0; special < 2; special++) {
    int32_t count = special ? design->get_special_net_list()->get_num() : design->get_net_list()->get_num();
    for (int32_t idx = 0; idx < count; idx++) {
      DCNet net;
      net.set_source_idx(idx);
      net.set_is_special(special != 0);
      idb::IdbNet* regular_net = special ? nullptr : design->get_net_list()->get_net_list()[idx];
      idb::IdbSpecialNet* special_net = special ? design->get_special_net_list()->get_net_list()[idx] : nullptr;
      net.set_name(special ? special_net->get_net_name() : regular_net->get_net_name());
      idb::IdbConnectType net_type = special ? special_net->get_connect_type() : regular_net->get_connect_type();
      net.set_is_pg(net_type == idb::IdbConnectType::kPower || net_type == idb::IdbConnectType::kGround);
      for (int32_t io = 0; io < 2; io++) {
        std::vector<idb::IdbPin*> pin_list = special ? getSpecialNetPinList(design, special_net, io != 0)
                                                     : (io ? regular_net->get_io_pins() : regular_net->get_instance_pin_list())->get_pin_list();
        for (idb::IdbPin* pin : pin_list) {
          DCConnection connection;
          connection.set_is_io(io != 0);
          connection.set_pin_name(pin->get_pin_name());
          connection.set_pin_type(wrapCutPinType(pin));
          connection.set_shape_list(wrapCutPinShapeList(pin));
          if ((io && !io_idx_map.contains(pin)) || (!io && !instance_idx_map.contains(pin->get_instance()))) {
            MJLOG.error(Loc::current(), "Invalid pin owner on net ", net.get_name());
          }
          connection.set_object_idx(io ? io_idx_map[pin] : instance_idx_map[pin->get_instance()]);
          net.get_connection_list().push_back(connection);
        }
      }
      wrapCutRouteList(net);
      dc_model.get_net_list().push_back(net);
    }
  }
}

DCPinType MJInterface::wrapCutPinType(idb::IdbPin* pin)
{
  idb::IdbConnectDirection direction = pin->get_term() == nullptr ? idb::IdbConnectDirection::kNone : pin->get_term()->get_direction();
  if (direction == idb::IdbConnectDirection::kInput) {
    return DCPinType::kInput;
  }
  if (direction == idb::IdbConnectDirection::kOutput || direction == idb::IdbConnectDirection::kOutputTriState) {
    return DCPinType::kOutput;
  }
  if (direction == idb::IdbConnectDirection::kInOut) {
    return DCPinType::kInout;
  }
  return DCPinType::kNone;
}

void MJInterface::wrapCutRouteList(DCNet& net)
{
  idb::IdbDesign* design = dmInst->get_idb_design();
  idb::IdbNet* regular_net = net.get_is_special() ? nullptr : design->get_net_list()->get_net_list()[net.get_source_idx()];
  idb::IdbSpecialNet* special_net = net.get_is_special() ? design->get_special_net_list()->get_net_list()[net.get_source_idx()] : nullptr;
  int32_t wire_num = net.get_is_special() ? special_net->get_wire_list()->get_num() : regular_net->get_wire_list()->get_wire_list().size();
  for (int32_t wire_idx = 0; wire_idx < wire_num; wire_idx++) {
    idb::IdbRegularWire* regular_wire = net.get_is_special() ? nullptr : regular_net->get_wire_list()->get_wire_list()[wire_idx];
    idb::IdbSpecialWire* special_wire = net.get_is_special() ? special_net->get_wire_list()->get_wire_list()[wire_idx] : nullptr;
    int32_t segment_num = net.get_is_special() ? special_wire->get_num() : regular_wire->get_num();
    for (int32_t segment_idx = 0; segment_idx < segment_num; segment_idx++) {
      idb::IdbRegularWireSegment* regular = net.get_is_special() ? nullptr : regular_wire->get_segment_list()[segment_idx];
      idb::IdbSpecialWireSegment* special = net.get_is_special() ? special_wire->get_segment_list()[segment_idx] : nullptr;
      idb::IdbLayer* layer = net.get_is_special() ? special->get_layer() : regular->get_layer();
      if (layer == nullptr || !layer->is_routing()) {
        MJLOG.error(Loc::current(), "Missing routing layer on net ", net.get_name());
      }
      DCRoute route;
      route.set_wire_idx(wire_idx);
      route.set_segment_idx(segment_idx);
      route.set_layer_name(layer->get_name());
      bool is_rect = net.get_is_special() ? special->is_rect() : regular->is_rect();
      if (is_rect) {
        idb::IdbRect rect = net.get_is_special() ? *special->get_delta_rect() : regular->get_segment_rect();
        route.set_rect(wrapCutRect(&rect));
        net.get_route_list().push_back(route);
      } else {
        std::vector<idb::IdbCoordinate<int32_t>*>& points = net.get_is_special() ? special->get_point_list() : regular->get_point_list();
        int32_t width = net.get_is_special() ? special->get_route_width() : dynamic_cast<idb::IdbLayerRouting*>(layer)->get_width();
        for (int32_t point_idx = 1; point_idx < static_cast<int32_t>(points.size()); point_idx++) {
          idb::IdbCoordinate<int32_t>* start = points[point_idx - 1];
          idb::IdbCoordinate<int32_t>* end = points[point_idx];
          if (!net.get_is_special() && (regular->is_virtual(start) || regular->is_virtual(end))) {
            continue;
          }
          int32_t start_ext = (net.get_is_special() ? special->get_point_ext(start) : regular->get_point_ext(start)).value_or(width / 2);
          int32_t end_ext = (net.get_is_special() ? special->get_point_ext(end) : regular->get_point_ext(end)).value_or(width / 2);
          route.set_rect(wrapCutWireRect(start->get_x(), start->get_y(), end->get_x(), end->get_y(), width, start_ext, end_ext));
          net.get_route_list().push_back(route);
        }
      }
      int32_t via_num = net.get_is_special() ? (special->is_via() ? 1 : 0) : regular->get_via_list().size();
      for (int32_t via_idx = 0; via_idx < via_num; via_idx++) {
        idb::IdbVia* via = net.get_is_special() ? special->get_via() : regular->get_via_list()[via_idx];
        route.set_via_idx(via_idx);
        route.set_rect(wrapCutViaRect(via));
        route.set_via_shape_list(wrapCutViaShapeList(via));
        net.get_route_list().push_back(route);
      }
    }
  }
}

DCRect MJInterface::wrapCutWireRect(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t width, int32_t ext1, int32_t ext2)
{
  if (width <= 0 || (x1 != x2 && y1 != y2)) {
    MJLOG.error(Loc::current(), "cut_def supports positive-width Manhattan wires only.");
  }
  if (x1 > x2 || y1 > y2) {
    std::swap(x1, x2);
    std::swap(y1, y2);
    std::swap(ext1, ext2);
  }
  return y1 == y2 ? DCRect(x1 - ext1, y1 - width / 2, x2 + ext2, y1 - width / 2 + width) : DCRect(x1 - width / 2, y1 - ext1, x1 - width / 2 + width, y2 + ext2);
}

#endif

#if 1  // output

void MJInterface::output()
{
}

void MJInterface::writeCutDEF(DCModel& dc_model, std::string path)
{
  idb::IdbLayout layout;
  idb::IdbDefService service(&layout);
  idb::IdbDesign* design = service.get_design();
  idb::IdbDesign* source = dmInst->get_idb_design();
  design->set_design_name(dc_model.get_design_name() + "_cut");
  design->set_version("5.8");
  design->get_units()->set_microns_dbu(dc_model.get_micron_dbu());
  design->set_bus_bit_chars(new idb::IdbBusBitChars(*source->get_bus_bit_chars()));
  writeCutFloorplan(dc_model, &layout);
  for (idb::IdbVia* via : source->get_via_list()->get_via_list()) {
    design->get_via_list()->add_via(via->clone());
  }
  writeCutInstanceList(dc_model, design);
  writeCutPinList(dc_model, design);
  writeCutNetList(dc_model, design);
  writeCutShapeList(dc_model, design);
  idb::DefWrite writer(&service);
  if (!writer.writeDb(path.c_str())) {
    MJLOG.error(Loc::current(), "Failed to write cropped DEF: ", path);
  }
}

void MJInterface::writeCutFloorplan(DCModel& dc_model, idb::IdbLayout* layout)
{
  idb::IdbLayout* source = dmInst->get_idb_layout();
  layout->get_units()->set_microns_dbu(dc_model.get_micron_dbu());
  DCRect& die = dc_model.get_die();
  layout->initDie(die.get_ll_x(), die.get_ll_y(), die.get_ur_x(), die.get_ur_y());
  for (DCGrid& row : dc_model.get_row_list()) {
    if (row.get_count()[0] == 0 || row.get_count()[1] == 0) {
      continue;
    }
    idb::IdbRow* source_row = source->get_rows()->get_row_list()[row.get_source_idx()];
    layout->get_rows()->createRow(source_row->get_name(), source_row->get_site(), row.get_origin()[0], row.get_origin()[1], source_row->get_orient(),
                                  row.get_count()[0], row.get_count()[1], row.get_step()[0], row.get_step()[1]);
  }
  for (DCGrid& track : dc_model.get_track_list()) {
    int32_t axis = track.get_axis();
    if (track.get_count()[axis] == 0) {
      continue;
    }
    idb::IdbTrackGrid* source_track = source->get_track_grid_list()->get_track_grid_list()[track.get_source_idx()];
    idb::IdbTrackGrid* output_track = layout->get_track_grid_list()->add_track_grid();
    output_track->get_track()->set_direction(source_track->get_track()->get_direction());
    output_track->get_track()->set_start(track.get_origin()[axis]);
    output_track->get_track()->set_pitch(track.get_step()[axis]);
    output_track->set_track_number(track.get_count()[axis]);
    for (idb::IdbLayer* layer : source_track->get_layer_list()) {
      output_track->add_layer_list(layer);
    }
  }
  for (DCGrid& gcell : dc_model.get_gcell_list()) {
    int32_t axis = gcell.get_axis();
    if (gcell.get_count()[axis] == 0) {
      continue;
    }
    idb::IdbGCellGrid* output_grid = layout->get_gcell_grid_list()->add_gcell_grid();
    output_grid->set_direction(source->get_gcell_grid_list()->get_gcell_grid_list()[gcell.get_source_idx()]->get_direction());
    output_grid->set_start(gcell.get_origin()[axis]);
    output_grid->set_num(gcell.get_count()[axis]);
    output_grid->set_space(gcell.get_step()[axis]);
  }
}

void MJInterface::writeCutInstanceList(DCModel& dc_model, idb::IdbDesign* design)
{
  for (DCObject& object : dc_model.get_instance_list()) {
    if (!object.get_retained()) {
      continue;
    }
    idb::IdbInstance* source = dmInst->get_idb_design()->get_instance_list()->get_instance_list()[object.get_source_idx()];
    idb::IdbInstance* instance = design->get_instance_list()->add_instance(source->get_name());
    instance->set_cell_master(source->get_cell_master());
    instance->set_type(source->get_type());
    instance->set_orient(source->get_orient(), false);
    instance->set_coodinate(source->get_coordinate()->get_x(), source->get_coordinate()->get_y(), false);
    instance->set_status(source->get_status());
    instance->set_weight(source->get_weight());
    instance->set_bounding_box();
    instance->set_pin_list_coodinate();
    instance->set_obs_box_list();
  }
}

void MJInterface::writeCutPinList(DCModel& dc_model, idb::IdbDesign* design)
{
  std::set<int32_t> replaced_pin_set;
  for (DCIOPin& pin : dc_model.get_cut_pin_list()) {
    if (pin.get_source_idx() >= 0) {
      replaced_pin_set.insert(pin.get_source_idx());
    }
  }
  for (DCObject& object : dc_model.get_io_pin_list()) {
    if (!object.get_retained() || replaced_pin_set.contains(object.get_source_idx())) {
      continue;
    }
    idb::IdbPin* source = dmInst->get_idb_design()->get_io_pin_list()->get_pin_list()[object.get_source_idx()];
    idb::IdbPin* pin = design->get_io_pin_list()->add_pin_list(source->get_pin_name());
    pin->set_as_io();
    // Terms and LEF masters are read-only references during serialization; the input design outlives service.
    pin->set_term(source->get_term());
    pin->set_net_name(source->get_net_name().empty() && source->get_net() != nullptr ? source->get_net()->get_net_name() : source->get_net_name());
    pin->set_orient(source->get_orient());
    pin->set_location(source->get_location()->get_x(), source->get_location()->get_y());
  }
  for (DCIOPin& io_pin : dc_model.get_cut_pin_list()) {
    DCNet& net = dc_model.get_net_list()[io_pin.get_net_idx()];
    idb::IdbDesign* source = dmInst->get_idb_design();
    idb::IdbConnectType type = net.get_is_special() ? source->get_special_net_list()->get_net_list()[net.get_source_idx()]->get_connect_type()
                                                    : source->get_net_list()->get_net_list()[net.get_source_idx()]->get_connect_type();
    if (io_pin.get_source_idx() >= 0) {
      type = source->get_io_pin_list()->get_pin_list()[io_pin.get_source_idx()]->get_term()->get_type();
    }
    idb::IdbPin* pin = design->get_io_pin_list()->add_pin_list(io_pin.get_name());
    pin->set_as_io();
    pin->set_net_name(net.get_name());
    pin->set_orient(idb::IdbOrient::kN_R0);
    idb::IdbTerm* term = pin->set_term();
    term->set_name(io_pin.get_name());
    term->set_direction(GetDCPinTypeName()(io_pin.get_type()));
    term->set_type(type == idb::IdbConnectType::kNone ? idb::IdbConnectType::kSignal : type);
    term->set_has_port(true);
    term->set_special(net.get_is_special());
    term->set_placement_status_fix();
    for (DCLayerRect& shape : io_pin.get_shape_list()) {
      DCRect& rect = shape.get_rect();
      idb::IdbPort* port = term->add_port();
      port->set_placement_status_fix();
      port->set_orient(idb::IdbOrient::kN_R0);
      port->set_coordinate(rect.get_ll_x(), rect.get_ll_y());
      idb::IdbLayerShape* layer_shape = port->add_layer_shape();
      layer_shape->set_type_rect();
      layer_shape->set_layer(dmInst->get_idb_layout()->get_layers()->find_layer(shape.get_layer_name()));
      layer_shape->add_rect(0, 0, rect.get_ur_x() - rect.get_ll_x(), rect.get_ur_y() - rect.get_ll_y());
    }
    DCRect& rect = io_pin.get_shape_list().front().get_rect();
    pin->set_location(rect.get_ll_x(), rect.get_ll_y());
    pin->set_average_coordinate(rect.get_ll_x() + (rect.get_ur_x() - rect.get_ll_x()) / 2, rect.get_ll_y() + (rect.get_ur_y() - rect.get_ll_y()) / 2);
    pin->set_port_layer_shape();
    pin->set_pin_bounding_box();
  }
}

void MJInterface::writeCutNetList(DCModel& dc_model, idb::IdbDesign* design)
{
  idb::IdbDesign* source = dmInst->get_idb_design();
  for (DCNet& net : dc_model.get_net_list()) {
    if (!net.get_retained()) {
      continue;
    }
    idb::IdbNet* source_net = net.get_is_special() ? nullptr : source->get_net_list()->get_net_list()[net.get_source_idx()];
    idb::IdbSpecialNet* source_pg_net = net.get_is_special() ? source->get_special_net_list()->get_net_list()[net.get_source_idx()] : nullptr;
    idb::IdbNet* output_net = net.get_is_special() ? nullptr : design->get_net_list()->add_net(net.get_name(), source_net->get_connect_type());
    idb::IdbSpecialNet* output_pg_net = net.get_is_special() ? design->get_special_net_list()->add_net(net.get_name()) : nullptr;
    if (net.get_is_special()) {
      output_pg_net->set_connect_type(source_pg_net->get_connect_type());
    }
    for (DCConnection& connection : net.get_connection_list()) {
      if (!connection.get_retained()) {
        continue;
      }
      idb::IdbPin* pin = nullptr;
      if (connection.get_is_io()) {
        pin = design->get_io_pin_list()->find_pin(connection.get_pin_name());
      } else {
        std::string instance_name = dc_model.get_instance_list()[connection.get_object_idx()].get_name();
        pin = design->get_instance_list()->find_instance(instance_name)->get_pin(connection.get_pin_name());
      }
      if (pin == nullptr) {
        MJLOG.error(Loc::current(), "Failed to rebuild retained pin ", connection.get_pin_name(), " on ", net.get_name());
      }
      connectCutPin(pin, output_net, output_pg_net);
    }
    if (net.get_cut_pin_idx() >= 0) {
      connectCutPin(design->get_io_pin_list()->find_pin(dc_model.get_cut_pin_list()[net.get_cut_pin_idx()].get_name()), output_net, output_pg_net);
    }
    std::map<int32_t, idb::IdbRegularWire*> wire_map;
    std::map<int32_t, idb::IdbSpecialWire*> pg_wire_map;
    for (DCRoute& route : net.get_route_list()) {
      if (!route.get_retained()) {
        continue;
      }
      DCRect& rect = route.get_rect();
      if (net.get_is_special()) {
        idb::IdbSpecialWire* source_wire = source_pg_net->get_wire_list()->get_wire_list()[route.get_wire_idx()];
        idb::IdbSpecialWireSegment* source_segment = source_wire->get_segment_list()[route.get_segment_idx()];
        if (!pg_wire_map.contains(route.get_wire_idx())) {
          idb::IdbSpecialWire* wire = output_pg_net->get_wire_list()->add_wire();
          wire->set_wire_state(source_wire->get_wire_state());
          wire->set_shield_name(source_wire->get_shiled_name());
          pg_wire_map[route.get_wire_idx()] = wire;
        }
        idb::IdbSpecialWireSegment* segment = pg_wire_map[route.get_wire_idx()]->add_segment();
        segment->set_layer(dmInst->get_idb_layout()->get_layers()->find_layer(route.get_layer_name()));
        segment->set_layer_as_new();
        segment->set_shape_type(source_segment->get_shape_type());
        if (route.get_via_idx() >= 0) {
          idb::IdbVia* via = source_segment->get_via()->clone();
          segment->set_is_via(true);
          segment->set_via(via);
          segment->set_route_width(0);
          segment->add_point(via->get_coordinate()->get_x(), via->get_coordinate()->get_y());
        } else {
          // Explicit zero end extensions serialize the exact clipped rectangle.
          if (rect.get_ur_x() - rect.get_ll_x() >= rect.get_ur_y() - rect.get_ll_y()) {
            int32_t width = rect.get_ur_y() - rect.get_ll_y();
            segment->set_route_width(width);
            segment->add_flush_point(rect.get_ll_x(), rect.get_ll_y() + width / 2, 0);
            segment->add_flush_point(rect.get_ur_x(), rect.get_ll_y() + width / 2, 0);
          } else {
            int32_t width = rect.get_ur_x() - rect.get_ll_x();
            segment->set_route_width(width);
            segment->add_flush_point(rect.get_ll_x() + width / 2, rect.get_ll_y(), 0);
            segment->add_flush_point(rect.get_ll_x() + width / 2, rect.get_ur_y(), 0);
          }
        }
      } else {
        idb::IdbRegularWire* source_wire = source_net->get_wire_list()->get_wire_list()[route.get_wire_idx()];
        idb::IdbRegularWireSegment* source_segment = source_wire->get_segment_list()[route.get_segment_idx()];
        if (!wire_map.contains(route.get_wire_idx())) {
          idb::IdbRegularWire* wire = output_net->get_wire_list()->add_wire();
          wire->set_wire_state(source_wire->get_wire_statement());
          wire->set_shield_name(source_wire->get_shiled_name());
          wire_map[route.get_wire_idx()] = wire;
        }
        idb::IdbRegularWireSegment* segment = wire_map[route.get_wire_idx()]->add_segment();
        segment->set_layer(dmInst->get_idb_layout()->get_layers()->find_layer(route.get_layer_name()));
        segment->set_layer_as_new();
        if (route.get_via_idx() >= 0) {
          idb::IdbVia* via = source_segment->get_via_list()[route.get_via_idx()];
          segment->set_is_via(true);
          segment->copy_via(via);
          segment->add_point(via->get_coordinate()->get_x(), via->get_coordinate()->get_y());
        } else {
          segment->set_is_rect(true);
          segment->add_point(rect.get_ll_x(), rect.get_ll_y());
          segment->set_delta_rect(0, 0, rect.get_ur_x() - rect.get_ll_x(), rect.get_ur_y() - rect.get_ll_y());
        }
      }
    }
  }
}

void MJInterface::connectCutPin(idb::IdbPin* pin, idb::IdbNet* net, idb::IdbSpecialNet* special_net)
{
  if (special_net != nullptr) {
    pin->set_special_net(special_net);
    pin->set_net_name(special_net->get_net_name());
    if (pin->is_io_pin()) {
      special_net->add_io_pin(pin);
    } else {
      special_net->add_instance_pin(pin);
    }
  } else {
    pin->set_net(net);
    pin->set_net_name(net->get_net_name());
    if (pin->is_io_pin()) {
      net->add_io_pin(pin);
    } else {
      net->add_instance_pin(pin);
    }
  }
}

void MJInterface::writeCutShapeList(DCModel& dc_model, idb::IdbDesign* design)
{
  idb::IdbDesign* source = dmInst->get_idb_design();
  std::vector<idb::IdbBlockage*> blockage_list = source->get_blockage_list()->get_blockage_list();
  for (DCObject& object : dc_model.get_blockage_list()) {
    if (!object.get_retained()) {
      continue;
    }
    idb::IdbBlockage* input = blockage_list[object.get_source_idx()];
    idb::IdbBlockage* output = nullptr;
    if (input->is_routing_blockage()) {
      idb::IdbRoutingBlockage* routing = dynamic_cast<idb::IdbRoutingBlockage*>(input);
      idb::IdbRoutingBlockage* copy = design->get_blockage_list()->add_blockage_routing(routing->get_layer_name());
      copy->set_layer(routing->get_layer());
      copy->set_slots(routing->is_slots());
      copy->set_fills(routing->is_fills());
      copy->set_except_pgnet(routing->is_except_pgnet());
      copy->set_min_spacing(routing->get_min_spacing());
      copy->set_effective_width(routing->get_effective_width());
      output = copy;
    } else {
      idb::IdbPlacementBlockage* placement = dynamic_cast<idb::IdbPlacementBlockage*>(input);
      idb::IdbPlacementBlockage* copy = design->get_blockage_list()->add_blockage_placement();
      copy->set_soft(placement->is_soft());
      copy->set_fills(placement->is_partial());
      copy->set_max_density(placement->get_max_density());
      output = copy;
    }
    output->set_pushdown(input->is_pushdown());
    idb::IdbInstance* instance = design->get_instance_list()->find_instance(input->get_instance_name());
    if (instance != nullptr) {
      output->set_instance(instance);
      output->set_instance_name(instance->get_name());
    }
    for (DCRect& rect : object.get_rect_list()) {
      output->add_rect(rect.get_ll_x(), rect.get_ll_y(), rect.get_ur_x(), rect.get_ur_y());
    }
  }
  for (DCObject& object : dc_model.get_fill_list()) {
    if (!object.get_retained()) {
      continue;
    }
    idb::IdbFill* input = source->get_fill_list()->get_fill_list()[object.get_source_idx()];
    if (object.get_part_idx() < 0) {
      idb::IdbFillLayer* output = design->get_fill_list()->add_fill_layer(input->get_layer()->get_layer());
      for (DCRect& rect : object.get_rect_list()) {
        output->add_rect(rect.get_ll_x(), rect.get_ll_y(), rect.get_ur_x(), rect.get_ur_y());
      }
    } else {
      idb::IdbFillVia* output = design->get_fill_list()->add_fill_via(input->get_via()->get_via()->clone());
      idb::IdbCoordinate<int32_t>* coord = input->get_via()->get_coordinate_list()[object.get_part_idx()];
      output->add_coordinate(coord->get_x(), coord->get_y());
    }
  }
  for (DCObject& object : dc_model.get_region_list()) {
    if (!object.get_retained()) {
      continue;
    }
    idb::IdbRegion* input = source->get_region_list()->get_region_list()[object.get_source_idx()];
    idb::IdbRegion* output = design->get_region_list()->add_region(input->get_name());
    output->set_type(input->get_type());
    for (DCRect& rect : object.get_rect_list()) {
      output->add_boundary(rect.get_ll_x(), rect.get_ll_y(), rect.get_ur_x(), rect.get_ur_y());
    }
    for (idb::IdbInstance* input_instance : input->get_instance_list()) {
      idb::IdbInstance* instance = design->get_instance_list()->find_instance(input_instance->get_name());
      if (instance != nullptr) {
        output->add_instance(instance);
        instance->set_region(output);
      }
    }
  }
  for (idb::IdbGroup* input : source->get_group_list()->get_group_list()) {
    std::vector<idb::IdbInstance*> instance_list;
    for (idb::IdbInstance* input_instance : input->get_instance_list()->get_instance_list()) {
      idb::IdbInstance* instance = design->get_instance_list()->find_instance(input_instance->get_name());
      if (instance != nullptr) {
        instance_list.push_back(instance);
      }
    }
    if (instance_list.empty()) {
      continue;
    }
    idb::IdbGroup* output = design->get_group_list()->add_group(input->get_group_name());
    if (input->get_region() != nullptr) {
      output->set_region(design->get_region_list()->find_region(input->get_region()->get_name()));
    }
    for (idb::IdbInstance* instance : instance_list) {
      output->add_instance(instance);
    }
  }
  for (DCObject& object : dc_model.get_slot_list()) {
    if (!object.get_retained()) {
      continue;
    }
    idb::IdbSlot* input = source->get_slot_list()->get_slot_list()[object.get_source_idx()];
    idb::IdbSlot* output = design->get_slot_list()->add_slot();
    output->set_layer_name(input->get_layer_name());
    output->set_layer(input->get_layer());
    for (DCRect& rect : object.get_rect_list()) {
      output->add_rect(rect.get_ll_x(), rect.get_ll_y(), rect.get_ur_x(), rect.get_ur_y());
    }
  }
}

#endif

#endif

#endif

// private

MJInterface* MJInterface::_mj_interface_instance = nullptr;

}  // namespace imj
