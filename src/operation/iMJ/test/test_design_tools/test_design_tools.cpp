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
    testPGNetExclusion();
    testPGNetlist();
    testIOBoundaryNetExclusion();
    testDriverInference();
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
    MJI.analyzeDriver();
    std::string report = readText(_output / "driver_analyzer/driver_report.rpt");
    require(report.find("MultiDriverNets : 1") != std::string::npos, "multiple outputs");
    require(report.find("UnresolvedNets : 0") != std::string::npos, "sole inout inferred as driver");
    require(report.find("PORT bus driver (inferred from inout)") != std::string::npos, "inferred driver provenance");
    require(report.find("UndrivenNets : 2") != std::string::npos, "undriven net");
    require(report.find("ExcludedPGNets : 1") != std::string::npos, "PG exclusion");
    require(report.find("INSTANCE u0/Y driver") != std::string::npos, "driver identity");
    idb::IdbNet* net = dmInst->get_idb_design()->get_net_list()->find_net("multi");
    idb::IdbPin* pin = dmInst->get_idb_design()->get_instance_list()->find_instance("u0")->get_pin("Y");
    net->get_instance_pin_list()->get_pin_list().push_back(pin);
    MJI.analyzeDriver();
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
    MJI.analyzeDriver();
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
    MJI.analyzeDriver();
    std::string report = readText(_output / "netlist/driver_analyzer/driver_report.rpt");
    require(report.find("MultiDriverNets : 1") != std::string::npos, "netlist multi driver");
    MJI.destroyMJ();
    require(dmInst->readDef((_data / "design.def.in").string()), "read active DEF");
    require(dmInst->addVerilog((_data / "design.v").string(), "driver_test"), "read reference netlist");
    MJI.initMJ({{"-temp_directory_path", (_output / "active_design").string()}});
    MJI.analyzeDriver();
    report = readText(_output / "active_design/driver_analyzer/driver_report.rpt");
    require(report.find("Design : crop_test\n") != std::string::npos && report.find("Design : driver_test\n") == std::string::npos,
            "driver analysis must use the active design, not the LVS reference");
    require(report.find("MultiDriverNets : 1") != std::string::npos && report.find("UnresolvedNets : 0") != std::string::npos,
            "active DEF driver classification");
    MJI.destroyMJ();
  }

  void testPGNetExclusion()
  {
    loadDEF();
    MJI.initMJ({{"-temp_directory_path", (_output / "pg_exclusion").string()}});
    idb::IdbDesign* design = dmInst->get_idb_design();
    idb::IdbPin* first = addInferencePin("first_supply", idb::IdbConnectDirection::kInput);
    idb::IdbPin* second = addInferencePin("second_supply", idb::IdbConnectDirection::kInput);
    // The explicit PG record is visited after its ordinary same-name record.
    addInferenceNet("VDD", {first, second});
    addInferenceNet("typed_power", {first, second});
    design->get_net_list()->find_net("typed_power")->set_connect_type(idb::IdbConnectType::kPower);
    addInferenceNet("typed_ground", {first, second});
    design->get_net_list()->find_net("typed_ground")->set_connect_type(idb::IdbConnectType::kGround);
    idb::IdbPin* supply_port = addInferencePin("supply_port", idb::IdbConnectDirection::kInOut);
    addInferenceNet("lef_supply", {supply_port},
                    {design->get_instance_list()->find_instance("u0")->get_pin("VPWR"), design->get_instance_list()->find_instance("u1")->get_pin("VPWR")});
    idb::IdbPin* ground_port = addInferencePin("ground_port", idb::IdbConnectDirection::kInOut);
    ground_port->get_term()->set_type(idb::IdbConnectType::kGround);
    addInferenceNet("io_supply", {ground_port, first, second});
    idb::IdbSpecialNet* wildcard = design->get_special_net_list()->add_net("wildcard_supply");
    wildcard->set_connect_type(idb::IdbConnectType::kNone);
    wildcard->add_pin_string("VPWR");
    // Also cover a PG ordinary record followed by a signal special record.
    idb::IdbSpecialNet* duplicate = design->get_special_net_list()->add_net("typed_power");
    duplicate->set_connect_type(idb::IdbConnectType::kSignal);
    duplicate->get_io_pins()->add_pin_ref_unique(first);
    duplicate->get_io_pins()->add_pin_ref_unique(second);
    addInferenceNet("VSS", {addInferencePin("VSS", idb::IdbConnectDirection::kInOut)});
    idb::IdbSpecialNet* signal = design->get_special_net_list()->add_net("special_signal");
    signal->set_connect_type(idb::IdbConnectType::kSignal);
    signal->get_io_pins()->add_pin_ref_unique(addInferencePin("special_signal", idb::IdbConnectDirection::kInOut));

    imj::DAModel model;
    MJI.wrapDriverDesign(model);
    std::set<std::string> net_name_set;
    for (imj::DANet& net : model.get_net_list()) {
      net_name_set.insert(net.get_name());
    }
    require(net_name_set == std::set<std::string>{"multi", "a", "out", "bus", "spare", "transit", "VSS", "special_signal"},
            "PG nets must not reach the graph, while signal special nets and signal VSS remain");
    require(model.get_excluded_pg_net_num() == 6, "count each logical PG net once across ordinary/special records");
    MJI.analyzeDriver();
    std::string report = readText(_output / "pg_exclusion/driver_analyzer/driver_report.rpt");
    require(report.find("SignalNets : 8\n") != std::string::npos && report.find("ExcludedPGNets : 6\n") != std::string::npos, "PG report counts");
    require(report.find("MultiDriverNets : 1\n") != std::string::npos && report.find("UnresolvedNets : 0\n") != std::string::npos
                && report.find("UndrivenNets : 2\n") != std::string::npos && report.find("InferredPins : 3\n") != std::string::npos,
            "PG pins must not contribute inferred roles or driver findings");
    for (std::string name : {"VDD", "typed_power", "typed_ground", "lef_supply", "io_supply", "wildcard_supply"}) {
      require(report.find("Net : " + name + "\n") == std::string::npos, "excluded PG net in report: " + name);
    }
    require(design->get_net_list()->find_net("lef_supply")->get_connect_type() == idb::IdbConnectType::kSignal
                && supply_port->get_term()->get_direction() == idb::IdbConnectDirection::kInOut && wildcard->get_instance_pin_list()->get_pin_list().empty(),
            "PG classification must not mutate IDB types, directions or wildcard connectivity");
    MJI.destroyMJ();
  }

  void testPGNetlist()
  {
    dmInst->reset();
    require(dmInst->readLef(std::vector<std::string>{(_data / "tech.lef").string()}, true), "read PG netlist tech");
    require(dmInst->readLef(std::vector<std::string>{(_data / "cell.lef").string()}), "read PG netlist cell");
    require(dmInst->readVerilog((_data / "power.v").string(), "power_test"), "read PG netlist");
    MJI.initMJ({{"-temp_directory_path", (_output / "pg_netlist").string()}});
    idb::IdbDesign* design = dmInst->get_idb_design();
    require(design->get_net_list()->find_net("rail")->get_connect_type() == idb::IdbConnectType::kSignal
                && design->get_instance_list()->find_instance("u0")->get_pin("VPWR")->get_term()->get_type() == idb::IdbConnectType::kPower,
            "ordinary Verilog supply wire derives PG evidence from LEF terminals");
    imj::DAModel model;
    MJI.wrapDriverDesign(model);
    std::set<std::string> net_name_set;
    for (imj::DANet& net : model.get_net_list()) {
      net_name_set.insert(net.get_name());
    }
    require(net_name_set == std::set<std::string>{"a", "VDD", "VSS", "y0", "y1", "y2"}, "exclude supply0/supply1/LEF PG, not signal names");
    require(model.get_excluded_pg_net_num() == 3, "Verilog PG net count");
    MJI.analyzeDriver();
    std::string report = readText(_output / "pg_netlist/driver_analyzer/driver_report.rpt");
    require(report.find("MultiDriverNets : 2\n") != std::string::npos && report.find("UnresolvedNets : 0\n") != std::string::npos
                && report.find("UndrivenNets : 0\n") != std::string::npos && report.find("InferredPins : 1\n") != std::string::npos,
            "only signal connectivity participates in Verilog driver inference");
    require(report.find("PORT VSS driver (inferred from inout)") != std::string::npos && report.find("Net : rail\n") == std::string::npos,
            "signal VSS is inferred but LEF supply rail is not");
    MJI.destroyMJ();
  }

  void testIOBoundaryNetExclusion()
  {
    dmInst->reset();
    require(dmInst->readLef(std::vector<std::string>{(_data / "tech.lef").string()}, true), "read IO boundary tech");
    require(dmInst->readLef(std::vector<std::string>{(_data / "cell.lef").string(), (_data / "pad.lef").string()}), "read IO boundary cells");
    require(dmInst->readVerilog((_data / "io_boundary.v").string(), "io_boundary_test"), "read IO boundary netlist");
    MJI.initMJ({{"-temp_directory_path", (_output / "io_boundary").string()}});
    idb::IdbDesign* design = dmInst->get_idb_design();
    idb::IdbNet* clock_net = design->get_net_list()->find_net("clock_pin");
    std::vector<idb::IdbPin*> clock_pin_list = clock_net->get_instance_pin_list()->get_pin_list();
    require(design->get_instance_list()->find_instance("pad0")->get_cell_master()->is_pad(), "PAD classification from LEF");
    MJI.analyzeDriver();
    std::string report = readText(_output / "io_boundary/driver_analyzer/driver_report.rpt");
    require(report.find("SignalNets : 5\n") != std::string::npos && report.find("ExcludedPGNets : 1\n") != std::string::npos
                && report.find("ExcludedIOBoundaryNets : 3\n") != std::string::npos,
            "exclude input/output/inout package boundaries independently of PG nets");
    require(report.find("MultiDriverNets : 2\n") != std::string::npos && report.find("InferredPins : 1\n") != std::string::npos
                && report.find("UnresolvedNets : 0\n") != std::string::npos,
            "only direct core IO is inferred; core-side multiple drivers remain");
    for (std::string name : {"clock_pin", "data_pin", "gpio_pin"}) {
      require(report.find("Net : " + name + "\n") == std::string::npos, "package boundary in graph/report: " + name);
    }
    require(report.find("Net : driven\nStatus : MULTI_DRIVER\n") != std::string::npos
                && report.find("Net : from_gpio\nStatus : MULTI_DRIVER\n") != std::string::npos
                && report.find("PORT direct_pin driver (inferred from inout)") != std::string::npos,
            "keep core-to-pad, pad-to-core and direct core IO connections");
    require(clock_net->get_instance_pin_list()->get_pin_list() == clock_pin_list
                && design->get_instance_list()->find_instance("pad2")->get_pin("BOND")->get_term()->get_direction() == idb::IdbConnectDirection::kInOut,
            "boundary exclusion must not alter IDB connectivity or pin directions");
    MJI.analyzeDriver();
    require(readText(_output / "io_boundary/driver_analyzer/driver_report.rpt") == report, "stable boundary exclusion");

    // Classify the merged, deduplicated logical connection, not each DEF record separately.
    idb::IdbSpecialNet* split = design->get_special_net_list()->add_net("clock_pin");
    split->set_connect_type(idb::IdbConnectType::kSignal);
    split->get_instance_pin_list()->get_pin_list() = {clock_pin_list[0], clock_pin_list[0]};
    clock_net->get_instance_pin_list()->get_pin_list().clear();
    clock_net->get_io_pins()->get_pin_list().push_back(clock_net->get_io_pins()->get_pin_list()[0]);
    MJI.analyzeDriver();
    require(readText(_output / "io_boundary/driver_analyzer/driver_report.rpt") == report, "split/duplicate boundary records");

    idb::IdbSpecialNet* mixed = design->get_special_net_list()->add_net("gpio_pin");
    mixed->set_connect_type(idb::IdbConnectType::kSignal);
    mixed->get_instance_pin_list()->add_pin_ref_unique(design->get_instance_list()->find_instance("u0")->get_pin("Y"));
    design->get_net_list()
        ->find_net("data_pin")
        ->get_instance_pin_list()
        ->add_pin_ref_unique(design->get_instance_list()->find_instance("pad1")->get_pin("DRIVE"));
    clock_net->get_io_pins()->add_pin_ref_unique(addInferencePin("second_clock_port", idb::IdbConnectDirection::kInput));
    MJI.analyzeDriver();
    report = readText(_output / "io_boundary/driver_analyzer/driver_report.rpt");
    require(report.find("SignalNets : 8\n") != std::string::npos && report.find("ExcludedIOBoundaryNets : 0\n") != std::string::npos,
            "additional core pin, second PAD terminal or second IO prevents boundary exclusion");
    require(report.find("Net : gpio_pin\nStatus : SINGLE_DRIVER\n") != std::string::npos
                && report.find("Net : data_pin\nStatus : SINGLE_DRIVER\n") != std::string::npos
                && report.find("Net : clock_pin\nStatus : MULTI_DRIVER\n") != std::string::npos,
            "analyze retained mixed connections including known multiple drivers");
    MJI.destroyMJ();
  }

  void testDriverInference()
  {
    loadDEF();
    MJI.initMJ({{"-temp_directory_path", (_output / "inference").string()}});
    idb::IdbDesign* design = dmInst->get_idb_design();
    idb::IdbPin* driver = addInferencePin("known_driver", idb::IdbConnectDirection::kInput);
    idb::IdbPin* other_driver = addInferencePin("other_driver", idb::IdbConnectDirection::kInput);
    idb::IdbPin* load = addInferencePin("known_load", idb::IdbConnectDirection::kOutput);
    idb::IdbPin* inout_load = addInferencePin("inout_load", idb::IdbConnectDirection::kInOut);
    idb::IdbPin* unknown_load = addInferencePin("unknown_load", idb::IdbConnectDirection::kNone);
    addInferenceNet("infer_load", {driver, inout_load, unknown_load, inout_load});
    addInferenceNet("infer_driver", {addInferencePin("inout_driver", idb::IdbConnectDirection::kInOut), load});
    addInferenceNet("infer_unknown", {addInferencePin("unknown_driver", idb::IdbConnectDirection::kNone), load});
    addInferenceNet("ambiguous",
                    {addInferencePin("ambiguous_a", idb::IdbConnectDirection::kInOut), addInferencePin("ambiguous_b", idb::IdbConnectDirection::kInOut), load});
    addInferenceNet("known_multi", {driver, other_driver, addInferencePin("multi_inout", idb::IdbConnectDirection::kInOut)});

    // Shared pin nodes exercise propagation back to nets already visited in the work queue.
    idb::IdbPin* chain_a = addInferencePin("chain_a", idb::IdbConnectDirection::kInOut);
    idb::IdbPin* chain_b = addInferencePin("chain_b", idb::IdbConnectDirection::kInOut);
    idb::IdbPin* chain_c = addInferencePin("chain_c", idb::IdbConnectDirection::kInOut);
    addInferenceNet("chain_first", {chain_a, chain_b});
    addInferenceNet("chain_second", {chain_b, chain_c});
    addInferenceNet("chain_seed", {driver, chain_c});
    idb::IdbPin* cycle_a = addInferencePin("cycle_a", idb::IdbConnectDirection::kInOut);
    idb::IdbPin* cycle_b = addInferencePin("cycle_b", idb::IdbConnectDirection::kInOut);
    idb::IdbPin* cycle_c = addInferencePin("cycle_c", idb::IdbConnectDirection::kInOut);
    idb::IdbPin* cycle_d = addInferencePin("cycle_d", idb::IdbConnectDirection::kInOut);
    addInferenceNet("cycle_first", {cycle_a, cycle_b});
    addInferenceNet("cycle_second", {cycle_b, cycle_c});
    addInferenceNet("cycle_third", {cycle_c, cycle_d});
    addInferenceNet("cycle_fourth", {cycle_d, cycle_a});

    idb::IdbPin* instance_driver = design->get_instance_list()->find_instance("partial")->get_pin("A");
    idb::IdbPin* instance_load = design->get_instance_list()->find_instance("unplaced")->get_pin("A");
    for (idb::IdbPin* pin : {instance_driver, instance_load}) {
      pin->set_term()->set_direction(idb::IdbConnectDirection::kInOut);
      pin->get_term()->set_type(idb::IdbConnectType::kSignal);
    }
    addInferenceNet("instance_driver", {load}, {instance_driver});
    addInferenceNet("instance_load", {driver}, {instance_load});

    MJI.analyzeDriver();
    std::string report = readText(_output / "inference/driver_analyzer/driver_report.rpt");
    require(report.find("InferredPins : 10\n") != std::string::npos, "count distinct inferred graph nodes");
    require(report.find("MultiDriverNets : 2\n") != std::string::npos, "known multiple drivers remain visible");
    require(report.find("UnresolvedNets : 5\n") != std::string::npos, "ambiguous nets and cycle remain unresolved");
    require(report.find("UndrivenNets : 2\n") != std::string::npos, "known undriven nets remain visible");
    require(report.find("PORT inout_load load (inferred from inout)") != std::string::npos, "inout becomes load beside known driver");
    require(report.find("PORT unknown_load load (inferred from unknown)") != std::string::npos, "missing direction becomes load");
    require(report.find("PORT inout_driver driver (inferred from inout)") != std::string::npos, "sole inout becomes driver");
    require(report.find("PORT unknown_driver driver (inferred from unknown)") != std::string::npos, "sole unknown becomes driver");
    require(report.find("Net : known_multi\nStatus : MULTI_DRIVER\nDrivers : 2\nUnresolved : 1\n") != std::string::npos,
            "do not infer from an already violated single-driver constraint");
    require(report.find("Net : ambiguous\nStatus : UNRESOLVED\nDrivers : 0\nUnresolved : 2\n") != std::string::npos, "do not select an arbitrary driver");
    require(report.find("PORT chain_a load (inferred from inout)") != std::string::npos
                && report.find("PORT chain_b driver (inferred from inout)") != std::string::npos
                && report.find("PORT chain_c load (inferred from inout)") != std::string::npos,
            "iterate graph constraints to a fixed point");
    require(report.find("INSTANCE partial/A driver (inferred from inout)") != std::string::npos
                && report.find("INSTANCE unplaced/A load (inferred from inout)") != std::string::npos,
            "keep separate instance pins with the same port name");
    require(inout_load->get_term()->get_direction() == idb::IdbConnectDirection::kInOut
                && unknown_load->get_term()->get_direction() == idb::IdbConnectDirection::kNone
                && instance_driver->get_term()->get_direction() == idb::IdbConnectDirection::kInOut
                && instance_load->get_term()->get_direction() == idb::IdbConnectDirection::kInOut,
            "inference must not change IDB directions");
    MJI.analyzeDriver();
    require(readText(_output / "inference/driver_analyzer/driver_report.rpt") == report, "repeated analysis must be stable");

    std::reverse(design->get_net_list()->get_net_list().begin(), design->get_net_list()->get_net_list().end());
    for (idb::IdbNet* net : design->get_net_list()->get_net_list()) {
      std::reverse(net->get_io_pins()->get_pin_list().begin(), net->get_io_pins()->get_pin_list().end());
    }
    MJI.analyzeDriver();
    std::string reordered_report = readText(_output / "inference/driver_analyzer/driver_report.rpt");
    require(getReportNetMap(reordered_report) == getReportNetMap(report), "inference must not depend on net/pin traversal order");
    MJI.destroyMJ();
  }

  idb::IdbPin* addInferencePin(std::string name, idb::IdbConnectDirection direction)
  {
    idb::IdbPin* pin = dmInst->get_idb_design()->get_io_pin_list()->add_pin_list(name);
    pin->set_as_io();
    pin->set_term()->set_direction(direction);
    pin->get_term()->set_type(idb::IdbConnectType::kSignal);
    return pin;
  }

  void addInferenceNet(std::string name, const std::vector<idb::IdbPin*>& io_pin_list, const std::vector<idb::IdbPin*>& instance_pin_list = {})
  {
    idb::IdbNet* net = dmInst->get_idb_design()->get_net_list()->add_net(name, idb::IdbConnectType::kSignal);
    net->get_io_pins()->get_pin_list() = io_pin_list;
    net->get_instance_pin_list()->get_pin_list() = instance_pin_list;
  }

  std::map<std::string, std::multiset<std::string>> getReportNetMap(const std::string& report)
  {
    std::istringstream stream(report);
    std::map<std::string, std::multiset<std::string>> net_map;
    std::string net_name;
    for (std::string line; std::getline(stream, line);) {
      if (line.starts_with("Net : ")) {
        net_name = line.substr(6);
      }
      if (!line.empty()) {
        net_map[net_name].insert(line);
      }
    }
    return net_map;
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
