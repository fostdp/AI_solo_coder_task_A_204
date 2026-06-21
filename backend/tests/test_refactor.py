#!/usr/bin/env python3
"""Stone Mill DEM Modular Refactor Regression Tests.

Checks:
  1. C++ modules exist with expected classes
  2. Message queues exist with TriviallyCopyable types
  3. JSON config validates
  4. Frontend files split correctly
  5. index.html script tags updated
  6. CMakeLists.txt includes all new sources
  7. main.cpp wires all 4 modules
"""

import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
BACKEND = ROOT / "backend"
FRONTEND = ROOT / "frontend"

ok = True

def check(cond, msg):
    global ok
    if cond:
        print(f"  [PASS] {msg}")
    else:
        print(f"  [FAIL] {msg}")
        ok = False

def grep(path, pattern):
    try:
        txt = Path(path).read_text(encoding="utf-8")
        return re.search(pattern, txt) is not None
    except Exception as e:
        return False

def read(path):
    return Path(path).read_text(encoding="utf-8")

print("=" * 60)
print("1. C++ Modular Headers")
print("=" * 60)
for m in ["mqtt_receiver", "dem_simulator", "size_optimizer", "alarm_mqtt"]:
    h = BACKEND / "include" / "modules" / f"{m}.h"
    cpp = BACKEND / "src" / "modules" / f"{m}.cpp"
    check(h.exists(), f"{m}.h exists")
    check(cpp.exists(), f"{m}.cpp exists")
    cls_name = {
        "mqtt_receiver": "MqttReceiver",
        "dem_simulator": "DemSimulator",
        "size_optimizer": "SizeOptimizer",
        "alarm_mqtt": "AlarmMqtt",
    }[m]
    check(grep(h, f"class {cls_name}"), f"{m}.h declares class {cls_name}")
    check(grep(h, "void start\(\)"), f"{m}.h declares start()")
    check(grep(h, "void stop\(\)"), f"{m}.h declares stop()")

print("\n" + "=" * 60)
print("2. Message Queue & POD Messages")
print("=" * 60)
mq = BACKEND / "include" / "message_queue.h"
check(mq.exists(), "message_queue.h exists")
check(grep(mq, "class MessageQueue"), "MessageQueue template exists")
check(grep(mq, "boost::lockfree::queue|std::queue<T>"), "Uses either lockfree or fallback queue")
for t in ["SensorMessage", "DEMRequest", "DEMResponse",
          "OptimizeRequest", "OptimizeResponse", "AlertMessage"]:
    check(grep(mq, f"struct {t}"), f"POD struct {t} defined")

print("\n" + "=" * 60)
print("3. JSON Config")
print("=" * 60)
cfg = BACKEND / "config" / "app_config.json"
check(cfg.exists(), "app_config.json exists")
try:
    data = json.loads(read(cfg))
    for sec in ["process", "dem", "breakage", "optimization", "thresholds"]:
        check(sec in data, f"config has section '{sec}'")
    check("dt" in data["dem"], "dem.dt present")
    check("population_size" in data["optimization"], "optimization.population_size present")
    check("wear_warning" in data["thresholds"], "thresholds.wear_warning present")
except Exception as e:
    check(False, f"JSON parse error: {e}")

print("\n" + "=" * 60)
print("4. Frontend Split")
print("=" * 60)
sr = FRONTEND / "js" / "stone_roller_3d.js"
pp = FRONTEND / "js" / "particle_panel.js"
check(sr.exists(), "stone_roller_3d.js exists")
check(pp.exists(), "particle_panel.js exists")
check(grep(sr, "class StoneRoller3D"), "stone_roller_3d.js declares StoneRoller3D")
check(grep(sr, "createMillModel|createRoller|addLights"), "StoneRoller3D has rendering methods")
check(grep(pp, "class ParticlePanel"), "particle_panel.js declares ParticlePanel")
check(grep(pp, "InstancedMesh|createGrainParticles"), "ParticlePanel handles particles")
check(grep(pp, "breakParticle"), "ParticlePanel has breakParticle")

html = FRONTEND / "index.html"
check(grep(html, "stone_roller_3d\.js"), "index.html includes stone_roller_3d.js")
check(grep(html, "particle_panel\.js"), "index.html includes particle_panel.js")
check(not grep(html, "mill3d\.js"), "index.html no longer includes mill3d.js")

app = FRONTEND / "js" / "app.js"
check(grep(app, "StoneMill3DAdapter"), "app.js uses adapter for backward compat")
check(grep(app, "StoneRoller3D|ParticlePanel"), "app.js references new modules")

css = FRONTEND / "css" / "style.css"
check(grep(css, "\.particle-panel"), "style.css has .particle-panel styles")

print("\n" + "=" * 60)
print("5. CMakeLists.txt")
print("=" * 60)
cm = ROOT / "CMakeLists.txt"
check(cm.exists(), "CMakeLists.txt exists")
for f in ["config_loader.cpp",
          "modules/mqtt_receiver.cpp",
          "modules/dem_simulator.cpp",
          "modules/size_optimizer.cpp",
          "modules/alarm_mqtt.cpp"]:
    check(grep(cm, f.replace("\\", "\\\\")), f"CMakeLists includes {f}")
check(grep(cm, "USE_BOOST_LOCKFREE|FALLBACK_MUTEX_QUEUE"),
      "CMake has Boost/fallback option")

print("\n" + "=" * 60)
print("6. main.cpp Orchestration")
print("=" * 60)
main = BACKEND / "src" / "main.cpp"
check(main.exists(), "main.cpp exists")
check(grep(main, "MqttReceiver"), "main constructs MqttReceiver")
check(grep(main, "DemSimulator"), "main constructs DemSimulator")
check(grep(main, "SizeOptimizer"), "main constructs SizeOptimizer")
check(grep(main, "AlarmMqtt"), "main constructs AlarmMqtt")
check(grep(main, "SensorQueue|DEMReqQueue|OptReqQueue|AlertQueue"),
      "main wires queue types")
check(grep(main, "run_smoke_test"), "main has smoke test")

print("\n" + "=" * 60)
print("7. Config Loader")
print("=" * 60)
cl_h = BACKEND / "include" / "config_loader.h"
cl_cpp = BACKEND / "src" / "config_loader.cpp"
check(cl_h.exists(), "config_loader.h exists")
check(cl_cpp.exists(), "config_loader.cpp exists")
check(grep(cl_h, "class ConfigLoader"), "ConfigLoader class declared")
check(grep(cl_h, "load_from_file|to_json"), "ConfigLoader has load/save methods")

print("\n" + "=" * 60)
print("RESULT:", "ALL CHECKS PASSED" if ok else "SOME CHECKS FAILED")
print("=" * 60)
sys.exit(0 if ok else 1)
