// SPDX-License-Identifier: MulanPSL-2.0
#include <unistd.h>

#include <filesystem>

#include "SdcTestDesign.hpp"
#include "SdcWriter.hpp"

void testUnits()
{
  eval("set_units -time ns -resistance kOhm -capacitance pF -voltage V -current mA");
  eval("set_units -time 1000ps -capacitance 1000fF -distance um -power W");
  eval("create_clock -name clk -period 10 [get_ports clk]");
  eval("set_units -time ps");
  eval("create_clock -name after_units -period 5");
  require(STADM.getDatabase().get_timing_constraint().get_clock_map()["clk"].get_period() == 10.0, "set_units changed an existing clock");
  require(STADM.getDatabase().get_timing_constraint().get_clock_map()["after_units"].get_period() == 5.0, "set_units rescaled a new clock");
  for (std::string script : {"set_units -time ns_bad", "set_units -time 0ns", "set_units -time -1ns", "set_units -time 2ns", "set_units -capacitance nS",
                             "set_units -current", "set_units -typo ns"}) {
    require(SdcCommand::getInst().evalString(script) == TCL_ERROR, "invalid units accepted: " + script);
  }
}

void testQueries()
{
  eval(R"TCL(
    proc check {condition} {
      if {![uplevel 1 [list expr $condition]]} {error "assertion failed: $condition"}
    }
    check {[get_fanin -to {} -flat -startpoints_only -only_cells] eq {}}
    check {[get_fanout -from {} -flat -endpoints_only -only_cells] eq {}}
    check {[get_fanin -to [get_pins cdc/reconv/Y] -flat -startpoints_only] eq {cdc/source_reg:CK}}
    check {[get_fanin -to [get_nets {cdc/async_data[0]}] -flat -startpoints_only -only_cells] eq {cdc/source_reg}}
    check {[get_fanin -to [get_pins cdc/source_reg/D] -flat -startpoints_only] eq {in}}
    check {[get_fanout -from [get_pins cdc/source_reg/Q] -flat -endpoints_only] eq {cdc/sink_reg:D out outside_reg:D}}
    check {[get_fanout -from [get_nets {cdc/async_data[0]}] -flat -endpoints_only -only_cells] eq {cdc/sink_reg outside_reg}}
    check {[get_fanout -from [get_pins cdc/sink_reg/D] -flat -endpoints_only -only_cells] eq {cdc/sink_reg}}
    check {[get_fanin -to [get_pins loop/Y] -flat -startpoints_only] eq {}}
    check {[get_fanout -from [get_pins loop/Y] -flat -endpoints_only] eq {}}
    check {[get_fanout -from [get_pins loop/Y] -flat -only_cells] eq {loop}}
    check {[llength [get_fanin -to [get_pins cdc/reconv/Y] -flat]] == 8}
    check {[get_fanout -from [get_pins outside_reg/Q] -flat -endpoints_only] eq {}}
    check {[get_name [get_cells cdc/source_reg]] eq {cdc/source_reg}}
    check {[get_name [get_pins cdc/source_reg/Q]] eq {Q}}
    check {[get_name [get_cells [list {cell with space}]]] eq {cell with space}}
    foreach cell [get_cells [list {cell with space}]] {
      check {[get_name $cell] eq {cell with space}}
    }
    check {[get_name [get_nets {cdc/async_data[0]}]] eq {cdc/async_data[0]}}
  )TCL");
  for (std::string script : {"get_fanin", "get_fanout", "get_fanin -from in", "get_fanin -to no_such_pin", "get_fanout -from cdc/source_reg",
                             "get_fanout -from in -trace_arcs invalid", "get_name {}", "get_name {cdc/source_reg outside_reg}", "get_name missing"}) {
    require(SdcCommand::getInst().evalString(script) == TCL_ERROR, "invalid query accepted: " + script);
  }
  require(STADM.getDatabase().get_arc_list().empty(), "SDC queries changed the analysis graph");
  require(STADM.getDatabase().get_pin_map()["cdc/source_reg:Q"].get_direction() == PinDirection::kInout, "SDC queries changed pin direction");
}

