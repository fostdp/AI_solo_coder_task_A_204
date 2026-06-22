# 古代石碾粉碎过程离散元仿真与粒度优化系统
Stone Mill DEM Simulation & Grain Size Optimization System

农业史团队汉代石碾复原研究全栈工程。C++ DEM 离散元软球模型 + 破碎准则(湿度修正) + 遗传算法粒度优化 + Three.js 3D 石碾粒子可视化 + 多谷物传感器模拟器。

---

## 1. 系统架构

```mermaid
flowchart TB
    subgraph 采集层
        SIM[传感器模拟器<br/>多谷物/含水率曲线<br/>sensor_simulator.py]
    end
    subgraph 消息总线
        MQTT[MQTT Broker<br/>Eclipse Mosquitto :1883]
    end
    subgraph C++ 服务 (模块化 + Boost.Lockfree 队列)
        MQTT_RX[mqtt_receiver<br/>采集 & 校验]
        DEM[dem_simulator<br/>软球模型 / 粗粒化 / 破碎]
        GA[size_optimizer<br/>遗传算法粒度优化]
        ALM[alarm_mqtt<br/>4类告警检测]
        MQTT_RX -- SensorQueue --> ALM
        DEM_REQ[外部 DEMReqQueue] --> DEM
        DEM -- DEMRespQueue --> DEM_RESP[外部 DEMRespQueue]
        OPT_REQ[外部 OptReqQueue] --> GA
        GA -- OptRespQueue --> OPT_RESP[外部 OptRespQueue]
        ALM -- AlertQueue --> ALERT_OUT[外部 AlertQueue]
        LOG[spdlog 日志]
        MET[Prometheus<br/>/metrics :9091]
    end
    subgraph 存储层 (ClickHouse + 4级 TTL)
        RAW[sensor_data<br/>7天原始]
        MV1M[1分钟降采样<br/>30天]
        MV5M[5分钟降采样<br/>180天]
        MVDAY[每日聚合<br/>2年]
        ALERTS[(alerts<br/>6个月)]
        DEM_TBL[(dem_simulation_data<br/>3个月)]
        OPT_TBL[(optimization_results<br/>2年)]
        RAW -->|Materialized View| MV1M
        RAW -->|Materialized View| MV5M
        RAW -->|Materialized View| MVDAY
    end
    subgraph 前端层 (Nginx + gzip_static)
        NG[Nginx :8088<br/>/api, /metrics 反代]
        S3D[stone_roller_3d.js<br/>石碾 3D 渲染]
        PANEL[particle_panel.js<br/>谷物粒子 + 控制面板]
    end
    subgraph 可观测层
        PROM[Prometheus :9090]
        GRAF[Grafana :3000]
    end

    SIM -->|stone_mill/sensor/{id}| MQTT
    MQTT --> MQTT_RX
    MQTT_RX -.->|写入| RAW
    ALM -.->|写入| ALERTS
    DEM -.->|写入| DEM_TBL
    GA -.->|写入| OPT_TBL
    MET -->|scrape| PROM
    PROM --> GRAF
    NG --> S3D & PANEL
    NG -->|/api| stone-mill-backend
    NG -->|/metrics| MET
```

---

## 2. 模块职责

| 模块 | 语言 | 职责 |
|------|------|------|
| mqtt_receiver | C++17 | MQTT 订阅、字段校验、异常拦截、写入 SensorQueue |
| dem_simulator | C++17 | 软球接触 / Hertz-Mindlin、粗粒化加速、Tavar 破碎 + 湿度修正 |
| size_optimizer | C++17 | 遗传算法(锦标赛选择/算术交叉/高斯变异/精英保留) + DEM 适应度评估 |
| alarm_mqtt     | C++17 | wear / low_yield / overspeed / overpressure 4 类告警 + 5 分钟抑制 |
| sensor_simulator | Python 3.12 | 多谷物种类模拟、含水率日周期曲线、粒度分布生成 |
| logger / metrics | C++17 | spdlog 彩色控制台 + 滚动文件；Prometheus /metrics HTTP |
| ClickHouse | SQL | MergeTree 原始表 + SummingMergeTree 三级降采样物化视图 + TTL |
| StoneRoller3D / ParticlePanel | JavaScript | Three.js InstancedMesh 粒子、控制面板 UI、浏览器端简易物理 |

---

## 3. 部署 (Docker Compose)

### 3.1 一键启动

```bash
docker compose up -d --build
```

