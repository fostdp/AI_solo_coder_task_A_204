class StoneMillAPI {
    constructor(baseUrl = 'http://localhost:8080') {
        this.baseUrl = baseUrl;
    }

    setBaseUrl(url) {
        this.baseUrl = url;
    }

    async request(endpoint, method = 'GET', params = {}) {
        const url = new URL(`${this.baseUrl}${endpoint}`);

        if (method === 'GET') {
            Object.keys(params).forEach(key => {
                if (params[key] !== undefined && params[key] !== null) {
                    url.searchParams.append(key, params[key]);
                }
            });
        }

        const options = {
            method,
            headers: {
                'Content-Type': 'application/json',
            },
        };

        if (method === 'POST') {
            options.body = JSON.stringify(params);
        }

        try {
            const response = await fetch(url.toString(), options);
            const data = await response.json();
            return data;
        } catch (error) {
            console.error(`API Error [${method} ${endpoint}]:`, error);
            return {
                success: false,
                data: null,
                error: error.message
            };
        }
    }

    async getSensorData(millId = 1, limit = 100) {
        return this.request('/api/sensor_data', 'GET', { mill_id: millId, limit });
    }

    async getAlerts(millId = 0, onlyUnresolved = true) {
        return this.request('/api/alerts', 'GET', {
            mill_id: millId,
            only_unresolved: onlyUnresolved ? 1 : 0
        });
    }

    async resolveAlert(alertId) {
        return this.request('/api/alerts/resolve', 'POST', { alert_id: alertId });
    }

    async getGrainStats(millId = 1) {
        return this.request('/api/grain_stats', 'GET', { mill_id: millId });
    }

    async runDEMSimulation(params) {
        return this.request('/api/dem/simulate', 'POST', params);
    }

    async runOptimization(params) {
        return this.request('/api/optimize', 'POST', params);
    }

    async getOptimizationResult(millId = 1) {
        return this.request('/api/optimization/result', 'GET', { mill_id: millId });
    }

    async getCurrentStatus(millId = 1) {
        return this.request('/api/status', 'GET', { mill_id: millId });
    }

    async getThresholds() {
        return this.request('/api/thresholds', 'GET');
    }

    async setThresholds(params) {
        return this.request('/api/thresholds', 'POST', params);
    }

    async testConnection() {
        const startTime = Date.now();
        try {
            const result = await this.getCurrentStatus(1);
            const latency = Date.now() - startTime;
            return {
                success: result.success,
                latency,
                message: result.success ? `连接成功 (${latency}ms)` : '连接失败'
            };
        } catch (error) {
            return {
                success: false,
                latency: 0,
                message: `连接失败: ${error.message}`
            };
        }
    }
}

const api = new StoneMillAPI();