void testCdcException()
{
  eval(R"TCL(
    set CDC cdc
    set START [get_fanin -to [get_nets $CDC/*async_data*] -flat -startpoints_only -only_cells]
    set START [concat $START [get_cells $CDC/*source_reg]]
    set END [list]
    set CANDIDATES [get_fanout -from [get_nets $CDC/*async*data*] -flat -endpoints_only -only_cells]
    foreach cell $CANDIDATES {
      if {[string match "${CDC}/*" [get_name $cell]]} {lappend END $cell}
    }
    lappend END {*}[get_fanout -from [get_pins $CDC/source_reg/Q] -flat -endpoints_only -only_cells]
    set_max_delay 3.0 -from $START -to $END -ignore_clock_latency
  )TCL");
  std::vector<TimingException>& exceptions = STADM.getDatabase().get_timing_constraint().get_path_exception_list();
  require(exceptions.size() == 1, "CDC exception missing");
  require(exceptions[0].get_from_objects() == std::set<std::string>{"cdc/source_reg"}, "CDC launch cells incorrect");
  require(exceptions[0].get_to_objects() == std::set<std::string>{"cdc/sink_reg", "outside_reg"}, "CDC capture cells incorrect");
  require(exceptions[0].get_delay() == 3.0 && exceptions[0].get_ignore_clock_latency(), "CDC delay qualifiers lost");
}

void testLoadObjectIdentity()
{
  Database& database = STADM.getDatabase();
  addPin("", "package", PinDirection::kInout);
  addNet("package", {"package"});
  addNet("inner/package", {});
  eval("set port [get_ports package]; set net [get_nets -filter {full_name == package}]; set_load 0.7 $port");
  TimingConstraint& constraint = database.get_timing_constraint();
  require(constraint.get_net_load_map().empty(), "port load also applied to same-name nets");
  require(constraint.get_port_constraint_map().at("package").get_load(AnalysisType::kMax, TransType::kRise) == 0.7, "port load missing");
  eval("foreach item $net {set_load 0.2 $item}");
  require(constraint.get_net_load_map().size() == 1 && constraint.get_net_load_map().contains("package"), "explicit net collection lost its type");
  require(constraint.get_port_constraint_map().at("package").get_load(AnalysisType::kMax, TransType::kRise) == 0.7, "net load changed a same-name port");
  eval("set_load 0.3 [get_nets inner/package]");
  require(constraint.get_net_load_map().at("inner/package").get(AnalysisType::kMax, TransType::kRise) == 0.3, "hierarchical net load missing");
  eval("set_load 0.8 [lindex $port 0]");
  require(constraint.get_port_constraint_map().at("package").get_load(AnalysisType::kMax, TransType::kRise) == 0.8, "scalar collection element lost its type");
  require(constraint.get_net_load_map().at("package").get(AnalysisType::kMax, TransType::kRise) == 0.2, "port element overwrote net load");
  eval("set_load 0.4 [concat $port $net]");
  require(constraint.get_port_constraint_map().at("package").get_load(AnalysisType::kMax, TransType::kRise) == 0.4
              && constraint.get_net_load_map().at("package").get(AnalysisType::kMax, TransType::kRise) == 0.4,
          "mixed port/net collection was not preserved");
}

// A Tcl input script must become a self-contained SDC constraint snapshot.
void testOutputSdc()
{
  auto& database = STADM.getDatabase();
  database.get_timing_constraint() = TimingConstraint{};
  eval(R"TCL(
    set p [expr {5 * 2}]
    create_clock -name main -period $p [get_ports clk]
    set inputs [remove_from_collection [all_inputs] [get_ports clk]]
    foreach port $inputs { set_input_delay -clock main 2 [get_ports $port] }
    set_clock_latency -source 3 [get_clocks main]
    remove_clock_latency -source [get_clocks main]
  )TCL");
  const auto path = std::filesystem::temp_directory_path() / ("ista-output-sdc-" + std::to_string(getpid()) + ".sdc");
  sdc::writeSdcFile(database, path.string());
  std::ifstream input(path);
  std::string exported((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  require(!exported.empty(), "output_sdc produced an empty snapshot");
  require(exported.find("remove_from_collection") == std::string::npos && exported.find("foreach") == std::string::npos,
          "output_sdc retained Tcl script operations");
  database.get_timing_constraint() = TimingConstraint{};
  eval(exported);
  auto& constraints = database.get_timing_constraint();
  require(constraints.get_clock_map().at("main").get_period() == 10, "clock period did not survive SDC export");
  require(constraints.get_clock_map().at("main").get_source_latency(AnalysisType::kMax, TransType::kRise) == 0, "export resurrected removed clock latency");
  require(constraints.get_port_constraint_map().at("in").get_input_delay_max() == 2, "input delay did not survive SDC export");
  require(!constraints.get_port_constraint_map().contains("clk"), "export lost the result of collection subtraction");
  std::filesystem::remove(path);
}

void testGeneratedDefaultSdc()
{
  Database& database = STADM.getDatabase();
  for (bool virtual_clock : {false, true}) {
    database.get_timing_constraint() = TimingConstraint{};
    eval(R"TCL(
      set sdc_version 2.1
      set_units -time ns -resistance kOhm -capacitance pF -voltage V -current mA
      set clk_freq_mhz 100
      set clk_period [expr {1000.0 / $clk_freq_mhz}]
    )TCL");
    if (virtual_clock) {
      eval(R"TCL(
        set clk_name __VIRTUAL_CLK__
        create_clock -name $clk_name -period $clk_period
        set data_inputs [all_inputs]
      )TCL");
    } else {
      eval(R"TCL(
        set clk_name clk
        set clk_port_name clk
        set clk_port [get_ports $clk_port_name]
        create_clock -name $clk_name -period $clk_period $clk_port
        set_propagated_clock [all_clocks]
        set data_inputs [remove_from_collection [all_inputs] $clk_port]
      )TCL");
    }
    eval(R"TCL(
      set_clock_latency -source 0.5 [get_clocks $clk_name]
      set_input_delay -max 3.0 -clock [get_clocks $clk_name] $data_inputs
      set_input_delay -min 1.0 -clock [get_clocks $clk_name] $data_inputs
      set_output_delay -max 1.0 -clock [get_clocks $clk_name] [all_outputs]
      set_load 0.2 [all_outputs]
      set_clock_uncertainty -setup 0.20 [all_clocks]
      set_clock_uncertainty -hold 0.25 [get_clocks $clk_name]
      set_clock_transition 0.15 [get_clocks $clk_name]
      set_input_transition 0.20 $data_inputs
      set_max_fanout 32 [current_design]
    )TCL");

    std::ostringstream before;
    sdc::writeSdc(database, before);
    eval(R"TCL(
      set_max_transition [expr {$clk_period * 0.17}] -clock_path [get_clocks $clk_name]
      set_max_transition [expr {$clk_period * 0.34}] -data_path [get_clocks $clk_name]
      set_max_capacitance 0.15 [current_design]
      set_timing_derate -early 0.95 -clock -delay_corner [all_delay_corners]
      set_timing_derate -late 1.10 -clock -delay_corner [all_delay_corners]
      set_timing_derate -late 1.10 -data -delay_corner [all_delay_corners]
      set corners [all_delay_corners]
      if {[llength $corners] != 0} {error "syntax-only query invented delay corners"}
      set_timing_derate -late 1.10 -data -delay_corner $corners
      set_max_transition -rise 0.5 [current_design]
      set_max_transition -fall 0.5 [get_ports in]
    )TCL");
    std::ostringstream after;
    sdc::writeSdc(database, after);
    require(before.str() == after.str(), "syntax-only commands changed effective constraints");
    require(database.get_arc_list().empty(), "syntax-only commands built an analysis graph");
    TimingConstraint& constraints = database.get_timing_constraint();
    require(constraints.get_clock_map().size() == 1, "default SDC clock count changed");
    TimingClock& clock = constraints.get_clock_map().begin()->second;
    require(clock.get_period() == 10.0 && clock.get_source_latency(AnalysisType::kMax, TransType::kRise) == 0.5, "default SDC clock values changed");
    require(
        constraints.get_port_constraint_map()["in"].get_input_delay_min() == 1.0 && constraints.get_port_constraint_map()["in"].get_input_delay_max() == 3.0,
        "default SDC min/max input delay changed");
    require(constraints.get_port_constraint_map()["out"].get_load(AnalysisType::kMax, TransType::kRise) == 0.2, "default SDC output load changed");
  }
  for (std::string script :
       {"set_max_transition", "set_max_transition 0.5", "set_max_transition bad [current_design]", "set_max_transition Inf [current_design]",
        "set_max_capacitance", "set_max_capacitance 0.15", "set_max_capacitance bad [current_design]", "set_timing_derate -early",
        "set_timing_derate -late bad", "set_timing_derate -late 1.1 -delay_corner", "set_timing_derate -unknown 1.1", "all_delay_corners unexpected",
        "unsupported_sdc_command"}) {
    require(SdcCommand::getInst().evalString(script) == TCL_ERROR, "invalid syntax accepted: " + script);
  }
}

int main()
{
  Logger::initInst();
  DataManager::initInst();
  SdcCommand::initInst();
  sdc::registerSdcCommands(SdcCommand::getInst());
  int status = 0;
  try {
    buildDesign();
    testUnits();
    testQueries();
    testCdcException();
    testLoadObjectIdentity();
    testOutputSdc();
    testGeneratedDefaultSdc();
    STALOG.info(Loc::current(), "SDC compatibility tests passed");
  } catch (const std::exception& error) {
    STALOG.warn(Loc::current(), error.what());
    status = 1;
  }
  SdcCommand::destroyInst();
  DataManager::destroyInst();
  Logger::destroyInst();
  return status;
}
