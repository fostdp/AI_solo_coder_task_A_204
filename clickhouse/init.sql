-- 古代石碾粉碎过程离散元仿真与粒度优化系统
-- ClickHouse 数据库初始化脚本 v2
--   - 原始数据保留7天
--   - 1分钟降采样保留30天
--   - 5分钟降采样保留180天
--   - 每日聚合保留2年
--   - 告警保留6个月
--   - DEM仿真保留3个月
--   - 优化结果保留2年

CREATE DATABASE IF NOT EXISTS stone_mill;
USE stone_mill;

-- ========================================
-- 1. 原始传感器数据表 (保留 7 天)
-- ========================================
CREATE TABLE IF NOT EXISTS sensor_data (
    mill_id UInt32,
    grain_type LowCardinality(String) DEFAULT 'wheat',
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
    roller_gap Float64,
    moisture Float64
) ENGINE = MergeTree()
PARTITION BY toYYYYMMDD(timestamp)
ORDER BY (mill_id, grain_type, timestamp)
TTL timestamp + INTERVAL 7 DAY
SETTINGS index_granularity = 8192, min_bytes_for_wide_part = '10M';

-- ========================================
-- 2. 1分钟降采样 (保留 30 天)
-- ========================================
CREATE TABLE IF NOT EXISTS sensor_data_1m (
    mill_id UInt32,
    grain_type LowCardinality(String),
    timestamp DateTime('Asia/Shanghai'),
    avg_roller_speed Float64,
    min_roller_speed Float64,
    max_roller_speed Float64,
    avg_roller_pressure Float64,
    min_roller_pressure Float64,
    max_roller_pressure Float64,
    avg_grain_size_0_1mm Float64,
    avg_grain_size_1_2mm Float64,
    avg_grain_size_2_3mm Float64,
    avg_grain_size_3_4mm Float64,
    avg_grain_size_4_5mm Float64,
    avg_grain_size_gt5mm Float64,
    avg_yield Float64,
    sum_yield Float64,
    avg_wear_degree Float64,
    avg_roller_gap Float64,
    avg_moisture Float64,
    min_moisture Float64,
    max_moisture Float64,
    sample_count UInt32
) ENGINE = SummingMergeTree((sample_count))
PARTITION BY toYYYYMM(timestamp)
ORDER BY (mill_id, grain_type, timestamp)
TTL timestamp + INTERVAL 30 DAY
SETTINGS index_granularity = 1024;

-- 1分钟物化视图：从 raw -> 1m
CREATE MATERIALIZED VIEW IF NOT EXISTS sensor_data_1m_mv
TO sensor_data_1m
AS
SELECT
    mill_id,
    grain_type,
    toStartOfMinute(timestamp) AS timestamp,
    avg(roller_speed)         AS avg_roller_speed,
    min(roller_speed)         AS min_roller_speed,
    max(roller_speed)         AS max_roller_speed,
    avg(roller_pressure)      AS avg_roller_pressure,
    min(roller_pressure)      AS min_roller_pressure,
    max(roller_pressure)      AS max_roller_pressure,
    avg(grain_size_0_1mm)     AS avg_grain_size_0_1mm,
    avg(grain_size_1_2mm)     AS avg_grain_size_1_2mm,
    avg(grain_size_2_3mm)     AS avg_grain_size_2_3mm,
    avg(grain_size_3_4mm)     AS avg_grain_size_3_4mm,
    avg(grain_size_4_5mm)     AS avg_grain_size_4_5mm,
    avg(grain_size_gt5mm)     AS avg_grain_size_gt5mm,
    avg(yield)                AS avg_yield,
    sum(yield)                AS sum_yield,
    avg(wear_degree)          AS avg_wear_degree,
    avg(roller_gap)           AS avg_roller_gap,
    avg(moisture)             AS avg_moisture,
    min(moisture)             AS min_moisture,
    max(moisture)             AS max_moisture,
    toUInt32(count())         AS sample_count
FROM sensor_data
GROUP BY mill_id, grain_type, timestamp;

