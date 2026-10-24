// SPDX-License-Identifier: MulanPSL-2.0
#include "ArnoldiResponseCalculator.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>

#include "ArnoldiDelayCalculator.hpp"
#include "ArnoldiWaveformSolver.hpp"
#include "ThresholdNormalizer.hpp"

namespace ista {
DriverResponse& ArnoldiResponseCalculator::response(std::string& output_pin, TimingArc& timing_arc, AnalysisType analysis_type, TransType output_trans_type,
                                                    double input_slew)
{
  TimingResultKey timing_result_key = timingKey(output_pin, timing_arc, analysis_type, output_trans_type, input_slew);
  if (_responses.count(timing_result_key) == 0) {
    _responses[timing_result_key] = calculate(output_pin, timing_arc, analysis_type, output_trans_type, input_slew);
  }
  return _responses[timing_result_key];
}

ArnoldiResponseCalculator::TimingResultKey ArnoldiResponseCalculator::timingKey(std::string& output_pin, TimingArc& timing_arc, AnalysisType analysis_type,
                                                                                TransType output_trans_type, double input_slew)
{
  return std::make_tuple(output_pin, reinterpret_cast<std::uintptr_t>(&timing_arc), analysis_type, output_trans_type, input_slew);
}

DriverResponse ArnoldiResponseCalculator::calculate(std::string& output_pin, TimingArc& timing_arc, AnalysisType analysis_type, TransType output_trans_type,
                                                    double input_slew)
{
  DriverResponse timing_result;
  Database& database = _database;
  if (database.network().pins().count(output_pin) == 0) {
    return timing_result;
  }
  Pin& output_pin_data = database.network().pins()[output_pin];
  if (output_pin_data.get_net_name().empty() || database.parasitics().nets().count(output_pin_data.get_net_name()) == 0) {
    return timing_result;
  }

  if (_rc.hasDisabledDriver(output_pin_data.get_net_name(), output_pin))
    return timing_result;

  ParasiticNet& parasitic_net = database.parasitics().nets()[output_pin_data.get_net_name()];
  std::string source_node_name = _rc.nodeName(parasitic_net, output_pin);
  if (source_node_name.empty()) {
    return timing_result;
  }

  ParasiticArnoldiModel& arnoldi_model = _models.arnoldiModel(parasitic_net, source_node_name, analysis_type, output_trans_type);
  auto calculated = ArnoldiDelayCalculator{}.calculate(timing_arc, output_trans_type, input_slew, arnoldi_model);
  if (!calculated) {
    return timing_result;
  }
  const auto& delay_list = calculated->delays;
  const auto& slew_list = calculated->slews;
  double effective_capacitance = calculated->effective_capacitance;
  double drive_resistance = calculated->drive_resistance;

  timing_result.set_is_valid(true);
  timing_result.set_effective_capacitance(effective_capacitance);
  timing_result.set_gate_delay(delay_list.front());
  timing_result.set_driver_slew(slew_list.front());
  bool suppress_min_slew = _config.min_slew_degradation == 0 && analysis_type == AnalysisType::kMin
                           && arnoldi_model.get_pi_resistance() > kMinSlewResistanceThreshold * drive_resistance;
  for (std::size_t term_idx = 0; term_idx < arnoldi_model.get_term_node_list().size(); term_idx++) {
    std::string& term_node_name = arnoldi_model.get_term_node_list()[term_idx];
    double wire_delay = delay_list[term_idx] - delay_list.front();
    double load_slew = slew_list[term_idx];
    std::string pin_name = _rc.pinName(term_node_name);
    if (term_idx > 0) {
      const double ratio = _threshold.voltageRatio(timing_arc, pin_name);
      if (std::abs(ratio - 1.) > 1e-12) {
        auto* receiver = _threshold.getThresholdTimingCell(pin_name);
        const double middle = ratio * normalizedThreshold(_threshold.getTimingCellInputThreshold(*receiver, output_trans_type));
        const double lower = ratio * normalizedThreshold(_threshold.getTimingCellSlewLowerThreshold(*receiver, output_trans_type));
        const double upper = ratio * normalizedThreshold(_threshold.getTimingCellSlewUpperThreshold(*receiver, output_trans_type));
        const double derate = receiver->get_slew_derate_from_library();
        if (!(middle > 0. && middle < 1. && lower > 0. && upper < 1. && upper > lower && derate > 0. && std::isfinite(derate))) {
          // A failed crossing is not a missing cache entry eligible for another RC fallback.
          timing_result.setLoadFailed(term_node_name);
          timing_result.setLoadFailed(pin_name);
          continue;
        }
        const bool fall = output_trans_type == TransType::kFall;
        double source_time = 0., middle_time = 0., early = 0., late = 0.;
        const double source_threshold = normalizedThreshold(fall ? timing_arc.get_output_threshold_pct_fall() : timing_arc.get_output_threshold_pct_rise());
        ArnoldiWaveformSolver::crossingTime(calculated->driver_ramp, calculated->waveform, 0, fall ? source_threshold : 1. - source_threshold, source_time);
        ArnoldiWaveformSolver::crossingTime(calculated->driver_ramp, calculated->waveform, term_idx, fall ? middle : 1. - middle, middle_time);
        ArnoldiWaveformSolver::crossingTime(calculated->driver_ramp, calculated->waveform, term_idx, fall ? upper : 1. - lower, early);
        ArnoldiWaveformSolver::crossingTime(calculated->driver_ramp, calculated->waveform, term_idx, fall ? lower : 1. - upper, late);
        wire_delay = middle_time - source_time;
        load_slew = (late - early) / derate;
        if (!std::isfinite(wire_delay) || !std::isfinite(load_slew) || load_slew < 0.) {
          timing_result.setLoadFailed(term_node_name);
          timing_result.setLoadFailed(pin_name);
          continue;
        }
      } else {
        _threshold.adjust(timing_arc, pin_name, output_trans_type, wire_delay, load_slew);
      }
      if (std::abs(ratio - 1.) <= 1e-12 && suppress_min_slew && _threshold.compatible(timing_arc, pin_name, output_trans_type)) {
        // Keep the delay threshold correction from the physical load waveform.
        // Convert the replacement slew to the receiver library independently.
        load_slew = slew_list.front();
        double unused_wire_delay = 0.0;
        _threshold.adjust(timing_arc, pin_name, output_trans_type, unused_wire_delay, load_slew);
      }
    }
    timing_result.get_wire_delay_map()[term_node_name] = wire_delay;
    timing_result.get_load_slew_map()[term_node_name] = load_slew;
    timing_result.get_wire_delay_map()[pin_name] = wire_delay;
    timing_result.get_load_slew_map()[pin_name] = load_slew;
  }
  cacheDriverResponse(output_pin, analysis_type, output_trans_type, timing_result.get_driver_slew(), timing_result);
  return timing_result;
}

void ArnoldiResponseCalculator::cacheDriverResponse(std::string& output_pin, AnalysisType analysis_type, TransType output_trans_type, double driver_slew,
                                                    DriverResponse& timing_result)
{
  DriverResultKey driver_result_key = driverKey(output_pin, analysis_type, output_trans_type, driver_slew);
  _driver_responses[driver_result_key] = timing_result;
}

ArnoldiResponseCalculator::DriverResultKey ArnoldiResponseCalculator::driverKey(std::string& output_pin, AnalysisType analysis_type,
                                                                                TransType output_trans_type, double driver_slew)
{
  return std::make_tuple(output_pin, analysis_type, output_trans_type, driver_slew);
}

bool ArnoldiResponseCalculator::loadFailed(Arc& arc, AnalysisType analysis, TransType edge, double slew)
{
  if (!std::isfinite(slew))
    return false;
  auto key = driverKey(arc.get_source_pin(), analysis, edge, slew);
  auto found = _driver_responses.find(key);
  return found != _driver_responses.end() && found->second.loadFailed(arc.get_sink_pin());
}

std::optional<double> ArnoldiResponseCalculator::wireDelay(Arc& arc, AnalysisType analysis_type, TransType trans_type, double input_slew)
{
  if (!std::isfinite(input_slew)) {
    return std::nullopt;
  }
  std::string& source_pin = arc.get_source_pin();
  DriverResultKey driver_result_key = driverKey(source_pin, analysis_type, trans_type, input_slew);
  if (_driver_responses.count(driver_result_key) == 0) {
    return std::nullopt;
  }
  DriverResponse& timing_result = _driver_responses[driver_result_key];
  std::string& sink_pin = arc.get_sink_pin();
  if (timing_result.get_wire_delay_map().count(sink_pin) == 0) {
    return std::nullopt;
  }
  return timing_result.get_wire_delay_map()[sink_pin];
}

std::optional<double> ArnoldiResponseCalculator::loadSlew(Arc& arc, AnalysisType analysis_type, TransType trans_type, double input_slew)
{
  if (!std::isfinite(input_slew)) {
    return std::nullopt;
  }
  std::string& source_pin = arc.get_source_pin();
  DriverResultKey driver_result_key = driverKey(source_pin, analysis_type, trans_type, input_slew);
  if (_driver_responses.count(driver_result_key) == 0) {
    return std::nullopt;
  }
  DriverResponse& timing_result = _driver_responses[driver_result_key];
  std::string& sink_pin = arc.get_sink_pin();
  if (timing_result.get_load_slew_map().count(sink_pin) == 0) {
    return std::nullopt;
  }
  return timing_result.get_load_slew_map()[sink_pin];
}

}  // namespace ista
