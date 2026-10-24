// SPDX-License-Identifier: MulanPSL-2.0
#include "design_rule/DesignRuleCommands.hpp"

namespace ista::sdc {

TclSetMaxTransition::TclSetMaxTransition(const char* cmd_name, ClientData client_data) : SdcTclCmd(cmd_name, client_data)
{
  addOption(new ecc::TclSwitchOption("-clock_path"));
  addOption(new ecc::TclSwitchOption("-data_path"));
  addOption(new ecc::TclSwitchOption("-rise"));
  addOption(new ecc::TclSwitchOption("-fall"));
  addOption(new ecc::TclDoubleOption("transition", 1));
  addOption(new ecc::TclStringOption("objects", 1));
}

unsigned TclSetMaxTransition::exec()
{
  if (!getOptionOrArg("transition")->is_set_val() || !getOptionOrArg("objects")->is_set_val()) {
    setTclError("set_max_transition requires a transition and an object collection");
    return 0;
  }
  // Syntax compatibility only. Constraint storage and transition checks remain to be implemented.
  warn("set_max_transition is accepted for SDC syntax compatibility only; the constraint is not applied");
  return 1;
}

}  // namespace ista::sdc
