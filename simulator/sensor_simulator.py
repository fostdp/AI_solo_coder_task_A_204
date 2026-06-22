#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
古代石碾传感器模拟器 (v2 - 多谷物/含水率曲线)
Stone Mill Sensor Simulator v2

模拟汉代石碾的传感器数据，通过MQTT上报：
- 碾轮转速 (rad/s)
- 碾轮压力 (N)
- 谷物粒度分布 (6个粒度区间的质量占比)
- 产量 (kg/min)
- 碾轮磨损度 (%)
- 碾轮间隙 (mm)
- 含水率 (%)
- 谷物种类 (wheat/rice/corn/soy/millet/mixed)

每台石碾按设定间隔上报数据
"""

import json
import time
import random
import math
import argparse
import threading
import os
import sys
from datetime import datetime, timedelta

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("请先安装paho-mqtt: pip install paho-mqtt")
    sys.exit(1)


GRAIN_PROFILES = {
    "wheat": {
        "zh": "小麦",
        "hardness": 1.0,
        "yield_kg_per_min_base": 10.0,
        "moisture_default": 0.13,
        "moisture_diurnal_amp": 0.025,
        "size_baseline": [0.15, 0.28, 0.27, 0.18, 0.09, 0.03],
        "breakage_sensitivity": 1.0,
        "pressure_base": 500,
        "speed_base": 15,
        "gap_base": 2.0,
        "description": "中等硬度，面粉标准原料"
    },
    "rice": {
        "zh": "水稻",
        "hardness": 0.7,
        "yield_kg_per_min_base": 12.0,
        "moisture_default": 0.15,
        "moisture_diurnal_amp": 0.03,
        "size_baseline": [0.10, 0.18, 0.25, 0.25, 0.15, 0.07],
        "breakage_sensitivity": 1.3,
        "pressure_base": 420,
        "speed_base": 17,
        "gap_base": 2.5,
        "description": "较软，易成粉，谷壳占比高"
    },
    "corn": {
        "zh": "玉米",
        "hardness": 1.4,
        "yield_kg_per_min_base": 8.0,
        "moisture_default": 0.14,
        "moisture_diurnal_amp": 0.02,
        "size_baseline": [0.05, 0.12, 0.22, 0.28, 0.22, 0.11],
        "breakage_sensitivity": 0.7,
        "pressure_base": 620,
        "speed_base": 13,
        "gap_base": 2.8,
        "description": "坚硬，大颗粒多，破碎难度大"
    },
    "soy": {
        "zh": "大豆",
        "hardness": 0.6,
        "yield_kg_per_min_base": 9.0,
        "moisture_default": 0.11,
        "moisture_diurnal_amp": 0.015,
        "size_baseline": [0.03, 0.08, 0.18, 0.30, 0.28, 0.13],
        "breakage_sensitivity": 0.9,
        "pressure_base": 450,
        "speed_base": 14,
        "gap_base": 3.0,
        "description": "含油脂，易碎但易出油，颗粒偏大"
    },
    "millet": {
        "zh": "粟(小米)",
        "hardness": 1.1,
        "yield_kg_per_min_base": 11.0,
        "moisture_default": 0.12,
        "moisture_diurnal_amp": 0.02,
        "size_baseline": [0.22, 0.32, 0.24, 0.14, 0.06, 0.02],
        "breakage_sensitivity": 1.1,
        "pressure_base": 480,
        "speed_base": 16,
        "gap_base": 1.8,
        "description": "小颗粒，易成细粉"
    },
    "mixed": {
        "zh": "混合谷物",
        "hardness": 1.0,
        "yield_kg_per_min_base": 10.0,
        "moisture_default": 0.13,
        "moisture_diurnal_amp": 0.02,
        "size_baseline": [0.12, 0.22, 0.26, 0.23, 0.12, 0.05],
        "breakage_sensitivity": 1.0,
        "pressure_base": 500,
        "speed_base": 15,
        "gap_base": 2.2,
        "description": "多种谷物混合基准"
    },
}

SUPPORTED_GRAINS = list(GRAIN_PROFILES.keys())


class StoneMillSensor:
    """石碾传感器模拟器 (多谷物 + 含水率曲线)"""

    GRAIN_SIZE_BINS = [
        "0-1mm",
        "1-2mm",
        "2-3mm",
        "3-4mm",
        "4-5mm",
        ">5mm"
    ]

    def __init__(self, mill_id: int, mqtt_client: mqtt.Client,
                 base_topic: str = "stone_mill/sensor",
                 grain_type: str = "wheat",
                 moisture: float = None,
                 use_diurnal_moisture: bool = True):
        self.mill_id = mill_id
        self.mqtt_client = mqtt_client
        self.topic = f"{base_topic}/{mill_id}"

        if grain_type not in GRAIN_PROFILES:
            raise ValueError(f"未知谷物种类: {grain_type}, 可选: {SUPPORTED_GRAINS}")
        self.grain_type = grain_type
        self.profile = GRAIN_PROFILES[grain_type]

        self.roller_speed_base = self.profile["speed_base"] + random.uniform(-1, 1)
        self.roller_pressure_base = self.profile["pressure_base"] + random.uniform(-30, 30)
        self.roller_gap = self.profile["gap_base"] + random.uniform(-0.3, 0.3)
        self.wear_degree = random.uniform(20, 40)
        self.wear_rate = random.uniform(0.005, 0.015)

        self.use_diurnal_moisture = use_diurnal_moisture
        if moisture is not None:
            self.moisture_base = moisture
        else:
            self.moisture_base = self.profile["moisture_default"]
        self.moisture = self.moisture_base

        self.yield_base = self.profile["yield_kg_per_min_base"]
        self.running = False
        self.thread = None

    def compute_moisture(self) -> float:
        """计算当前含水率：基础 + 正弦日周期 + 微小随机扰动"""
        if not self.use_diurnal_moisture:
            m = self.moisture_base + random.uniform(-0.005, 0.005)
        else:
            hour = datetime.now().hour + datetime.now().minute / 60.0
            phase = (hour - 6.0) / 24.0 * 2.0 * math.pi
            amp = self.profile["moisture_diurnal_amp"]
            m = self.moisture_base + math.sin(phase) * amp + random.uniform(-0.005, 0.005)
        return max(0.04, min(0.30, m))

    def generate_size_distribution(self, speed: float, gap: float, wear: float,
                                     moisture: float) -> dict:
        """
        基于破碎函数理论生成粒度分布
        使用Tavar破碎模型：B(x) = 1 - exp(-(x/x50)^n)
        考虑谷物种类硬度、湿度修正
        """
        prof = self.profile
        hardness = prof["hardness"]
        breakage_sens = prof["breakage_sensitivity"]

        wear_factor = 1 + wear / 100.0 * 0.3
        effective_speed = speed * wear_factor
        effective_gap = gap * (1 + wear / 200.0)

        moisture_ref = prof["moisture_default"]
        moisture_factor = 1.0 - 0.8 * (moisture - moisture_ref) / max(0.01, moisture_ref)
        moisture_factor = max(0.5, min(1.5, moisture_factor))

        hardness_factor = 1.0 / hardness

        fine_ratio = (effective_speed / 30.0) * moisture_factor * hardness_factor * breakage_sens
        coarse_ratio = (effective_gap / 5.0) / moisture_factor / max(0.5, hardness_factor)

        dist = {}
        total = 0.0
        for i, bin_name in enumerate(self.GRAIN_SIZE_BINS):
            base_baseline = prof["size_baseline"][i]
            if i < 2:
                base = base_baseline * fine_ratio * random.uniform(0.85, 1.15)
            elif i < 4:
                base = base_baseline * random.uniform(0.9, 1.1)
            else:
                base = base_baseline * coarse_ratio * random.uniform(0.85, 1.15)
            dist[bin_name] = max(0.001, base)
            total += dist[bin_name]

        for bin_name in self.GRAIN_SIZE_BINS:
            dist[bin_name] = dist[bin_name] / total

        return dist

    def generate_data(self) -> dict:
        """生成传感器数据"""
        timestamp = datetime.now()

        hour_factor = math.sin(timestamp.hour / 24.0 * 2 * math.pi) * 0.1 + 1.0

        self.wear_degree += self.wear_rate
        if self.wear_degree > 95:
            self.wear_degree = 95

        self.moisture = self.compute_moisture()

        roller_speed = self.roller_speed_base * hour_factor * random.uniform(0.95, 1.05)
        roller_pressure = self.roller_pressure_base * hour_factor * random.uniform(0.95, 1.05)

        size_dist = self.generate_size_distribution(
            roller_speed, self.roller_gap, self.wear_degree, self.moisture
        )

        fine_fraction = size_dist["0-1mm"] + size_dist["1-2mm"] + size_dist["2-3mm"]
        efficiency = 0.6 + 0.3 * fine_fraction
        yield_val = self.yield_base * efficiency * hour_factor * random.uniform(0.9, 1.1)

        if self.wear_degree > 70:
            yield_val *= (1 - (self.wear_degree - 70) / 100)

        dist_array = [round(size_dist[b], 4) for b in self.GRAIN_SIZE_BINS]

        data = {
            "mill_id": self.mill_id,
            "grain_type": self.grain_type,
            "grain_zh": self.profile["zh"],
            "timestamp": timestamp.isoformat(),
            "timestamp_ns": int(time.time() * 1e9),
            "roller_speed": round(roller_speed, 3),
            "roller_pressure": round(roller_pressure, 3),
            "grain_size_0_1mm": dist_array[0],
            "grain_size_1_2mm": dist_array[1],
            "grain_size_2_3mm": dist_array[2],
            "grain_size_3_4mm": dist_array[3],
            "grain_size_4_5mm": dist_array[4],
            "grain_size_gt5mm": dist_array[5],
            "yield": round(yield_val, 3),
            "wear_degree": round(self.wear_degree, 2),
            "roller_gap": round(self.roller_gap, 2),
            "moisture": round(self.moisture, 4),
        }

        return data

    def format_payload(self, data: dict) -> str:
        """格式化为键值对字符串"""
        parts = [f"{k}={v}" for k, v in data.items()]
        return ",".join(parts)

    def publish_data(self, data: dict, use_json: bool = False) -> bool:
        """发布数据到MQTT"""
        try:
            if use_json:
                payload = json.dumps(data, ensure_ascii=False)
            else:
                payload = self.format_payload(data)

            result = self.mqtt_client.publish(self.topic, payload, qos=1)
            return result.rc == mqtt.MQTT_ERR_SUCCESS
        except Exception as e:
            print(f"[传感器 {self.mill_id}] 发布失败: {e}")
            return False

    def start(self, interval: int = 60, use_json: bool = False):
        """启动传感器模拟器"""
        self.running = True

        def run():
            print(f"[传感器 {self.mill_id}] 启动 (谷物={self.profile['zh']}/{self.grain_type}, "
                  f"基础含水率={self.moisture_base:.2%}, 间隔={interval}s)")
            while self.running:
                try:
                    data = self.generate_data()
                    success = self.publish_data(data, use_json)
                    status = "✓" if success else "✗"
                    fine = data['grain_size_0_1mm'] + data['grain_size_1_2mm'] + data['grain_size_2_3mm']
                    print(f"{status} [{datetime.now().strftime('%H:%M:%S')}] "
                          f"石碾{self.mill_id}({self.profile['zh']}): "
                          f"转速={data['roller_speed']:.2f}rad/s, "
                          f"压力={data['roller_pressure']:.0f}N, "
                          f"产量={data['yield']:.2f}kg/min, "
                          f"细粉占比={fine*100:.1f}%, "
                          f"含水率={data['moisture']*100:.2f}%, "
                          f"磨损={data['wear_degree']:.1f}%")
                except Exception as e:
                    print(f"[传感器 {self.mill_id}] 错误: {e}")
                    import traceback
                    traceback.print_exc()

                time.sleep(interval)

        self.thread = threading.Thread(target=run, daemon=True)
        self.thread.start()

    def stop(self):
        """停止传感器模拟器"""
        self.running = False
        if self.thread:
            self.thread.join(timeout=2)
        print(f"[传感器 {self.mill_id}] 已停止")


def on_connect(client, userdata, flags, rc):
    """MQTT连接回调"""
    if rc == 0:
        print(f"[MQTT] 已连接到 {userdata['host']}:{userdata['port']}")
    else:
        print(f"[MQTT] 连接失败，错误码: {rc}")


def on_disconnect(client, userdata, rc):
    """MQTT断开连接回调"""
    print(f"[MQTT] 连接断开，错误码: {rc}")


def main():
    parser = argparse.ArgumentParser(description="石碾传感器模拟器 v2 (多谷物 + 含水率曲线)")
    parser.add_argument("--host", default=os.environ.get("MQTT_HOST", "localhost"),
                        help="MQTT服务器地址 (或环境变量 MQTT_HOST)")
    parser.add_argument("--port", type=int, default=int(os.environ.get("MQTT_PORT", "1883")),
                        help="MQTT服务器端口 (或环境变量 MQTT_PORT)")
    parser.add_argument("--topic", default=os.environ.get("MQTT_TOPIC", "stone_mill/sensor"),
                        help="MQTT主题前缀 (或环境变量 MQTT_TOPIC)")
    parser.add_argument("--alert-topic", default=os.environ.get("MQTT_ALERT_TOPIC", "stone_mill/alerts"),
                        help="告警主题")
    parser.add_argument("--mills", type=int, default=int(os.environ.get("SIM_MILLS", "3")),
                        help="模拟石碾数量")
    parser.add_argument("--interval", type=int, default=int(os.environ.get("SIM_INTERVAL", "5")),
                        help="上报间隔(秒)，默认5秒")
    parser.add_argument("--json", action="store_true", default=True,
                        help="使用JSON格式发布(默认)")
    parser.add_argument("--kv", action="store_true", help="使用key=value格式发布")
    parser.add_argument("--username", default=os.environ.get("MQTT_USERNAME"), help="MQTT用户名")
    parser.add_argument("--password", default=os.environ.get("MQTT_PASSWORD"), help="MQTT密码")
    parser.add_argument("--grain-type", default=os.environ.get("SIM_GRAIN", "wheat"),
                        help=f"谷物种类: {', '.join(SUPPORTED_GRAINS)} (或环境变量 SIM_GRAIN)")
    parser.add_argument("--mixed", action="store_true", help="每台石碾随机分配不同谷物")
    parser.add_argument("--moisture", type=float, default=None,
                        help="固定含水率(0~1)，不设则使用谷物默认值+日周期")
    parser.add_argument("--no-diurnal", action="store_true", help="关闭含水率日周期曲线")
    parser.add_argument("--list-grains", action="store_true", help="列出支持的谷物种类并退出")
    parser.add_argument("--test-alert", action="store_true", help="发送测试告警")

    args = parser.parse_args()

    if args.list_grains:
        print("支持的谷物种类:")
        for k, v in GRAIN_PROFILES.items():
            print(f"  {k:8s} [{v['zh']}] 硬度={v['hardness']}, 基础含水率={v['moisture_default']:.2%}, "
                  f"产量≈{v['yield_kg_per_min_base']}kg/min - {v['description']}")
        return

    if args.grain_type not in GRAIN_PROFILES and not args.mixed:
        print(f"错误: 未知谷物种类 '{args.grain_type}'，使用 --list-grains 查看可用种类")
        sys.exit(2)

    use_json = True if not args.kv else False

    print("=" * 70)
    print("古代石碾传感器模拟器 v2 (多谷物 + 含水率曲线)")
    print("Stone Mill Sensor Simulator v2")
    print("=" * 70)

    mqtt_client = mqtt.Client(userdata={"host": args.host, "port": args.port})
    mqtt_client.on_connect = on_connect
    mqtt_client.on_disconnect = on_disconnect

    if args.username:
        mqtt_client.username_pw_set(args.username, args.password)

    try:
        mqtt_client.connect(args.host, args.port, keepalive=60)
        mqtt_client.loop_start()
    except Exception as e:
        print(f"[MQTT] 无法连接: {e}")
        print("请确保MQTT服务器已启动，或使用 --host 指定正确地址")
        sys.exit(1)

    time.sleep(1)

    if args.test_alert:
        alert = {
            "alert_id": f"test_{int(time.time())}",
            "mill_id": 1,
            "timestamp": datetime.now().isoformat(),
            "alert_type": "wear",
            "severity": "warning",
            "message": "测试告警：碾轮磨损度过高",
            "current_value": 75.5,
            "threshold": 70.0,
            "resolved": False
        }
        mqtt_client.publish(args.alert_topic, json.dumps(alert, ensure_ascii=False))
        print(f"[测试] 已发送测试告警到 {args.alert_topic}")
        time.sleep(1)
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        return

    sensors = []
    for i in range(args.mills):
        mill_id = i + 1
        if args.mixed:
            grain_type = random.choice(SUPPORTED_GRAINS)
        else:
            grain_type = args.grain_type
        try:
            sensor = StoneMillSensor(
                mill_id, mqtt_client, args.topic,
                grain_type=grain_type,
                moisture=args.moisture,
                use_diurnal_moisture=not args.no_diurnal
            )
            sensors.append(sensor)
            sensor.start(args.interval, use_json)
        except Exception as e:
            print(f"[传感器 {mill_id}] 初始化失败: {e}")

    print("\n" + "=" * 70)
    print(f"已启动 {len(sensors)} 台石碾传感器")
    print(f"MQTT Broker:  {args.host}:{args.port}")
    print(f"上报主题:    {args.topic}/<石碾ID>")
    print(f"告警主题:    {args.alert_topic}")
    print(f"上报间隔:    {args.interval}秒")
    print(f"发布格式:    {'JSON' if use_json else 'key=value'}")
    if args.moisture is not None:
        print(f"固定含水率:  {args.moisture:.2%} ({'禁用' if args.no_diurnal else '启用'}日周期)")
    else:
        print(f"含水率:      谷物默认值 ({'禁用' if args.no_diurnal else '启用'}日周期)")
    if args.mixed:
        print(f"谷物:        随机混合 (每台石碾随机分配)")
    else:
        print(f"谷物:        {GRAIN_PROFILES[args.grain_type]['zh']} ({args.grain_type})")
    print("按 Ctrl+C 停止")
    print("=" * 70 + "\n")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n\n正在停止传感器...")
        for sensor in sensors:
            sensor.stop()

        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        print("已退出")


if __name__ == "__main__":
    main()
