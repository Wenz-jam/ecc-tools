// SPDX-License-Identifier: MulanPSL-2.0
#include "operating_condition/SetTimingDerate.hpp"

namespace ista::sdc {

TclSetTimingDerate::TclSetTimingDerate(const char* cmd_name, ClientData client_data) : SdcTclCmd(cmd_name, client_data)
{
  addOption(new ecc::TclSwitchOption("-early"));
  addOption(new ecc::TclSwitchOption("-late"));
  addOption(new ecc::TclSwitchOption("-clock"));
  addOption(new ecc::TclSwitchOption("-data"));
  addOption(new ecc::TclStringOption("-delay_corner", 0));
  addOption(new ecc::TclDoubleOption("derate", 1));
}

unsigned TclSetTimingDerate::exec()
{
  if (!getOptionOrArg("derate")->is_set_val()) {
    setTclError("set_timing_derate requires a derate factor");
    return 0;
  }
  // Syntax compatibility only. Do not change arc delays or clock/data propagation here.
  warn("set_timing_derate is accepted for SDC syntax compatibility only; OCV derates are not applied");
  return 1;
}

}  // namespace ista::sdc
