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
#include "DAModel.hpp"
#include "DCModel.hpp"
#include "DEFCutter.hpp"
#include "DriverAnalyzer.hpp"
#include "IdbDesign.h"
#include "IdbLayout.h"
#include "Logger.hpp"
#include "MJInterface.hpp"
#include "idm.h"

class DesignToolsTest
{
 public:
  void run()
  {
    setenv("ECC_LOGGER_THROW_ON_ERROR", "1", 1);
    std::filesystem::create_directories(_output);
    loadDEF();
    MJI.initMJ({{"-temp_directory_path", _output.string()}});
    testDriverAnalysis();
    testDEFCut();
    MJI.destroyMJ();
    testNetlist();
    dmInst->reset();
  }

 private:
  std::filesystem::path _data = std::filesystem::path(__FILE__).parent_path() / "data";
  std::filesystem::path _output = std::filesystem::temp_directory_path() / ("test_imj_design_tools_" + std::to_string(getpid()));

  void loadDEF()
  {
    dmInst->reset();
    require(dmInst->readLef(std::vector<std::string>{(_data / "tech.lef").string()}, true), "read tech");
    require(dmInst->readLef(std::vector<std::string>{(_data / "cell.lef").string()}), "read cell");
    require(dmInst->readDef((_data / "design.def.in").string()), "read DEF");
    // IDB currently reads group records but not their member callbacks.
    idb::IdbDesign* design = dmInst->get_idb_design();
    design->get_group_list()->get_group_list()[0]->add_instance(design->get_instance_list()->find_instance("u0"));
    design->get_group_list()->get_group_list()[0]->add_instance(design->get_instance_list()->find_instance("u1"));
    design->get_group_list()->get_group_list()[1]->add_instance(design->get_instance_list()->find_instance("outside"));
    design->get_group_list()->get_group_list()[1]->add_instance(design->get_instance_list()->find_instance("u0"));
  }

  void require(bool condition, const std::string& message)
  {
    if (!condition) {
      throw std::runtime_error(message);
    }
  }

  std::string readText(const std::filesystem::path& path)
  {
    std::ifstream stream(path);
    require(stream.good(), "missing " + path.string());
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
  }

  void testDriverAnalysis()
  {
    MJI.analyzeDriver({});
    std::string report = readText(_output / "driver_analyzer/driver_report.rpt");
    require(report.find("MultiDriverNets : 1") != std::string::npos, "multiple outputs");
    require(report.find("UnresolvedNets : 1") != std::string::npos, "inout classification");
    require(report.find("UndrivenNets : 2") != std::string::npos, "undriven net");
    require(report.find("ExcludedPGNets : 1") != std::string::npos, "PG exclusion");
    require(report.find("INSTANCE u0/Y driver") != std::string::npos, "driver identity");
    idb::IdbNet* net = dmInst->get_idb_design()->get_net_list()->find_net("multi");
    idb::IdbPin* pin = dmInst->get_idb_design()->get_instance_list()->find_instance("u0")->get_pin("Y");
    net->get_instance_pin_list()->get_pin_list().push_back(pin);
    MJI.analyzeDriver({});
    report = readText(_output / "driver_analyzer/driver_report.rpt");
    require(report.find("Drivers : 2") != std::string::npos && report.find("Drivers : 3") == std::string::npos, "deduplicate pins");
    net->get_instance_pin_list()->get_pin_list().pop_back();
  }

