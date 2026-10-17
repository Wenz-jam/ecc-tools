// SPDX-License-Identifier: MulanPSL-2.0
#include "DataManager.hpp"
#include "IdbCellMaster.h"
#include "IdbInstance.h"
#include "IdbNet.h"
#include "Logger.hpp"
#include "PowerPropagator.hpp"
#include "PWInterface.hpp"

using namespace ipw;

void require(bool condition, const std::string& message)
{
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testNet(idb::CellMasterType cell_type, idb::IdbConnectDirection port_direction, idb::IdbConnectDirection cell_direction,
             int32_t port_num, bool has_core_load, bool excluded)
{
  idb::IdbCellMaster io_master;
  io_master.set_name("boundary_cell");
  io_master.set_type(cell_type);
  // Deliberately do not use the name PAD: classification must come from LEF.
  io_master.add_term("package_contact")->set_direction(cell_direction);
  idb::IdbInstance io_instance;
  io_instance.set_name("io_cell");
  io_instance.set_cell_master(&io_master);
  idb::IdbPin* io_cell_pin = io_instance.get_pin_list()->get_pin_list().front();

  idb::IdbCellMaster core_master;
  core_master.set_name("core_cell");
  core_master.set_type(idb::CellMasterType::kCore);
  core_master.add_term("A")->set_direction(idb::IdbConnectDirection::kInput);
  idb::IdbInstance core_instance;
  core_instance.set_name("core");
  core_instance.set_cell_master(&core_master);
  idb::IdbPin* core_pin = core_instance.get_pin_list()->get_pin_list().front();

  idb::IdbPin port;
  port.set_pin_name("external");
  port.set_as_io();
  port.set_term()->set_direction(port_direction);
  idb::IdbPin other_port;
  other_port.set_pin_name("other_external");
  other_port.set_as_io();
  other_port.set_term()->set_direction(port_direction);

  idb::IdbNet net;
  net.set_net_name("connection");
  net.set_connect_type(idb::IdbConnectType::kSignal);
  net.add_instance_pin(io_cell_pin);
  io_cell_pin->set_net(&net);
  if (port_num > 0) {
    net.add_io_pin(&port);
    port.set_net(&net);
  }
  if (port_num > 1) {
    net.add_io_pin(&other_port);
    other_port.set_net(&net);
  }
  if (has_core_load) {
    net.add_instance_pin(core_pin);
    core_pin->set_net(&net);
  }

  DataManager::initInst();
  require(PWI.isIOBoundaryNet(&net) == excluded, "incorrect IO boundary classification");
  PWI.wrapInstance(&io_instance);
  PWI.wrapInstancePinList(&io_instance);
  if (has_core_load) {
    PWI.wrapInstance(&core_instance);
    PWI.wrapInstancePinList(&core_instance);
  }
  for (idb::IdbPin* pin : net.get_io_pins()->get_pin_list()) {
    PWI.wrapPortPin(pin);
  }
  PWI.wrapNet(&net);

  Database& database = PWDM.getDatabase();
  require(database.get_instance_map().count("io_cell") == 1, "IO cell instance was removed");
  require(database.get_pin_map().count("io_cell:package_contact") == (excluded ? 0U : 1U), "incorrect IO cell pin membership");
  require(database.get_net_map().count("connection") == (excluded ? 0U : 1U), "incorrect net membership");
  for (idb::IdbPin* pin : net.get_io_pins()->get_pin_list()) {
    require(database.get_pin_map().count(pin->get_pin_name()) == (excluded ? 0U : 1U), "incorrect top port membership");
  }
  if (has_core_load) {
    require(database.get_pin_map().count("core:A") == 1, "core-facing load was removed");
  }
  for (auto& [name, wrapped_net] : database.get_net_map()) {
    for (const std::string& pin_name : wrapped_net.get_pin_name_list()) {
      require(database.get_pin_map().count(pin_name) == 1, "wrapped net references a filtered pin");
    }
  }
  PWI.wrapPowerIndex();
  uint64_t index = 0;
  for (auto& [name, pin] : database.get_pin_map()) {
    require(pin.get_index() == index++, "filtered pins left holes in the power index");
  }
  database.set_design_name("boundary_test");
  std::map<std::string, vcd::VcdSignalActivity> signal_activity_map;
  signal_activity_map["boundary_test/connection"].set_transition_density(0.1);
  signal_activity_map["boundary_test/connection"].set_static_probability(0.5);
  require(PowerPropagator::annotateVcdActivity(signal_activity_map) == (excluded ? 0U : 1U), "incorrect VCD net match");
  require(database.getVcdActivityNum() == database.get_pin_map().size(), "incorrect VCD pin annotation count");
  require(database.get_net_map().count("connection") == (excluded ? 0U : 1U), "VCD annotation recreated a filtered net");
  require(net.get_io_pins()->get_pin_num() == static_cast<uint32_t>(port_num), "IDB ports were modified");
  require(net.get_instance_pin_list()->get_pin_num() == (has_core_load ? 2U : 1U), "IDB instance pins were modified");
  require(io_cell_pin->get_term()->get_direction() == cell_direction, "IDB direction was modified");
  DataManager::destroyInst();
}

void testUnconnectedPort()
{
  DataManager::initInst();
  idb::IdbPin port;
  port.set_pin_name("unconnected");
  port.set_as_io();
  port.set_term()->set_direction(idb::IdbConnectDirection::kInput);
  require(!PWI.isIOBoundaryNet(nullptr), "unconnected pin treated as a PAD boundary");
  PWI.wrapPortPin(&port);
  require(PWDM.getDatabase().get_pin_map().count("unconnected") == 1, "unconnected port was removed");
  DataManager::destroyInst();
}

int main()
{
  Logger::initInst();
  for (idb::IdbConnectDirection port_direction :
       {idb::IdbConnectDirection::kInput, idb::IdbConnectDirection::kOutput, idb::IdbConnectDirection::kInOut}) {
    for (idb::IdbConnectDirection cell_direction :
         {idb::IdbConnectDirection::kInput, idb::IdbConnectDirection::kOutput, idb::IdbConnectDirection::kInOut}) {
      testNet(idb::CellMasterType::kPad, port_direction, cell_direction, 1, false, true);
      testNet(idb::CellMasterType::kCore, port_direction, cell_direction, 1, false, false);
    }
  }
  testNet(idb::CellMasterType::kPadInput, idb::IdbConnectDirection::kInOut, idb::IdbConnectDirection::kInOut, 1, false, true);
  testNet(idb::CellMasterType::kPad, idb::IdbConnectDirection::kInput, idb::IdbConnectDirection::kOutput, 0, true, false);
  testNet(idb::CellMasterType::kPad, idb::IdbConnectDirection::kInput, idb::IdbConnectDirection::kInOut, 1, true, false);
  testNet(idb::CellMasterType::kPad, idb::IdbConnectDirection::kInput, idb::IdbConnectDirection::kInOut, 2, false, false);
  testUnconnectedPort();
  PWInterface::destroyInst();
  Logger::destroyInst();
  return 0;
}
