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
#include "DriverAnalyzer.hpp"

#include "DataManager.hpp"
#include "MJInterface.hpp"
#include "Utility.hpp"

namespace imj {

void DriverAnalyzer::initInst()
{
  if (_da_instance == nullptr) {
    _da_instance = new DriverAnalyzer();
  }
}

DriverAnalyzer& DriverAnalyzer::getInst()
{
  if (_da_instance == nullptr) {
    MJLOG.error(Loc::current(), "The instance not initialized!");
  }
  return *_da_instance;
}

void DriverAnalyzer::destroyInst()
{
  delete _da_instance;
  _da_instance = nullptr;
}

void DriverAnalyzer::analyze(std::map<std::string, std::any>& config_map)
{
  Monitor monitor;
  MJLOG.info(Loc::current(), "Starting...");
  DAModel da_model = initDAModel(config_map);
  analyzeNetList(da_model);
  writeReport(da_model);
  MJLOG.info(Loc::current(), "Completed", monitor.getStatsInfo());
}

DAModel DriverAnalyzer::initDAModel(std::map<std::string, std::any>& config_map)
{
  DAModel da_model;
  da_model.set_source(MJUTIL.getConfigValue<std::string>(config_map, "-source", "design"));
  if (da_model.get_source() != "design" && da_model.get_source() != "netlist") {
    MJLOG.error(Loc::current(), "analyze_driver -source must be design or netlist.");
  }
  MJI.wrapDriverDesign(da_model);
  return da_model;
}

void DriverAnalyzer::analyzeNetList(DAModel& da_model)
{
  for (DANet& net : da_model.get_net_list()) {
    std::set<std::tuple<bool, std::string, std::string>> pin_set;
    for (DAPin& pin : net.get_pin_list()) {
      if (!pin_set.emplace(pin.get_is_io(), pin.get_instance_name(), pin.get_pin_name()).second) {
        continue;
      }
      if (pin.get_type() == DAPinType::kDriver) {
        net.set_driver_num(net.get_driver_num() + 1);
      } else if (pin.get_type() != DAPinType::kLoad) {
        net.set_unresolved_num(net.get_unresolved_num() + 1);
      }
    }
    if (net.get_driver_num() > 1) {
      da_model.set_multi_driver_net_num(da_model.get_multi_driver_net_num() + 1);
    } else if (net.get_unresolved_num() > 0) {
      da_model.set_unresolved_net_num(da_model.get_unresolved_net_num() + 1);
    } else if (net.get_driver_num() == 0) {
      da_model.set_undriven_net_num(da_model.get_undriven_net_num() + 1);
    }
  }
}

#if 1  // exhibit

void DriverAnalyzer::writeReport(DAModel& da_model)
{
  std::string path = MJDM.getConfig().da_temp_directory_path + "driver_report.rpt";
  std::ofstream* stream = MJUTIL.getOutputFileStream(path);
  *stream << "Design : " << da_model.get_design_name() << "\nSource : " << da_model.get_source() << "\nSignalNets : " << da_model.get_net_list().size()
          << "\nExcludedPGNets : " << da_model.get_excluded_pg_net_num() << "\nMultiDriverNets : " << da_model.get_multi_driver_net_num()
          << "\nUnresolvedNets : " << da_model.get_unresolved_net_num() << "\nUndrivenNets : " << da_model.get_undriven_net_num()
          << "\n\nINOUT and unknown directions are unresolved, not definite drivers.\n"
          << "Multiple output pins indicate structural drivers; enable exclusivity is not evaluated.\n";
  for (DANet& net : da_model.get_net_list()) {
    if (net.get_driver_num() == 1 && net.get_unresolved_num() == 0) {
      continue;
    }
    std::string status = net.get_driver_num() > 1 ? "MULTI_DRIVER" : net.get_unresolved_num() > 0 ? "UNRESOLVED" : "UNDRIVEN";
    *stream << "\nNet : " << net.get_name() << "\nStatus : " << status << "\nDrivers : " << net.get_driver_num()
            << "\nUnresolved : " << net.get_unresolved_num() << "\n";
    std::set<std::tuple<bool, std::string, std::string>> pin_set;
    for (DAPin& pin : net.get_pin_list()) {
      if (pin.get_type() == DAPinType::kLoad || !pin_set.emplace(pin.get_is_io(), pin.get_instance_name(), pin.get_pin_name()).second) {
        continue;
      }
      *stream << "  " << (pin.get_is_io() ? "PORT " : "INSTANCE ")
              << (pin.get_is_io() ? pin.get_pin_name() : pin.get_instance_name() + "/" + pin.get_pin_name()) << " " << GetDAPinTypeName()(pin.get_type())
              << "\n";
    }
  }
  MJUTIL.closeFileStream(stream);
  if (da_model.get_multi_driver_net_num() > 0 || da_model.get_unresolved_net_num() > 0) {
    MJLOG.warn(Loc::current(), "Multi-driver nets=", da_model.get_multi_driver_net_num(), ", unresolved nets=", da_model.get_unresolved_net_num(), ". See ",
               path);
  } else {
    MJLOG.info(Loc::current(), "No multiple drivers found. Undriven nets=", da_model.get_undriven_net_num(), ". See ", path);
  }
}

#endif

DriverAnalyzer* DriverAnalyzer::_da_instance = nullptr;

}  // namespace imj
