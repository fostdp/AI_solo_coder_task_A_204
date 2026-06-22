#!/usr/bin/env python3
"""Stone Mill DEM - Engineering Test Suite
验证工程化改造：Docker、Nginx gzip、Prometheus、Grafana、ClickHouse TTL、
Mosquitto、传感器模拟器多谷物、spdlog/metrics、README。
"""
import os
import sys
import re
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
PASS = 0
FAIL = 0

def check(cond, msg):
    global PASS, FAIL
    if cond:
        PASS += 1
        print(f"  [PASS] {msg}")
    else:
        FAIL += 1
        print(f"  [FAIL] {msg}")

def read(p):
    return Path(p).read_text(encoding="utf-8")

# ====================================
print("\n=== 1. C++ Logger & Metrics ===")
check(Path("backend/include/logger.h").exists(), "logger.h exists")
check(Path("backend/src/logger.cpp").exists(), "logger.cpp exists")
check("USE_SPDLOG" in read("backend/src/logger.cpp"), "logger fallback to iostream")
check("LOG_INFO" in read("backend/include/logger.h"), "LOG_INFO macro defined")

check(Path("backend/include/metrics.h").exists(), "metrics.h exists")
check(Path("backend/src/metrics.cpp").exists(), "metrics.cpp exists")
check("/metrics" in read("backend/src/metrics.cpp"), "metrics exposes /metrics")
check("stonemill_dem_simulations_total" in read("backend/src/metrics.cpp"), "DEM simulation counter declared")
check("stonemill_optimization_seconds" in read("backend/src/metrics.cpp"), "optimization histogram declared")

check("spdlog" in read("CMakeLists.txt"), "CMake finds spdlog")
check("logger.cpp" in read("CMakeLists.txt"), "CMake includes logger.cpp")
check("metrics.cpp" in read("CMakeLists.txt"), "CMake includes metrics.cpp")

for mod in ("mqtt_receiver", "dem_simulator", "size_optimizer", "alarm_mqtt"):
    cpp = f"backend/src/modules/{mod}.cpp"
    check("LOG_" in read(cpp), f"{cpp} uses logger")
check("LOG_" in read("backend/src/main.cpp"), "backend/src/main.cpp uses logger")

# Modules should not have raw cout/cerr (except fallback logger)
for mod in ("mqtt_receiver", "dem_simulator", "size_optimizer", "alarm_mqtt"):
    cpp = f"backend/src/modules/{mod}.cpp"
    s = read(cpp)
    # std::cout / std::cerr forbidden in modules
    ok = "std::cout" not in s and "std::cerr" not in s
    check(ok, f"{mod}.cpp has no raw cout/cerr (uses logger)")

check("metrics.start_http_server" in read("backend/src/main.cpp"), "main starts metrics HTTP server")
check("METRIC_COUNTER_INC" in read("backend/src/main.cpp"), "main uses metrics macros")

# ====================================
print("\n=== 2. Sensor Simulator Multi-Grain + Moisture ===")
sim_text = read("simulator/sensor_simulator.py")
check("GRAIN_PROFILES" in sim_text, "GRAIN_PROFILES dict defined")
for g in ("wheat", "rice", "corn", "soy", "millet", "mixed"):
    check(f'"{g}"' in sim_text, f"grain type '{g}' supported")
check("compute_moisture" in sim_text, "moisture computation method")
check("diurnal" in sim_text.lower(), "diurnal moisture curve")
check("--list-grains" in sim_text, "--list-grains CLI option")
check("--grain-type" in sim_text, "--grain-type CLI option")
check("--mixed" in sim_text, "--mixed CLI option")
check("MQTT_HOST" in sim_text, "MQTT_HOST env var support")
check("timestamp_ns" in sim_text, "nanosecond timestamp emitted")
check("grain_type" in sim_text, "grain_type field in payload")

# requirements.txt
check(Path("simulator/requirements.txt").exists(), "simulator requirements.txt exists")
check("paho-mqtt" in read("simulator/requirements.txt"), "requirements includes paho-mqtt")

# ====================================
print("\n=== 3. ClickHouse TTL & Downsampling ===")
ch = read("clickhouse/init.sql")
check("TTL" in ch, "TTL defined")
check("sensor_data_1m" in ch, "1-minute downsampling table")
check("sensor_data_5m" in ch, "5-minute downsampling table")
check("Materialized View" in ch or "MATERIALIZED VIEW" in ch, "Materialized Views")
check("INTERVAL 7 DAY" in ch, "raw data 7 day TTL")
check("INTERVAL 30 DAY" in ch, "1m data 30 day TTL")
check("INTERVAL 180 DAY" in ch, "5m data 180 day TTL")
check("INTERVAL 2 YEAR" in ch, "daily stats 2 year TTL")
check("toStartOfMinute" in ch, "1-minute grouping")
check("toStartOfFiveMinute" in ch, "5-minute grouping")
check("SummingMergeTree" in ch, "SummingMergeTree engine")
check("LowCardinality(String)" in ch, "LowCardinality grain_type")

