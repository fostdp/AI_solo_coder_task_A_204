-- 古代石碾粉碎过程离散元仿真与粒度优化系统
-- ClickHouse 数据库初始化脚本

CREATE DATABASE IF NOT EXISTS stone_mill;

USE stone_mill;

-- 传感器原始数据表
CREATE TABLE IF NOT EXISTS sensor_data (
    mill_id UInt32,
    timestamp DateTime64(3, 'Asia/Shanghai'),
    roller_speed Float64,
    roller_pressure Float64,
    grain_size_0_1mm Float64,
    grain_size_1_2mm Float64,
    grain_size_2_3mm Float64,
    grain_size_3_4mm Float64,
    grain_size_4_5mm Float64,
    grain_size_gt5mm Float64,
    yield Float64,
    wear_degree Float64,
    roller_gap Float64
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (mill_id, timestamp)
TTL timestamp + INTERVAL 1 YEAR
SETTINGS index_granularity = 8192;

-- 粒度分布统计表
CREATE TABLE IF NOT EXISTS grain_size_stats (
    mill_id UInt32,
    date Date,
    hour UInt8,
    avg_roller_speed Float64,
    avg_roller_pressure Float64,
    avg_grain_size_0_1mm Float64,
    avg_grain_size_1_2mm Float64,
    avg_grain_size_2_3mm Float64,
    avg_grain_size_3_4mm Float64,
    avg_grain_size_4_5mm Float64,
    avg_grain_size_gt5mm Float64,
    avg_yield Float64,
    total_yield Float64,
    sample_count UInt64
) ENGINE = SummingMergeTree()
PARTITION BY date
ORDER BY (mill_id, date, hour)
TTL date + INTERVAL 2 YEAR;

-- 告警数据表
CREATE TABLE IF NOT EXISTS alerts (
    alert_id UUID DEFAULT generateUUIDv4(),
    mill_id UInt32,
    timestamp DateTime64(3, 'Asia/Shanghai'),
    alert_type Enum('wear', 'low_yield', 'abnormal_speed', 'abnormal_pressure'),
    severity Enum('info', 'warning', 'critical'),
    message String,
    current_value Float64,
    threshold Float64,
    resolved UInt8 DEFAULT 0
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (mill_id, timestamp, alert_type)
TTL timestamp + INTERVAL 6 MONTH;

-- 优化结果数据表
CREATE TABLE IF NOT EXISTS optimization_results (
    optimization_id UUID DEFAULT generateUUIDv4(),
    mill_id UInt32,
    timestamp DateTime64(3, 'Asia/Shanghai'),
    target_size_range String,
    best_roller_speed Float64,
    best_roller_gap Float64,
    predicted_yield Float64,
    predicted_target_ratio Float64,
    fitness_value Float64,
    generation_count UInt32,
    parameters String
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (mill_id, timestamp)
TTL timestamp + INTERVAL 1 YEAR;

-- 离散元仿真数据表
CREATE TABLE IF NOT EXISTS dem_simulation_data (
    simulation_id UUID DEFAULT generateUUIDv4(),
    mill_id UInt32,
    timestamp DateTime64(3, 'Asia/Shanghai'),
    particle_count UInt32,
    avg_velocity Float64,
    avg_force Float64,
    max_force Float64,
    broken_particles UInt32,
    breakage_rate Float64,
    simulation_time Float64,
    roller_speed Float64,
    roller_gap Float64
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (mill_id, timestamp)
TTL timestamp + INTERVAL 3 MONTH;

-- 实时状态汇总视图（物化视图）
CREATE MATERIALIZED VIEW IF NOT EXISTS mill_status_mv
TO grain_size_stats
AS
SELECT
    mill_id,
    toDate(timestamp) AS date,
    toHour(timestamp) AS hour,
    avg(roller_speed) AS avg_roller_speed,
    avg(roller_pressure) AS avg_roller_pressure,
    avg(grain_size_0_1mm) AS avg_grain_size_0_1mm,
    avg(grain_size_1_2mm) AS avg_grain_size_1_2mm,
    avg(grain_size_2_3mm) AS avg_grain_size_2_3mm,
    avg(grain_size_3_4mm) AS avg_grain_size_3_4mm,
    avg(grain_size_4_5mm) AS avg_grain_size_4_5mm,
    avg(grain_size_gt5mm) AS avg_grain_size_gt5mm,
    avg(yield) AS avg_yield,
    sum(yield) AS total_yield,
    count() AS sample_count
FROM sensor_data
GROUP BY mill_id, date, hour;

-- 创建索引
ALTER TABLE sensor_data ADD INDEX IF NOT EXISTS idx_timestamp timestamp TYPE minmax GRANULARITY 4;
ALTER TABLE sensor_data ADD INDEX IF NOT EXISTS idx_mill_yield (mill_id, yield) TYPE minmax GRANULARITY 4;
ALTER TABLE alerts ADD INDEX IF NOT EXISTS idx_resolved resolved TYPE set(0,1) GRANULARITY 256;
ALTER TABLE alerts ADD INDEX IF NOT EXISTS idx_severity severity TYPE set(0,1,2) GRANULARITY 256;
