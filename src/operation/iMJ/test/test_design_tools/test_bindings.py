import json
import tempfile
from pathlib import Path

import ecc_py


data = Path(__file__).parent / "data"
with tempfile.TemporaryDirectory(prefix="imj_bindings_") as temp:
    output = Path(temp) / "output"
    assert ecc_py.tech_lef_init(str(data / "tech.lef"))
    assert ecc_py.lef_init([str(data / "cell.lef")])
    assert ecc_py.def_init(str(data / "design.def.in"))
    assert ecc_py.init_mj(config_dict={"-temp_directory_path": str(output)})
    assert ecc_py.analyze_driver()
    report = (output / "driver_analyzer/driver_report.rpt").read_text()
    assert "MultiDriverNets : 1" in report
    assert "UnresolvedNets : 1" in report

    config = Path(temp) / "cut.json"
    config.write_text(json.dumps({"MJ": {"-rect": "3 3 12 12"}}))
    assert ecc_py.cut_def(config=str(config))
    first_def = (output / "def_cutter/cut.def").read_text()
    assert "DIEAREA ( 3000 3000 ) ( 12000 12000 )" in first_def
    assert ecc_py.cut_def(config=str(config), config_dict={"-rect": "2.5 2.5 12 12"})
    die_def = (output / "def_cutter/cut.def").read_text()
    assert "DIEAREA ( 2500 2500 ) ( 12000 12000 )" in die_def
    assert "PINS 7 ;" in die_def
    assert "- out + NET out + DIRECTION INPUT" in die_def
    assert "- multi + NET multi + DIRECTION OUTPUT" in die_def
    assert "- transit + NET transit + DIRECTION INOUT" in die_def
    assert not ecc_py.cut_def(config=str(Path(temp) / "missing.json"))
    assert ecc_py.destroy_mj()
    assert ecc_py.def_init(str(output / "def_cutter/cut.def"))
