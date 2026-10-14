# Driver Analysis and DEF Cutting

Load LEF and a design using the existing IDB commands, then initialize iMJ:

```tcl
tech_lef_init -path tech.lef
lef_init -path cells.lef
def_init -path design.def
# Or: verilog_init -path design.v -top top
init_mj -temp_directory_path ./mj_result
analyze_driver
# Only for a physically placed DEF; coordinates are microns.
cut_def -rect {10 10 40 40}
destroy_mj
# Continue working on the standalone cropped design.
def_init -path ./mj_result/def_cutter/cut.def
def_save -path ./cropped.def
netlist_save -path ./cropped.v
```

- `analyze_driver [-source design|netlist]` writes
  `driver_analyzer/driver_report.rpt`. The default is the active design, whether
  loaded from DEF or ordinary Verilog. `netlist` selects the separate reference
  design loaded by `lvs_verilog_init`.
- Top-level inputs and instance outputs are structural drivers. INOUT and
  unspecified directions are reported as unresolved, not silently classified.
  POWER/GROUND nets and pins are excluded. Multiple tristate outputs remain
  multiple structural drivers; this command does not prove enable exclusivity.
- `cut_def -rect {llx lly urx ury}` writes
  `def_cutter/cut.def` and `def_cutter/cut_report.rpt`. It does not replace or
  mutate the loaded design. Load the output with `def_init` to work on it.
- The supplied rectangle is always the new DIEAREA and must lie inside the
  input die. There is no boundary option or core mode. Complete row sites are
  retained on the original site grid, rounded inward at the crop edges.
- Coordinates remain absolute. Only complete placed instances and complete IO
  ports are retained. Rows/tracks are cropped on their existing grids. Ordinary
  and special routing, rectangular blockages, fills, regions and slots are
  clipped. Complete vias are retained. For a partially clipped via, its cut is
  removed while its in-die routing enclosures remain as metal and IO access.
- Cut nets receive IO pins with shapes over retained boundary metal. Where a
  net has no boundary crossing, retained routing or placed terminal metal
  provides the access shape. A cut net with no usable shape is rejected rather
  than assigning a fabricated IO location.
- One logical pin represents all cut locations on a net, using multiple PORTs.
  An existing retained IO is extended instead of adding another logical driver.
  Ordinary and special net records with the same name share the generated IO.
  Outside drivers produce INPUTs; inside drivers produce OUTPUTs. PG nets,
  bidirectional nets and unresolved driver roles use INOUT.
- The result has explicit ports and connectivity for DEF and Verilog export.
  It does not synthesize the deleted logic or reroute the retained metal: the
  new ports describe the interface to the removed environment. Added/extended
  IOs and their shapes are listed in the report.
- The current geometry path supports rectangular DIEAREA and Manhattan
  routing represented by IDB. Unsupported diagonal routing fails explicitly.
  Output uses the IDB DEF writer; geometry is preserved as clipped metal
  rectangles rather than preserving the original textual route segmentation.
- Cropping uses the information materialized by IDB, not a lossless DEF text
  transform. For example, IDB currently does not populate group member callbacks;
  it cannot reconstruct those memberships from an imported DEF.

Python uses the same flags and implementation:

```python
ecc_py.init_mj(config_dict={"-temp_directory_path": "./mj_result"})
ecc_py.analyze_driver()
ecc_py.cut_def(config_dict={"-rect": "10 10 40 40"})
ecc_py.destroy_mj()
```

An optional JSON file uses the existing top-level `MJ` object with string flag
values. Nonempty `config_dict` values override JSON values.

Regression:

```sh
cmake --build build --target test_imj_design_tools ecc_bin ecc_py
ctest --test-dir build -R '^test_imj_design_tools$' --output-on-failure
PYTHONPATH=bin python3 src/operation/iMJ/test/test_design_tools/test_bindings.py
```
