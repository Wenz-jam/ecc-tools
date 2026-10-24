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
// WHETHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//
// See the Mulan PSL v2 for more details.
// ***************************************************************************************
#include "ArnoldiDelayCalculator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "ArnoldiWaveformSolver.hpp"
#include "TimingTolerance.hpp"

namespace ista {

std::optional<ArnoldiDelayResult> ArnoldiDelayCalculator::calculate(TimingArc& timing_arc, TransType output_trans_type, double input_slew,
                                                                    const ParasiticArnoldiModel& arnoldi_model) const
{
  if (!arnoldi_model.get_is_valid() || arnoldi_model.get_order() <= 0 || arnoldi_model.get_total_capacitance() <= 0.0) {
    return std::nullopt;
  }
  double total_capacitance = arnoldi_model.get_total_capacitance();
  double delay_at_total_capacitance = tableDelay(timing_arc, output_trans_type, input_slew, total_capacitance);
  double delay_at_half_capacitance = tableDelay(timing_arc, output_trans_type, input_slew, 0.5 * total_capacitance);
  double delay_resistance = 0.0;
  if (total_capacitance > ista::kTimingTolerance) {
    delay_resistance = (delay_at_total_capacitance - delay_at_half_capacitance) / (0.5 * total_capacitance);
  }
  if (!(delay_resistance > 0.0)) {
    delay_resistance = 1.0;
  }

  double slew_derate = 1.0;
  double lower_threshold = 0.1;
  double upper_threshold = 0.9;
  double voltage_log = 0.0;
  double min_slew_factor = 0.0;
  double x1 = 0.0;
  double y1 = 0.0;
  ArnoldiWaveformSolver::thresholds(timing_arc, output_trans_type, slew_derate, lower_threshold, upper_threshold, voltage_log, min_slew_factor, x1, y1);

  double drive_resistance = tableResistance(timing_arc, output_trans_type, input_slew, total_capacitance, voltage_log, slew_derate, delay_resistance);
  if (!(drive_resistance > 0.0 && drive_resistance < 100.0)) {
    drive_resistance = 1.0;
  }
  double driver_ramp = tableRamp(timing_arc, output_trans_type, input_slew, drive_resistance, total_capacitance, lower_threshold, upper_threshold, voltage_log,
                                 slew_derate, min_slew_factor);
  if (!(driver_ramp > 0.0 && driver_ramp < 100.0)) {
    driver_ramp = 0.5;
  }

  ParasiticArnoldiPoleResidue pole_residue = ArnoldiWaveformSolver::poleResidue(arnoldi_model, drive_resistance);
  if (!pole_residue.get_is_valid()) {
    return std::nullopt;
  }

  double effective_capacitance = total_capacitance;
  double lower_capacitance = 0.;
  double upper_capacitance = std::numeric_limits<double>::infinity();
  bool converged = false;
  // A slew-limited resistance still drives the distributed network. Skipping
  // charge matching in that case loses RC shielding precisely for slow inputs
  // and heavily resistive nets. Fit charge and ramp together before publishing
  // either the cell delay or any receiver waveform.
  for (int32_t iter = 0; iter < 64; ++iter) {
    const double capacitance = ArnoldiWaveformSolver::effectiveCapacitance(driver_ramp, drive_resistance, pole_residue, driver_ramp);
    if (!std::isfinite(capacitance) || capacitance <= 0.0) {
      return std::nullopt;
    }
    const double ramp = tableRamp(timing_arc, output_trans_type, input_slew, drive_resistance, capacitance, lower_threshold, upper_threshold, voltage_log,
                                  slew_derate, min_slew_factor);
    if (!std::isfinite(ramp) || ramp <= 0.0) {
      return std::nullopt;
    }
    converged = std::abs(capacitance - effective_capacitance) <= 1e-7 * std::max(capacitance, effective_capacitance)
                && std::abs(ramp - driver_ramp) <= 1e-7 * std::max(ramp, driver_ramp);
    if (converged) {
      effective_capacitance = capacitance;
      driver_ramp = ramp;
      break;
    }
    // Solve charge(C) - C = 0. Nonlinear tables can make plain fixed-point
    // iteration oscillate around a valid solution. Retain the residual-sign
    // bracket and switch to bisection when iteration stops making progress.
    if (capacitance > effective_capacitance)
      lower_capacitance = effective_capacitance;
    else
      upper_capacitance = effective_capacitance;
    if (std::isfinite(upper_capacitance) && (iter >= 8 || capacitance <= lower_capacitance || capacitance >= upper_capacitance)) {
      effective_capacitance = lower_capacitance + .5 * (upper_capacitance - lower_capacitance);
      driver_ramp = tableRamp(timing_arc, output_trans_type, input_slew, drive_resistance, effective_capacitance, lower_threshold, upper_threshold, voltage_log,
                              slew_derate, min_slew_factor);
      if (!std::isfinite(driver_ramp) || driver_ramp <= 0.0)
        return std::nullopt;
    } else {
      effective_capacitance = capacitance;
      driver_ramp = ramp;
    }
  }
  if (!converged) {
    return std::nullopt;
  }

  double table_delay = tableDelay(timing_arc, output_trans_type, input_slew, effective_capacitance);
  double reference_time = ArnoldiWaveformSolver::rampTime(1.0 / (drive_resistance * effective_capacitance), driver_ramp, 0.5);
  std::vector<double> delay_list;
  std::vector<double> slew_list;
  for (std::size_t term_idx = 0; term_idx < arnoldi_model.get_term_node_list().size(); term_idx++) {
    double upper_time = 0.0;
    double middle_time = 0.0;
    double lower_time = 0.0;
    ArnoldiWaveformSolver::crossingTime(driver_ramp, pole_residue, term_idx, upper_threshold, upper_time, 0.5, middle_time, lower_threshold, lower_time);
    delay_list.push_back(middle_time + table_delay - reference_time);
    slew_list.push_back((lower_time - upper_time) / slew_derate);
  }
  if (delay_list.empty() || slew_list.empty()) {
    return std::nullopt;
  }

  return ArnoldiDelayResult{effective_capacitance, drive_resistance, std::move(delay_list), std::move(slew_list), driver_ramp, std::move(pole_residue)};
}

double ArnoldiDelayCalculator::tableResistance(TimingArc& timing_arc, TransType output_trans_type, double input_slew, double total_capacitance,
                                               double voltage_log, double slew_derate, double delay_resistance)
{
  if (!(total_capacitance > 0.0 && voltage_log > 0.0)) {
    return 0.0;
  }
  double table_slew = tableSlew(timing_arc, output_trans_type, input_slew, total_capacitance);
  double transition_time = slew_derate * table_slew;
  double drive_resistance = transition_time / (voltage_log * total_capacitance);
  if (delay_resistance > 0.0 && drive_resistance > delay_resistance) {
    drive_resistance = delay_resistance;
  }
  return drive_resistance;
}

double ArnoldiDelayCalculator::tableRamp(TimingArc& timing_arc, TransType output_trans_type, double input_slew, double drive_resistance, double capacitance,
                                         double lower_threshold, double upper_threshold, double voltage_log, double slew_derate, double min_slew_factor)
{
  if (!(drive_resistance > 0.0 && capacitance > 0.0 && voltage_log > 0.0)) {
    return 0.0;
  }
  double table_slew = tableSlew(timing_arc, output_trans_type, input_slew, capacitance);
  double transition_time = slew_derate * table_slew;
  double min_slew = drive_resistance * capacitance * min_slew_factor;
  if (voltage_log * drive_resistance * capacitance >= transition_time) {
    return min_slew;
  }
  double ramp = min_slew + 0.3 * transition_time;
  ArnoldiWaveformSolver::solveRamp(1.0 / (drive_resistance * capacitance), transition_time, lower_threshold, upper_threshold, ramp);
  return ramp;
}

double ArnoldiDelayCalculator::tableDelay(TimingArc& timing_arc, TransType output_trans_type, double input_slew, double output_load)
{
  if (timing_arc.get_delay_table_map().count(output_trans_type) == 0) {
    return 0.0;
  }
  double converted_output_load = output_load * timing_arc.get_cap_unit_scale();
  double delay = timing_arc.get_delay_table_map()[output_trans_type].findValue(input_slew * timing_arc.get_time_unit_scale(), converted_output_load);
  return delay / timing_arc.get_time_unit_scale();
}

double ArnoldiDelayCalculator::tableSlew(TimingArc& timing_arc, TransType output_trans_type, double input_slew, double output_load)
{
  if (timing_arc.get_slew_table_map().count(output_trans_type) == 0) {
    return input_slew;
  }
  double converted_output_load = output_load * timing_arc.get_cap_unit_scale();
  double slew = timing_arc.get_slew_table_map()[output_trans_type].findValue(input_slew * timing_arc.get_time_unit_scale(), converted_output_load);
  return slew / timing_arc.get_time_unit_scale();
}

}  // namespace ista