  void testDEFCut()
  {
    idb::IdbDesign* source = dmInst->get_idb_design();
    idb::IdbPin* bus_pin = source->get_io_pin_list()->find_pin("bus");
    idb::IdbPlacementStatus status = bus_pin->get_term()->get_placement_status();
    bus_pin->get_term()->set_placement_status_unplace();
    imj::DCModel unplaced_model;
    MJI.wrapCutDesign(unplaced_model);
    for (imj::DCObject& object : unplaced_model.get_io_pin_list()) {
      if (object.get_name() == "bus") {
        require(object.get_rect_list().empty(), "unplaced IO treated as physical geometry");
      }
    }
    bus_pin->get_term()->set_placement_status(status);
    require(dmInst->saveDef((_output / "before.def").string()), "save before");
    MJI.cutDEF({{"-rect", std::string("2.5 2.5 12 12")}});
    require(dmInst->get_idb_design() == source, "active design changed");
    require(dmInst->saveDef((_output / "after.def").string()), "save after");
    require(readText(_output / "before.def") == readText(_output / "after.def"), "source mutated");
    std::filesystem::copy_file(_output / "def_cutter/cut.def", _output / "die.def");
    std::string report = readText(_output / "def_cutter/cut_report.rpt");
    require(report.find("RetainedInstances : 2/5") != std::string::npos, "partial/outside/unplaced instance");
    require(report.find("CutNet : a removed_pins=1") != std::string::npos, "boundary connectivity report");
    bool rejected = false;
    try {
      MJI.cutDEF({{"-rect", std::string("12 12 2 2")}});
    } catch (const std::runtime_error&) {
      rejected = true;
      imj::DEFCutter::destroyInst();
    }
    require(rejected, "invalid rectangle accepted");
    rejected = false;
    try {
      MJI.cutDEF({{"-rect", std::string("3.1 3.1 3.2 3.2")}, {"-boundary", std::string("core")}});
    } catch (const std::runtime_error&) {
      rejected = true;
      imj::DEFCutter::destroyInst();
    }
    require(rejected, "removed option accepted");

    loadDEF();
    require(dmInst->readDef((_output / "die.def").string()), "read cut die DEF");
    checkCut(2500, 2500, 12000, 12000);
    idb::IdbLayout* layout = dmInst->get_idb_layout();
    require(layout->get_rows()->get_row_num() == 9, "row count");
    for (idb::IdbRow* row : layout->get_rows()->get_row_list()) {
      require(row->get_original_coordinate()->get_x() == 3000 && row->get_row_num_x() == 9 && row->get_step_x() == 1000, "site alignment");
    }
    imj::DCModel model;
    MJI.wrapCutDesign(model);
    int32_t via_num = 0;
    bool has_flush_wire = false;
    bool has_clipped_via_top = false;
    for (imj::DCNet& net : model.get_net_list()) {
      for (imj::DCRoute& route : net.get_route_list()) {
        require(model.get_input_die().contains(route.get_rect()), "route outside crop");
        if (net.get_name() == "a" && route.get_rect().get_ll_x() == 5100 && route.get_rect().get_ll_y() == 6950 && route.get_rect().get_ur_x() == 6000
            && route.get_rect().get_ur_y() == 7050) {
          has_flush_wire = true;
        }
        if (net.get_name() == "multi" && route.get_layer_name() == "M2" && route.get_via_idx() < 0 && route.get_rect().get_ll_x() == 11915
            && route.get_rect().get_ur_x() == 12000) {
          has_clipped_via_top = true;
        }
        if (route.get_via_idx() >= 0) {
          via_num++;
        }
      }
    }
    require(has_flush_wire, "negative/zero wire end extensions changed");
    require(has_clipped_via_top, "clipped via must retain its upper routing enclosure on M2");
    require(via_num == 2, "retain complete signal/PG vias, exclude boundary via");
    require(dmInst->get_idb_design()->get_fill_list()->get_num_fill() == 2, "fill clipping");
    require(dmInst->get_idb_design()->get_instance_list()->find_instance("u1")->get_orient() == idb::IdbOrient::kFS_MX, "orientation changed");
    checkIOPins(model);
    require(dmInst->saveDef((_output / "roundtrip.def").string()), "save standalone DEF");
    require(dmInst->readDef((_output / "roundtrip.def").string()), "read standalone DEF");
    imj::DCModel roundtrip_model;
    MJI.wrapCutDesign(roundtrip_model);
    checkIOPins(roundtrip_model);
    MJI.analyzeDriver({});
    report = readText(_output / "driver_analyzer/driver_report.rpt");
    require(report.find("MultiDriverNets : 1") != std::string::npos, "cropping must not introduce multiple drivers");
    require(report.find("UndrivenNets : 1") != std::string::npos, "external driver must be replaced with an input IO");
    dmInst->saveVerilog((_output / "cut.v").string());
    require(dmInst->addVerilog((_output / "cut.v").string(), "crop_test_cut"), "read standalone netlist");
    idb::IdbDesign* netlist = dmInst->get_netlist_idb_design();
    require(netlist->get_instance_list()->get_num() == 2, "standalone netlist instance count");
    require(netlist->get_io_pin_list()->find_pin("out")->get_term()->get_direction() == idb::IdbConnectDirection::kInput, "standalone input pin");
    require(netlist->get_io_pin_list()->find_pin("transit")->get_term()->get_direction() == idb::IdbConnectDirection::kInOut, "standalone inout pin");
  }

  void checkCut(int32_t ll_x, int32_t ll_y, int32_t ur_x, int32_t ur_y)
  {
    idb::IdbRect* die = dmInst->get_idb_layout()->get_die()->get_bounding_box();
    require(die->get_low_x() == ll_x && die->get_low_y() == ll_y && die->get_high_x() == ur_x && die->get_high_y() == ur_y, "die boundary");
    idb::IdbDesign* design = dmInst->get_idb_design();
    require(design->get_instance_list()->get_num() == 2, "component count");
    require(design->get_io_pin_list()->get_pin_num() == 7, "preserved and generated IO pin count");
    require(design->get_net_list()->find_net("a")->get_instance_pin_list()->get_pin_num() == 1, "deleted instance reference");
    require(design->get_special_net_list()->find_net("VDD")->get_instance_pin_list()->get_pin_num() == 2, "PG connectivity");
    require(design->get_group_list()->get_num() == 2, "group references");
  }

