#include "common.h"
#include "mqtt_client.h"
#include "clickhouse_client.h"
#include "dem_model.h"
#include "genetic_optimizer.h"
#include "alert_system.h"
#include "http_server.h"

#include <iostream>
#include <memory>
#include <thread>
#include <csignal>
#include <chrono>

using namespace stone_mill;

std::atomic<bool> g_running{true};

void signal_handler(int) {
    std::cout << "\n[MAIN] Received signal, shutting down..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "========================================" << std::endl;
    std::cout << "古代石碾粉碎过程离散元仿真与粒度优化系统" << std::endl;
    std::cout << "Stone Mill DEM Simulation & Optimization System" << std::endl;
    std::cout << "========================================" << std::endl;

    ProcessConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mqtt-host" && i + 1 < argc) {
            config.mqtt_host = argv[++i];
        } else if (arg == "--mqtt-port" && i + 1 < argc) {
            config.mqtt_port = std::stoi(argv[++i]);
        } else if (arg == "--clickhouse-host" && i + 1 < argc) {
            config.clickhouse_host = argv[++i];
        } else if (arg == "--http-port" && i + 1 < argc) {
            config.http_port = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --mqtt-host <host>      MQTT broker host (default: localhost)" << std::endl;
            std::cout << "  --mqtt-port <port>      MQTT broker port (default: 1883)" << std::endl;
            std::cout << "  --clickhouse-host <host> ClickHouse host (default: localhost)" << std::endl;
            std::cout << "  --http-port <port>      HTTP server port (default: 8080)" << std::endl;
            return 0;
        }
    }

    auto mqtt_client = std::make_shared<MQTTClient>(config);
    auto db_client = std::make_shared<ClickHouseClient>(config);
    auto dem_model = std::make_shared<DEMModel>();
    auto optimizer = std::make_shared<GeneticOptimizer>();
    auto alert_system = std::make_shared<AlertSystem>(mqtt_client, config.thresholds);
    auto http_server = std::make_shared<HTTPServer>(config, db_client, dem_model, optimizer, alert_system);

    optimizer->set_dem_model(dem_model);

    DEMConfig dem_config;
    dem_config.dt = 1e-5;
    dem_model->set_config(dem_config);

    BreakageModel breakage_model;
    dem_model->set_breakage_model(breakage_model);

    std::cout << "\n[MAIN] Initializing components..." << std::endl;

    if (!mqtt_client->connect()) {
        std::cerr << "[MAIN] Failed to connect to MQTT broker" << std::endl;
    }

    mqtt_client->subscribe(config.mqtt_topic,
        [&db_client, &alert_system](const SensorData& data) {
            std::cout << "[MAIN] Received sensor data from mill " << data.mill_id
                      << ": speed=" << data.roller_speed
                      << ", yield=" << data.yield << std::endl;
            db_client->insert_sensor_data(data);
            alert_system->process_sensor_data(data);
        });

    mqtt_client->start();

    if (!db_client->connect()) {
        std::cerr << "[MAIN] Failed to connect to ClickHouse" << std::endl;
    }

    db_client->start_async_writer();

    if (!http_server->start()) {
        std::cerr << "[MAIN] Failed to start HTTP server" << std::endl;
        return 1;
    }

    std::cout << "\n[MAIN] System started successfully!" << std::endl;
    std::cout << "[MAIN] HTTP server: http://localhost:" << config.http_port << std::endl;
    std::cout << "[MAIN] MQTT topic: " << config.mqtt_topic << std::endl;
    std::cout << "[MAIN] Alert topic: " << config.alert_topic << std::endl;
    std::cout << "\n[MAIN] Press Ctrl+C to stop" << std::endl;
    std::cout << "========================================" << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n[MAIN] Shutting down..." << std::endl;

    http_server->stop();
    mqtt_client->stop();
    db_client->stop_async_writer();

    std::cout << "[MAIN] Shutdown complete" << std::endl;

    return 0;
}