首次构建会拉取 `debian:bookworm`、`nginx:1.27-alpine`、`clickhouse:24.3-alpine`、`mosquitto:2.0.20`、`prometheus:2.53`、`grafana:11.1` 并构建 3 个自定义镜像。

### 3.2 启动顺序（健康检查）

```
mosquitto (1883/9001)
   └─ clickhouse (8123/9000/9363)
         └─ stone-mill-backend (8080/9091)
               ├─ sensor-simulator
               ├─ stone-mill-frontend (8088)
               └─ prometheus (9090)
                     └─ grafana (3000)
```

### 3.3 对外端口

| 服务 | 端口 | 说明 |
|------|------|------|
| 前端可视化 | **http://localhost:8088** | 石碾 3D + 控制面板（gzip_static） |
| C++ 指标 | http://localhost:9091/metrics | Prometheus 指标 |
| C++ 健康 | http://localhost:9091/health | 健康检查 |
| ClickHouse HTTP | http://localhost:8123 | 原生 HTTP 接口 |
| ClickHouse Native | tcp://localhost:9000 | Native 协议 |
| MQTT | mqtt://localhost:1883 | TCP；ws://localhost:9001 WebSocket |
| Prometheus | http://localhost:9090 | 指标查询与告警规则 |
| Grafana | http://localhost:3000 | 可视化面板 |

Grafana 默认登录：`admin / stone_mill_2024`（可通过 `GF_SECURITY_ADMIN_PASSWORD` 修改）

### 3.4 停止与清理

```bash
docker compose down           # 停止，保留 volume
docker compose down -v        # 停止，删除所有数据卷
docker compose logs -f backend  # 跟踪后端日志
```

---

## 4. 传感器模拟器用法

### 4.1 本机直接运行

```bash
pip install -r simulator/requirements.txt
python simulator/sensor_simulator.py --list-grains          # 查看支持的谷物种类
python simulator/sensor_simulator.py --grain-type wheat      # 指定小麦
python simulator/sensor_simulator.py --mixed                 # 每台石碾随机谷物
python simulator/sensor_simulator.py --moisture 0.18 --no-diurnal  # 固定含水率 18%，关闭日周期
python simulator/sensor_simulator.py --interval 2 --mills 5  # 2 秒间隔、5 台石碾
python simulator/sensor_simulator.py --test-alert            # 发送 1 条测试告警
```

### 4.2 谷物种类

| 英文 | 中文 | 硬度 | 默认含水率 | 说明 |
|------|------|------|-----------|------|
| wheat | 小麦 | 1.0 | 13% | 面粉标准原料 |
| rice  | 水稻 | 0.7 | 15% | 较软，谷壳占比高 |
| corn  | 玉米 | 1.4 | 14% | 坚硬，破碎难度大 |
| soy   | 大豆 | 0.6 | 11% | 含油脂，易出油 |
| millet | 粟(小米) | 1.1 | 12% | 小颗粒，细粉占比高 |
| mixed | 混合谷物 | 1.0 | 13% | 基准对照 |

### 4.3 含水率模型

```
moisture(t) = base_moisture
            + amp × sin(2π × (hour - 6)/24)
            + ε
```
即：每天 12:00（正午）湿度最高、24:00 最低；凌晨 6 点过基准线。各谷物振幅不同（水稻 ±3%、大豆 ±1.5%）。`--no-diurnal` 可关闭日周期。

### 4.4 Docker 运行模式

环境变量控制：

```yaml
environment:
  SIM_GRAIN: wheat       # 谷物种类
  SIM_MILLS: "3"         # 石碾数量
  SIM_INTERVAL: "5"      # 上报间隔（秒）
  MQTT_HOST: mosquitto
  MQTT_PORT: "1883"
  MQTT_TOPIC: stone_mill/sensor
```

```bash
docker compose run --rm sensor-simulator --list-grains
docker compose run --rm sensor-simulator --mixed --moisture 0.20
```

---

## 5. ClickHouse 数据保留与降采样

| 表 | 粒度 | 保留 | 引擎 |
|----|------|------|------|
| `sensor_data` | 原始 (s) | 7 天 | MergeTree |
| `sensor_data_1m` | 1 分钟 | 30 天 | SummingMergeTree |
| `sensor_data_5m` | 5 分钟 | 180 天 | SummingMergeTree |
| `grain_size_stats` | 1 天 | 2 年 | SummingMergeTree |
| `alerts` | 原始 | 6 个月 | MergeTree |
| `dem_simulation_data` | 原始 | 3 个月 | MergeTree |
| `optimization_results` | 原始 | 2 年 | MergeTree |