-- ========================================
-- 3. 5分钟降采样 (保留 180 天)
-- ========================================
CREATE TABLE IF NOT EXISTS sensor_data_5m (
    mill_id UInt32,
    grain_type LowCardinality(String),
    timestamp DateTime('Asia/Shanghai'),
    avg_roller_speed Float64,
    min_roller_speed Float64,
    max_roller_speed Float64,
    avg_roller_pressure Float64,
    min_roller_pressure Float64,
    max_roller_pressure Float64,
    avg_grain_size_0_1mm Float64,
    avg_grain_size_1_2mm Float64,
    avg_grain_size_2_3mm Float64,
    avg_grain_size_3_4mm Float64,
    avg_grain_size_4_5mm Float64,
    avg_grain_size_gt5mm Float64,
    avg_yield Float64,
    sum_yield Float64,
    avg_wear_degree Float64,
    avg_roller_gap Float64,
    avg_moisture Float64,
    min_moisture Float64,
    max_moisture Float64,
    sample_count UInt32
) ENGINE = SummingMergeTree((sample_count))
PARTITION BY toYYYYMM(timestamp)
ORDER BY (mill_id, grain_type, timestamp)
TTL timestamp + INTERVAL 180 DAY
SETTINGS index_granularity = 1024;

-- 5分钟物化视图：从 raw -> 5m
CREATE MATERIALIZED VIEW IF NOT EXISTS sensor_data_5m_mv
TO sensor_data_5m
AS
SELECT
    mill_id,
    grain_type,
    toStartOfFiveMinute(timestamp) AS timestamp,
    avg(roller_speed)         AS avg_roller_speed,
    min(roller_speed)         AS min_roller_speed,
    max(roller_speed)         AS max_roller_speed,
    avg(roller_pressure)      AS avg_roller_pressure,
    min(roller_pressure)      AS min_roller_pressure,
    max(roller_pressure)      AS max_roller_pressure,
    avg(grain_size_0_1mm)     AS avg_grain_size_0_1mm,
    avg(grain_size_1_2mm)     AS avg_grain_size_1_2mm,
    avg(grain_size_2_3mm)     AS avg_grain_size_2_3mm,
    avg(grain_size_3_4mm)     AS avg_grain_size_3_4mm,
    avg(grain_size_4_5mm)     AS avg_grain_size_4_5mm,
    avg(grain_size_gt5mm)     AS avg_grain_size_gt5mm,
    avg(yield)                AS avg_yield,
    sum(yield)                AS sum_yield,
    avg(wear_degree)          AS avg_wear_degree,
    avg(roller_gap)           AS avg_roller_gap,
    avg(moisture)             AS avg_moisture,
    min(moisture)             AS min_moisture,
    max(moisture)             AS max_moisture,
    toUInt32(count())         AS sample_count
FROM sensor_data
GROUP BY mill_id, grain_type, timestamp;

-- ========================================
-- 4. 每日聚合表 (保留 2 年)
-- ========================================
CREATE TABLE IF NOT EXISTS grain_size_stats (
    mill_id UInt32,
    grain_type LowCardinality(String) DEFAULT 'wheat',
    date Date,
    avg_roller_speed Float64,
    min_roller_speed Float64,
    max_roller_speed Float64,
    avg_roller_pressure Float64,
    avg_grain_size_0_1mm Float64,
    avg_grain_size_1_2mm Float64,
    avg_grain_size_2_3mm Float64,
    avg_grain_size_3_4mm Float64,
    avg_grain_size_4_5mm Float64,
    avg_grain_size_gt5mm Float64,
    avg_yield Float64,
    total_yield Float64,
    avg_wear_degree Float64,
    avg_moisture Float64,
    sample_count UInt64
) ENGINE = SummingMergeTree((sample_count, total_yield))
PARTITION BY toYYYYMM(date)
ORDER BY (mill_id, grain_type, date)
TTL date + INTERVAL 2 YEAR;

