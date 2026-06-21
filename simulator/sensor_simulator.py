#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
古代石碾传感器模拟器
Stone Mill Sensor Simulator

模拟汉代石碾的传感器数据，通过MQTT上报：
- 碾轮转速 (rad/s)
- 碾轮压力 (N)
- 谷物粒度分布 (6个粒度区间的质量占比)
- 产量 (kg/min)
- 碾轮磨损度 (%)
- 碾轮间隙 (mm)

每台石碾每分钟上报一次数据
"""

import json
import time
import random
import math
import argparse
import threading
from datetime import datetime, timedelta

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("请先安装paho-mqtt: pip install paho-mqtt")
    exit(1)


class StoneMillSensor:
    """石碾传感器模拟器"""

    GRAIN_SIZE_BINS = [
        "0-1mm",
        "1-2mm",
        "2-3mm",
        "3-4mm",
        "4-5mm",
        ">5mm"
    ]

    def __init__(self, mill_id: int, mqtt_client: mqtt.Client,
                 base_topic: str = "stone_mill/sensor"):
        self.mill_id = mill_id
        self.mqtt_client = mqtt_client
        self.topic = f"{base_topic}/{mill_id}"

        self.roller_speed_base = random.uniform(12, 18)
        self.roller_pressure_base = random.uniform(400, 600)
        self.roller_gap = random.uniform(1.5, 2.5)
        self.wear_degree = random.uniform(20, 40)
        self.wear_rate = random.uniform(0.005, 0.015)

        self.moisture_base = random.uniform(0.10, 0.15)
        self.moisture = self.moisture_base

        self.yield_base = random.uniform(8, 12)
        self.running = False
        self.thread = None

    def generate_size_distribution(self, speed: float, gap: float, wear: float,
                                     moisture: float = 0.12) -> dict:
        """
        基于破碎函数理论生成粒度分布
        使用Tavar破碎模型：B(x) = 1 - exp(-(x/x50)^n)
        考虑湿度修正：高湿度降低破碎效率，细粒比例减少
        """
        wear_factor = 1 + wear / 100.0 * 0.3
        effective_speed = speed * wear_factor
        effective_gap = gap * (1 + wear / 200.0)

        moisture_ref = 0.12
        moisture_factor = 1.0 - 0.8 * (moisture - moisture_ref) / moisture_ref
        moisture_factor = max(0.5, min(1.5, moisture_factor))

        fine_ratio = effective_speed / 30.0 * moisture_factor
        coarse_ratio = effective_gap / 5.0 / moisture_factor

        dist = {}
        total = 0.0

        for i, bin_name in enumerate(self.GRAIN_SIZE_BINS):
            if i < 2:
                base = fine_ratio * random.uniform(0.8, 1.2)
            elif i < 4:
                base = random.uniform(0.8, 1.2)
            else:
                base = coarse_ratio * random.uniform(0.8, 1.2)

            dist[bin_name] = base * math.exp(-i * 0.3)
            total += dist[bin_name]

        for bin_name in self.GRAIN_SIZE_BINS:
            dist[bin_name] = dist[bin_name] / total

        return dist

    def generate_data(self) -> dict:
        """生成传感器数据"""
        timestamp = datetime.now()

        time_factor = math.sin(timestamp.hour / 24.0 * 2 * math.pi) * 0.1 + 1.0

        self.wear_degree += self.wear_rate
        if self.wear_degree > 95:
            self.wear_degree = 95

        self.moisture = self.moisture_base + math.sin(time.time() / 3600.0) * 0.02
        self.moisture = max(0.05, min(0.25, self.moisture))

        roller_speed = self.roller_speed_base * time_factor * random.uniform(0.95, 1.05)
        roller_pressure = self.roller_pressure_base * time_factor * random.uniform(0.95, 1.05)

        size_dist = self.generate_size_distribution(
            roller_speed, self.roller_gap, self.wear_degree, self.moisture
        )

        fine_fraction = size_dist["0-1mm"] + size_dist["1-2mm"] + size_dist["2-3mm"]
        efficiency = 0.6 + 0.3 * fine_fraction
        yield_val = self.yield_base * efficiency * time_factor * random.uniform(0.9, 1.1)

        if self.wear_degree > 70:
            yield_val *= (1 - (self.wear_degree - 70) / 100)

        data = {
            "mill_id": self.mill_id,
            "timestamp": timestamp.isoformat(),
            "timestamp_unix": time.time() * 1000,
            "roller_speed": round(roller_speed, 3),
            "roller_pressure": round(roller_pressure, 3),
            "grain_size_0_1mm": round(size_dist["0-1mm"], 4),
            "grain_size_1_2mm": round(size_dist["1-2mm"], 4),
            "grain_size_2_3mm": round(size_dist["2-3mm"], 4),
            "grain_size_3_4mm": round(size_dist["3-4mm"], 4),
            "grain_size_4_5mm": round(size_dist["4-5mm"], 4),
            "grain_size_gt5mm": round(size_dist[">5mm"], 4),
            "yield": round(yield_val, 3),
            "wear_degree": round(self.wear_degree, 2),
            "roller_gap": round(self.roller_gap, 2),
            "moisture": round(self.moisture, 4)
        }

        return data

    def format_payload(self, data: dict) -> str:
        """格式化为键值对字符串"""
        parts = [
            f"mill_id={data['mill_id']}",
            f"timestamp={data['timestamp']}",
            f"roller_speed={data['roller_speed']}",
            f"roller_pressure={data['roller_pressure']}",
            f"grain_size_0_1mm={data['grain_size_0_1mm']}",
            f"grain_size_1_2mm={data['grain_size_1_2mm']}",
            f"grain_size_2_3mm={data['grain_size_2_3mm']}",
            f"grain_size_3_4mm={data['grain_size_3_4mm']}",
            f"grain_size_4_5mm={data['grain_size_4_5mm']}",
            f"grain_size_gt5mm={data['grain_size_gt5mm']}",
            f"yield={data['yield']}",
            f"wear_degree={data['wear_degree']}",
            f"roller_gap={data['roller_gap']}"
        ]
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
            print(f"[传感器 {self.mill_id}] 启动，上报间隔 {interval}秒")
            while self.running:
                try:
                    data = self.generate_data()
                    success = self.publish_data(data, use_json)
                    status = "✓" if success else "✗"
                    fine = data['grain_size_0_1mm'] + data['grain_size_1_2mm'] + data['grain_size_2_3mm']
                    print(f"{status} [{datetime.now().strftime('%H:%M:%S')}] 石碾{self.mill_id}: "
                          f"转速={data['roller_speed']:.2f}rad/s, "
                          f"压力={data['roller_pressure']:.0f}N, "
                          f"产量={data['yield']:.2f}kg/min, "
                          f"细粉占比={fine*100:.1f}%, "
                          f"磨损={data['wear_degree']:.1f}%")
                except Exception as e:
                    print(f"[传感器 {self.mill_id}] 错误: {e}")

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
    parser = argparse.ArgumentParser(description="石碾传感器模拟器")
    parser.add_argument("--host", default="localhost", help="MQTT服务器地址")
    parser.add_argument("--port", type=int, default=1883, help="MQTT服务器端口")
    parser.add_argument("--topic", default="stone_mill/sensor", help="MQTT主题前缀")
    parser.add_argument("--alert-topic", default="stone_mill/alerts", help="告警主题")
    parser.add_argument("--mills", type=int, default=3, help="模拟石碾数量")
    parser.add_argument("--interval", type=int, default=60, help="上报间隔(秒)")
    parser.add_argument("--json", action="store_true", help="使用JSON格式发布")
    parser.add_argument("--username", default=None, help="MQTT用户名")
    parser.add_argument("--password", default=None, help="MQTT密码")
    parser.add_argument("--test-alert", action="store_true", help="发送测试告警")

    args = parser.parse_args()

    print("=" * 60)
    print("古代石碾传感器模拟器")
    print("Stone Mill Sensor Simulator")
    print("=" * 60)

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
        exit(1)

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
        sensor = StoneMillSensor(mill_id, mqtt_client, args.topic)
        sensors.append(sensor)
        sensor.start(args.interval, args.json)

    print("\n" + "=" * 60)
    print(f"已启动 {args.mills} 台石碾传感器")
    print(f"上报主题: {args.topic}/<石碾ID>")
    print(f"上报间隔: {args.interval}秒")
    print("按 Ctrl+C 停止")
    print("=" * 60 + "\n")

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