1m/5m/daily 全部通过 `Materialized View` 自动从 `sensor_data` 增量聚合，无需 ETL。

```sql
-- 示例：查看 24h 产量
SELECT toStartOfHour(timestamp) AS h, sum(yield) AS total_kg
FROM stone_mill.sensor_data
WHERE timestamp > now() - INTERVAL 1 DAY
GROUP BY h ORDER BY h;
```

---

## 6. Prometheus 指标

所有指标前缀 `stonemill_`，由 C++ 服务 `:9091/metrics` 暴露：

| 指标 | 类型 | 说明 |
|------|------|------|
| `stonemill_sensor_messages_total` | Counter | 传感器消息总数，标签：mill_id, valid |
| `stonemill_sensor_invalid_total`  | Counter | 校验失败消息数，标签：mill_id |
| `stonemill_alerts_total`           | Counter | 告警总数，标签：mill_id, type(wear/low_yield/overspeed/overpressure), resolved |
| `stonemill_dem_simulations_total` | Counter | DEM 仿真次数，标签：status(ok/error/timeout) |
| `stonemill_optimizations_total`   | Counter | GA 优化次数，标签：status |
| `stonemill_queue_dropped_total`    | Counter | 队列丢弃数，标签：queue(sensor/dem_resp/opt_resp/alert) |
| `stonemill_sensor_queue_size`      | Gauge   | 队列近似长度，标签：queue |
| `stonemill_active_particles`       | Gauge   | 当前仿真颗粒数，标签：mill_id |
| `stonemill_alerts_active`          | Gauge   | 当前活跃告警数，标签：mill_id |
| `stonemill_dem_simulation_seconds` | Histogram | DEM 仿真耗时（秒） |
| `stonemill_optimization_seconds`   | Histogram | GA 优化耗时（秒） |

Grafana 已预配 Prometheus + ClickHouse 数据源，面板 JSON 放在 `deploy/grafana/dashboards/`。

---

## 7. 本地开发 (无 Docker)

### 7.1 C++ 构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
              -DUSE_BOOST_LOCKFREE=ON \
              -DUSE_SPDLOG=ON \
              -DBUILD_TESTS=ON
cmake --build build -j
build/bin/stone_mill_dem backend/config/app_config.json
# 浏览器访问: http://localhost:9091/metrics
```

依赖缺失自动降级：
- 无 Boost → `std::queue + std::mutex` 线程安全队列
- 无 spdlog → `std::cout/cerr` 带 ANSI 颜色

### 7.2 前端开发

```bash
# 直接用浏览器打开 frontend/index.html，或用任意静态服务器
python -m http.server 8080 --directory frontend
# http://localhost:8080
```

---

## 8. 目录结构

```
.
├── backend/
│   ├── include/               # C++ 头文件
│   │   ├── modules/           # 4 个业务模块
│   │   ├── logger.h           # spdlog/iostream 统一封装
│   │   ├── metrics.h          # Prometheus HTTP 指标
│   │   ├── message_queue.h    # Boost.Lockfree / mutex 双实现队列
│   │   └── ...
│   ├── src/                   # C++ 实现
│   ├── config/app_config.json # 外置 JSON 配置
│   └── tests/test_refactor.py # 回归测试脚本
├── frontend/
│   ├── js/
│   │   ├── stone_roller_3d.js # 石碾 3D 渲染
│   │   ├── particle_panel.js  # 粒子系统 + 控制面板
│   │   └── app.js             # StoneMill3DAdapter 兼容层
│   ├── css/style.css
│   └── index.html
├── simulator/
│   └── sensor_simulator.py    # 多谷物/含水率曲线模拟器
├── clickhouse/init.sql        # 建表 + 3 级降采样 MV + TTL
├── deploy/
│   ├── nginx/nginx.conf       # gzip_static + 缓存策略
│   ├── mosquitto/config/      # MQTT Broker 配置
│   ├── clickhouse/config/     # ClickHouse 自定义配置
│   ├── prometheus/            # Prometheus 采集
│   └── grafana/               # 数据源 & 面板预配
├── Dockerfile.backend
├── Dockerfile.frontend
├── Dockerfile.simulator
├── docker-compose.yml
├── CMakeLists.txt
└── README.md
```

---

## 9. 测试回归

```bash
python backend/tests/test_refactor.py   # 架构/模块/队列/POD/前端/CMake 47+ 项静态检查
node --check frontend/js/*.js           # JS 语法
# 以上均在 CI 通过
```

---

## 10. License

农业史研究项目 · 内部使用