# ====================================
print("\n=== 4. Dockerfiles ===")
check(Path("Dockerfile.backend").exists(), "Dockerfile.backend exists")
check(Path("Dockerfile.frontend").exists(), "Dockerfile.frontend exists")
check(Path("Dockerfile.simulator").exists(), "Dockerfile.simulator exists")
check("FROM debian:bookworm AS builder" in read("Dockerfile.backend"), "backend multi-stage: builder")
check("FROM debian:bookworm-slim AS runtime" in read("Dockerfile.backend"), "backend multi-stage: runtime slim")
check("HEALTHCHECK" in read("Dockerfile.backend"), "backend healthcheck")

check("gzip_static" in read("Dockerfile.frontend"), "frontend precompresses with gzip")
check("nginx:1.27-alpine" in read("Dockerfile.frontend"), "frontend base nginx alpine")

check("python:3.12-slim-bookworm" in read("Dockerfile.simulator"), "simulator python base")
check("ENTRYPOINT" in read("Dockerfile.simulator"), "simulator entrypoint")

# ====================================
print("\n=== 5. Nginx gzip_static + Cache ===")
ng = read("deploy/nginx/nginx.conf")
check(re.search(r"^\s*gzip\s+on\s*;", ng, re.M) is not None, "gzip enabled")
check(re.search(r"^\s*gzip_static\s+on\s*;", ng, re.M) is not None, "gzip_static enabled")
check("gzip_comp_level" in ng, "gzip compression level set")
check("gzip_types" in ng, "gzip types defined")
check("Cache-Control" in ng, "Cache-Control headers")
check("expires 30d" in ng, "30 day static cache")
check("proxy_pass" in ng and "stone-mill-backend" in ng, "reverse proxy to backend")
check("/metrics" in ng and "9091" in ng, "/metrics proxied to backend :9091")
check("worker_connections" in ng, "worker connections tuned")

# ====================================
print("\n=== 6. docker-compose.yml ===")
dc = read("docker-compose.yml")
check("stone-mill-backend:" in dc, "compose has backend service")
check("stone-mill-frontend:" in dc, "compose has frontend service")
check("sensor-simulator:" in dc, "compose has simulator service")
check("mosquitto:" in dc, "compose has mosquitto service")
check("clickhouse:" in dc, "compose has clickhouse service")
check("prometheus:" in dc, "compose has prometheus service")
check("grafana:" in dc, "compose has grafana service")
check("healthcheck:" in dc, "compose uses healthchecks")
check("depends_on:" in dc, "compose service dependencies")
check("Dockerfile.backend" in dc, "compose references backend dockerfile")
check("Dockerfile.frontend" in dc, "compose references frontend dockerfile")
check("Dockerfile.simulator" in dc, "compose references simulator dockerfile")
check("TZ: Asia/Shanghai" in dc, "TZ set to Asia/Shanghai")

# ====================================
print("\n=== 7. Middleware Configs ===")
check(Path("deploy/mosquitto/config/mosquitto.conf").exists(), "mosquitto.conf exists")
check("listener 1883" in read("deploy/mosquitto/config/mosquitto.conf"), "mosquitto listens 1883")
check("listener 9001" in read("deploy/mosquitto/config/mosquitto.conf"), "mosquitto websocket 9001")
check(Path("deploy/clickhouse/config/config.xml").exists(), "clickhouse custom config exists")
check("Asia/Shanghai" in read("deploy/clickhouse/config/config.xml"), "CH timezone Asia/Shanghai")

# ====================================
print("\n=== 8. Prometheus + Grafana ===")
pm = read("deploy/prometheus/prometheus.yml")
check("scrape_configs" in pm, "prometheus scrape configs")
check("stone-mill-backend:9091" in pm, "prometheus scrapes backend :9091")
check("clickhouse:9363" in pm, "prometheus scrapes clickhouse")
check("evaluation_interval" in pm, "prometheus evaluation_interval set")

check(Path("deploy/grafana/provisioning/datasources/datasources.yml").exists(), "Grafana datasources.yml")
check("Prometheus" in read("deploy/grafana/provisioning/datasources/datasources.yml"), "Grafana has Prometheus DS")
check("ClickHouse" in read("deploy/grafana/provisioning/datasources/datasources.yml"), "Grafana has ClickHouse DS")
check(Path("deploy/grafana/provisioning/dashboards/dashboards.yml").exists(), "Grafana dashboards provisioning")

# ====================================
print("\n=== 9. README ===")
readme = read("README.md")
check("mermaid" in readme, "README has Mermaid architecture diagram")
check("flowchart TB" in readme, "Mermaid flowchart defined")
check("Docker Compose" in readme, "README has deployment section")
check("Prometheus" in readme, "README has Prometheus metrics table")
check("ClickHouse" in readme, "README has ClickHouse section")
check("传感器模拟器" in readme or "Simulator" in readme, "README has simulator usage")
check("谷物" in readme or "grain" in readme.lower(), "README documents grain types")
for p in ("8088", "9091", "1883", "8123", "3000", "9090"):
    check(p in readme, f"README documents port {p}")

# ====================================
print(f"\n{'='*60}")
print(f"RESULT: PASS={PASS}, FAIL={FAIL}")
if FAIL > 0:
    print("THERE ARE FAILURES")
    sys.exit(1)
else:
    print("ALL CHECKS PASSED")
    sys.exit(0)