CREATE MATERIALIZED VIEW IF NOT EXISTS mill_status_mv
TO grain_size_stats
AS
SELECT
    mill_id,
    grain_type,
    toDate(timestamp) AS date,
    avg(roller_speed)         AS avg_roller_speed,
    min(roller_speed)         AS min_roller_speed,
    max(roller_speed)         AS max_roller_speed,
    avg(roller_pressure)      AS avg_roller_pressure,
    avg(grain_size_0_1mm)     AS avg_grain_size_0_1mm,
    avg(grain_size_1_2mm)     AS avg_grain_size_1_2mm,
    avg(grain_size_2_3mm)     AS avg_grain_size_2_3mm,
    avg(grain_size_3_4mm)     AS avg_grain_size_3_4mm,
    avg(grain_size_4_5mm)     AS avg_grain_size_4_5mm,
    avg(grain_size_gt5mm)     AS avg_grain_size_gt5mm,
    avg(yield)                AS avg_yield,
    sum(yield)                AS total_yield,
    avg(wear_degree)          AS avg_wear_degree,
    avg(moisture)             AS avg_moisture,
    count()                   AS sample_count
FROM sensor_data
GROUP BY mill_id, grain_type, date;

-- ========================================
-- 5. 告警数据表 (保留 6 个月)
-- ========================================
CREATE TABLE IF NOT EXISTS alerts (
    alert_id UUID DEFAULT generateUUIDv4(),
    mill_id UInt32,
    timestamp DateTime64(3, 'Asia/Shanghai'),
    alert_type Enum('wear' = 1, 'low_yield' = 2, 'overspeed' = 3, 'overpressure' = 4),
    severity Enum('info' = 1, 'warning' = 2, 'critical' = 3),
    message String,
    current_value Float64,
    threshold Float64,
    resolved UInt8 DEFAULT 0
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (mill_id, timestamp, alert_type)
TTL timestamp + INTERVAL 6 MONTH;

-- ========================================
-- 6. 优化结果数据表 (保留 2 年)
-- ========================================
CREATE TABLE IF NOT EXISTS optimization_results (
    optimization_id UUID DEFAULT generateUUIDv4(),
    mill_id UInt32,
    timestamp DateTime64(3, 'Asia/Shanghai'),
    target_bin_min UInt8,
    target_bin_max UInt8,
    best_roller_speed Float64,
    best_roller_gap Float64,
    predicted_yield Float64,
    predicted_target_ratio Float64,
    fitness_value Float64,
    generation_count UInt32,
    population_size UInt32,
    parameters String
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (mill_id, timestamp)
TTL timestamp + INTERVAL 2 YEAR;

-- ========================================
-- 7. DEM 仿真数据表 (保留 3 个月)
-- ========================================
CREATE TABLE IF NOT EXISTS dem_simulation_data (
    simulation_id UUID DEFAULT generateUUIDv4(),
    mill_id UInt32,
    timestamp DateTime64(3, 'Asia/Shanghai'),
    particle_count UInt32,
    coarse_scale UInt8 DEFAULT 1,
    avg_velocity Float64,
    avg_force Float64,
    max_force Float64,
    broken_particles UInt32,
    breakage_rate Float64,
    simulation_time Float64,
    wallclock_seconds Float64,
    roller_speed Float64,
    roller_gap Float64,
    moisture Float64 DEFAULT 0.12
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)
ORDER BY (mill_id, timestamp)
TTL timestamp + INTERVAL 3 MONTH;

-- ========================================
-- 8. 索引
-- ========================================
ALTER TABLE sensor_data ADD INDEX IF NOT EXISTS idx_timestamp timestamp TYPE minmax GRANULARITY 4;
ALTER TABLE sensor_data ADD INDEX IF NOT EXISTS idx_mill_yield (mill_id, yield) TYPE minmax GRANULARITY 4;
ALTER TABLE sensor_data ADD INDEX IF NOT EXISTS idx_moisture moisture TYPE minmax GRANULARITY 8;

ALTER TABLE alerts ADD INDEX IF NOT EXISTS idx_resolved resolved TYPE set(0,1) GRANULARITY 256;
ALTER TABLE alerts ADD INDEX IF NOT EXISTS idx_severity severity TYPE set(1,2,3) GRANULARITY 256;

ALTER TABLE sensor_data_1m ADD INDEX IF NOT EXISTS idx_1m_ts timestamp TYPE minmax GRANULARITY 4;
ALTER TABLE sensor_data_5m ADD INDEX IF NOT EXISTS idx_5m_ts timestamp TYPE minmax GRANULARITY 4;