  void checkIOPins(imj::DCModel& model)
  {
    idb::IdbDesign* design = dmInst->get_idb_design();
    for (idb::IdbPin* pin : design->get_io_pin_list()->get_pin_list()) {
      require(pin->get_net() != nullptr || pin->get_special_net() != nullptr, "unconnected IO pin");
      std::vector<imj::DCLayerRect> shapes = MJI.wrapCutPinShapeList(pin);
      require(!shapes.empty(), "IO without physical access");
      for (imj::DCLayerRect& shape : shapes) {
        require(model.get_input_die().contains(shape.get_rect()), "IO shape outside die");
      }
    }
    require(design->get_io_pin_list()->find_pin("multi")->get_term()->get_direction() == idb::IdbConnectDirection::kOutput, "internal driver produces OUTPUT");
    require(design->get_io_pin_list()->find_pin("out")->get_term()->get_direction() == idb::IdbConnectDirection::kInput, "external driver produces INPUT");
    require(design->get_io_pin_list()->find_pin("transit")->get_term()->get_direction() == idb::IdbConnectDirection::kInOut, "unknown transit direction");
    require(design->get_io_pin_list()->find_pin("VDD")->get_term()->get_direction() == idb::IdbConnectDirection::kInOut, "PG direction");
    require(design->get_io_pin_list()->find_pin("VDD")->get_term()->get_type() == idb::IdbConnectType::kPower, "PG pin use");
    require(design->get_net_list()->find_net("transit")->get_io_pins()->get_pin_num() == 1, "one logical pin for two crossings");
    require(design->get_io_pin_list()->find_pin("transit")->get_term()->get_port_number() == 2, "both transit crossings have PORT shapes");
    std::set<std::tuple<std::string, int32_t, int32_t, int32_t, int32_t>> transit_shapes;
    for (imj::DCLayerRect& shape : MJI.wrapCutPinShapeList(design->get_io_pin_list()->find_pin("transit"))) {
      imj::DCRect& rect = shape.get_rect();
      transit_shapes.emplace(shape.get_layer_name(), rect.get_ll_x(), rect.get_ll_y(), rect.get_ur_x(), rect.get_ur_y());
    }
    require(transit_shapes
                == std::set<std::tuple<std::string, int32_t, int32_t, int32_t, int32_t>>{{"M2", 8950, 2500, 9050, 2600}, {"M2", 8950, 11900, 9050, 12000}},
            "transit IO access must match the retained wire at both die edges");
    require(design->get_net_list()->find_net("a")->get_io_pins()->get_pin_num() == 1, "reuse retained INPUT rather than adding a driver");
    require(design->get_io_pin_list()->find_pin("a")->get_term()->get_direction() == idb::IdbConnectDirection::kInput, "retained input direction");
    require(design->get_io_pin_list()->find_pin("a")->get_term()->get_port_number() > 1, "retained input gets extra access PORT");
    require(design->get_net_list()->find_net("bus")->get_io_pins()->get_pin_num() == 1, "reuse retained INOUT rather than creating an alias");
    require(design->get_io_pin_list()->find_pin("bus")->get_term()->get_port_number() > 1, "retained inout gets boundary access");
    std::vector<imj::DCLayerRect> input_shapes = MJI.wrapCutPinShapeList(design->get_io_pin_list()->find_pin("out"));
    std::vector<imj::DCLayerRect> load_shapes = MJI.wrapCutPinShapeList(design->get_instance_list()->find_instance("u1")->get_pin("A"));
    for (imj::DCLayerRect& shape : input_shapes) {
      bool connected = false;
      for (imj::DCLayerRect& load_shape : load_shapes) {
        connected = connected || (load_shape.get_layer_name() == shape.get_layer_name() && load_shape.get_rect().contains(shape.get_rect()));
      }
      require(connected, "unrouted input must use actual retained pin metal");
    }
  }

  void testNetlist()
  {
    dmInst->reset();
    require(dmInst->readLef(std::vector<std::string>{(_data / "tech.lef").string()}, true), "read tech netlist");
    require(dmInst->readLef(std::vector<std::string>{(_data / "cell.lef").string()}), "read cell netlist");
    require(dmInst->readVerilog((_data / "design.v").string(), "driver_test"), "read netlist");
    MJI.initMJ({{"-temp_directory_path", (_output / "netlist").string()}});
    MJI.analyzeDriver({});
    std::string report = readText(_output / "netlist/driver_analyzer/driver_report.rpt");
    require(report.find("MultiDriverNets : 1") != std::string::npos, "netlist multi driver");
    MJI.destroyMJ();
    require(dmInst->addVerilog((_data / "design.v").string(), "driver_test"), "read reference netlist");
    MJI.initMJ({{"-temp_directory_path", (_output / "reference").string()}});
    MJI.analyzeDriver({{"-source", std::string("netlist")}});
    report = readText(_output / "reference/driver_analyzer/driver_report.rpt");
    require(report.find("Source : netlist") != std::string::npos && report.find("MultiDriverNets : 1") != std::string::npos, "reference netlist selection");
    MJI.destroyMJ();
  }
};

int main()
{
  try {
    DesignToolsTest().run();
  } catch (const std::exception& error) {
    MJLOG.warn(imj::Loc::current(), "Design tools regression failed: ", error.what());
    return 1;
  }
  return 0;
}
