// SPDX-License-Identifier: MulanPSL-2.0
#include "object/ObjectCommands.hpp"

namespace ista::sdc {

TclAllDelayCorners::TclAllDelayCorners(const char* cmd_name, ClientData client_data) : SdcTclCmd(cmd_name, client_data)
{
}

unsigned TclAllDelayCorners::exec()
{
  // There is no named delay-corner model yet. An empty collection does not invent analysis corners.
  warn("all_delay_corners is accepted for SDC syntax compatibility only; returning an empty collection");
  setResult(std::vector<std::string>{});
  return 1;
}

}  // namespace ista::sdc
