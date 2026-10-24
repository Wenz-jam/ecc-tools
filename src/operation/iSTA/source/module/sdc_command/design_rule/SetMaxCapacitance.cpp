// SPDX-License-Identifier: MulanPSL-2.0
#include "design_rule/DesignRuleCommands.hpp"

namespace ista::sdc {

TclSetMaxCapacitance::TclSetMaxCapacitance(const char* cmd_name, ClientData client_data) : SdcTclCmd(cmd_name, client_data)
{
  addOption(new ecc::TclDoubleOption("capacitance", 1));
  addOption(new ecc::TclStringOption("objects", 1));
}

unsigned TclSetMaxCapacitance::exec()
{
  if (!getOptionOrArg("capacitance")->is_set_val() || !getOptionOrArg("objects")->is_set_val()) {
    setTclError("set_max_capacitance requires a capacitance and an object collection");
    return 0;
  }
  // Syntax compatibility only. Constraint storage and capacitance checks remain to be implemented.
  warn("set_max_capacitance is accepted for SDC syntax compatibility only; the constraint is not applied");
  return 1;
}

}  // namespace ista::sdc
