# iSTA

`iSTA` follows the operation-tool layout used by hand-written `iRT`.

Current skeleton:

```text
src/operation/iSTA/
  interface/
  source/
    data_manager/
    module/
    toolkit/
      logger/
      monitor/
      utility/
  test/
```

Keep command and external API boundaries in `interface/`. Keep shared iSTA
state in `source/data_manager/`. Add concrete algorithm stages under
`source/module/<stage>/` only when the stage design is known.

## IO-cell boundary connectivity

iSTA and iPW preserve signal nets between top-level ports and IO-cell PADs,
including both endpoints. A routing skip based on LEF `CLASS PAD` is not a
timing or power boundary: these objects carry SDC constraints and activity
annotations, and Liberty can define paths through the IO cell.

Do not delete or rename those objects to bypass INOUT direction checks, move
their constraints to core-facing pins, or silently ignore missing SDC targets.
Preserving the objects does not by itself implement bidirectional propagation;
the current graph's single-direction limitation still needs a separate fix.
See the [bidirectional IO design](../../../docs/sta-pw-bidirectional-io.md) for
the proposed graph, mode, and power-accounting changes.

## Power/ground connectivity

iSTA excludes POWER/GROUND pins and nets in its Interface wrapper. A pin on a
POWER/GROUND regular or special net is excluded even if its own term is marked
SIGNAL or has no USE type, so excluded supply nets do not leave isolated timing
vertices. This changes only iSTA's analysis database, not IDB or iPW input.
Retained nets reference only retained pins, so DataManager cannot recreate
filtered PG pins when building their connectivity.

An ordinary Verilog wire can also represent a supply without carrying a supply
type. iSTA recognizes it when it has instance pins and all of them have the
same LEF POWER or GROUND type, without conflicting PG port types. Mixed nets
with logic pins, and explicitly typed clock/reset/scan/tie-off nets, are not
reclassified. Net names such as VDD/VSS alone are not evidence of supply use.

## Minimum slew during RC propagation

`init_sta -min_slew_degradation 1` (the default) propagates the calculated
receiver waveform slew. The same integer option is accepted by the C++ config
map and the Python dictionary/JSON bindings. Values other than `0` or `1` are
rejected. As with other Python configuration fields, an empty dictionary/JSON
string leaves the option unspecified. Configure it before starting a run;
changing the internal config after calculation requires reinitializing the
delay calculator and its caches.

`-min_slew_degradation 0` enables an experimental early-mode approximation for
cell-driven RC trees: when the moment-reduced pi resistance exceeds `0.45` times
the Arnoldi driver resistance, a receiver with matching slew thresholds and
slew derate uses the driver slew. Receivers with different slew conventions
retain their calculated waveform, including the existing library conversion.
The compatibility check applies to each load and transition; equivalent
fractional and percentage thresholds are accepted across library names.
The resistance decision uses the analysis corner and output transition.
RC wire delay and its threshold correction still use the calculated waveform.
Maximum analysis, input-port waveforms, resistance loops, and the DMP fallback
retain their previous behavior.

This approximation was evaluated against controlled reference minimum-slew
runs. Restricting it to compatible slew conventions avoids applying the
approximation to unvalidated library conversions, which previously worsened
some SRAM receiver delays. This is a conservative scope limit, not a
requirement of RC physics. The fitted Arnoldi resistance is a heuristic,
not an exact physical resistance derived from the delay/load slope.
Neither the ratio nor this scope limit should be treated as the reference tool's
complete algorithm. Some paths improve and others regress, so the option
remains experimental. It does not resolve all driver-model or SDF coverage
differences.
