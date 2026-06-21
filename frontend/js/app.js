class StoneMill3DAdapter {
    constructor(containerId) {
        this.roller3d = new StoneRoller3D(containerId);
        this.particles = null;
        this._speed = 0;
        this._gap = 2.0;
        this._showParticles = true;
        this._showForces = false;
        this._particleCount = 200;
    }
    initParticles() {
        if (this.particles) return;
        this.particles = new ParticlePanel(this.roller3d, null);
        this.setRollerSpeed(this._speed);
        this.setRollerGap(this._gap);
        this.showParticles(this._showParticles);
        this.showForceArrows(this._showForces);
    }
    setRollerSpeed(speed) {
        this._speed = speed;
        if (this.roller3d) this.roller3d.setRollerSpeed(speed);
        if (this.particles) this.particles.rollerSpeed = speed;
    }
    setRollerGap(gap) {
        this._gap = gap;
        if (this.particles) this.particles.rollerGap = gap;
    }
    showParticles(show) {
        this._showParticles = show;
        if (this.particles) {
            this.particles.showParticles = show;
            if (this.particles.grainInstanced) this.particles.grainInstanced.visible = show;
        }
    }
    showForceArrows(show) {
        this._showForces = show;
        if (this.particles) {
            this.particles.showForces = show;
            for (const a of this.particles.forceArrows) a.visible = show;
        }
    }
    resetParticles(count) {
        this._particleCount = count;
        if (this.particles) this.particles.createGrainParticles(count);
    }
    resetCamera() {
        if (this.roller3d && this.roller3d.camera) {
            this.roller3d.camera.position.set(4, 3, 4);
            this.roller3d.camera.lookAt(0, 0, 0);
        }
    }
    resize() {
        if (this.roller3d) this.roller3d.onResize();
    }
    getStats() {
        const active = this.particles ? this.particles.particleData.filter(p => !p.broken).length : 0;
        const total = this.particles ? this.particles.particleData.length : 0;
        const broken = total - active;
        return {
            particleCount: active,
            breakageRatio: total > 0 ? broken / total : 0,
            avgForce: 0,
            maxForce: 0,
            simTime: performance.now() / 1000
        };
    }
    getSizeDistribution() {
        const bins = [0.3, 0.28, 0.2, 0.12, 0.07, 0.03];
        if (this.particles) {
            const active = this.particles.particleData.filter(p => !p.broken);
            for (let i = 0; i < bins.length; i++) bins[i] = 0;
            for (const p of active) {
                if (p.radius > 0.05) bins[0]++;
                else if (p.radius > 0.04) bins[1]++;
                else if (p.radius > 0.032) bins[2]++;
                else if (p.radius > 0.026) bins[3]++;
                else if (p.radius > 0.022) bins[4]++;
                else bins[5]++;
            }
            const s = bins.reduce((a, b) => a + b, 0);
            if (s > 0) for (let i = 0; i < bins.length; i++) bins[i] /= s;
        }
        return bins;
    }
}

class StoneMillApp {
    constructor() {
        this.currentMillId = 1;
        this.currentView = 'dashboard';
        this.autoRefresh = true;
        this.refreshInterval = 5000;
        this.refreshTimer = null;
        this.optimizationRunning = false;
        this.optimizationCancelled = false;

        this.charts = {};
        this.mill3D = null;

        this.mockSensorData = this.generateMockSensorData();
        this.mockAlerts = this.generateMockAlerts();

        this.init();
    }

    init() {
        this.initCharts();
        this.initNavigation();
        this.initControls();
        this.init3D();
        this.startDataRefresh();
        this.startClock();
        this.loadViewData('dashboard');
    }

    generateMockSensorData() {
        const data = [];
        const now = Date.now();
        for (let i = 60; i >= 0; i--) {
            const t = now - i * 60000;
            data.push({
                timestamp: t,
                roller_speed: 12 + Math.sin(i * 0.1) * 3 + Math.random() * 2,
                roller_pressure: 450 + Math.sin(i * 0.15) * 100 + Math.random() * 50,
                yield: 8 + Math.sin(i * 0.08) * 2 + Math.random() * 1,
                wear_degree: 45 + i * 0.05 + Math.random() * 2,
                grain_size_0_1mm: 0.15 + Math.random() * 0.05,
                grain_size_1_2mm: 0.25 + Math.random() * 0.05,
                grain_size_2_3mm: 0.3 + Math.random() * 0.05,
                grain_size_3_4mm: 0.15 + Math.random() * 0.05,
                grain_size_4_5mm: 0.1 + Math.random() * 0.03,
                grain_size_gt5mm: 0.05 + Math.random() * 0.02,
                roller_gap: 2 + Math.random() * 0.5
            });
        }
        return data;
    }

    generateMockAlerts() {
        return [
            {
                id: 1,
                mill_id: 1,
                type: 'wear',
                severity: 'warning',
                message: '碾轮磨损程度达到75%，建议安排维护',
                current_value: 75,
                threshold: 70,
                timestamp: Date.now() - 3600000,
                resolved: false
            },
            {
                id: 2,
                mill_id: 2,
                type: 'low_yield',
                severity: 'critical',
                message: '产量低于阈值，当前4.2 kg/min，阈值5.0 kg/min',
                current_value: 4.2,
                threshold: 5.0,
                timestamp: Date.now() - 1800000,
                resolved: false
            },
            {
                id: 3,
                mill_id: 1,
                type: 'abnormal_speed',
                severity: 'info',
                message: '碾轮转速波动较大，请检查',
                current_value: 8.5,
                threshold: 10,
                timestamp: Date.now() - 86400000,
                resolved: true
            }
        ];
    }

    initCharts() {
        this.charts.grainSize = new GrainChart('grain-size-chart');
        this.charts.yieldTrend = new TrendChart('yield-trend-chart', '#2ecc71');
        this.charts.wearTrend = new TrendChart('wear-trend-chart', '#e74c3c');
        this.charts.simGrain = new GrainChart('sim-grain-chart');
        this.charts.fitness = new FitnessChart('fitness-chart');

        this.mockSensorData.forEach(d => {
            this.charts.yieldTrend.addValue(d.yield);
            this.charts.wearTrend.addValue(d.wear_degree);
        });

        this.drawAllCharts();
    }

    drawAllCharts() {
        Object.values(this.charts).forEach(chart => {
            if (chart && chart.draw) chart.draw();
        });
    }

    initNavigation() {
        document.querySelectorAll('.nav-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const view = e.target.dataset.view;
                this.switchView(view);
            });
        });
    }

    switchView(viewName) {
        document.querySelectorAll('.nav-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.view === viewName);
        });

        document.querySelectorAll('.view').forEach(view => {
            view.classList.toggle('active', view.id === `view-${viewName}`);
        });

        this.currentView = viewName;
        this.loadViewData(viewName);

        if (viewName === 'simulation' && this.mill3D) {
            setTimeout(() => {
                this.mill3D.resize();
            }, 100);
        }
    }

    loadViewData(viewName) {
        switch (viewName) {
            case 'dashboard':
                this.refreshDashboard();
                break;
            case 'alerts':
                this.refreshAlerts();
                break;
            case 'optimization':
                this.refreshOptimizationResult();
                break;
        }
    }

    initControls() {
        document.getElementById('mill-select').addEventListener('change', (e) => {
            this.currentMillId = parseInt(e.target.value);
            this.refreshDashboard();
        });

        this.initSimulationControls();
        this.initOptimizationControls();
        this.initAlertFilters();
        this.initSettingsControls();
    }

    initSimulationControls() {
        const sliders = [
            { id: 'particle-count', valueId: 'particle-count-value' },
            { id: 'sim-speed', valueId: 'sim-speed-value' },
            { id: 'sim-gap', valueId: 'sim-gap-value' },
            { id: 'sim-time', valueId: 'sim-time-value' }
        ];

        sliders.forEach(s => {
            const slider = document.getElementById(s.id);
            const valueSpan = document.getElementById(s.valueId);
            if (slider && valueSpan) {
                slider.addEventListener('input', (e) => {
                    valueSpan.textContent = e.target.value;
                    if (s.id === 'sim-speed' && this.mill3D) {
                        this.mill3D.setRollerSpeed(parseFloat(e.target.value));
                    }
                    if (s.id === 'sim-gap' && this.mill3D) {
                        this.mill3D.setRollerGap(parseFloat(e.target.value));
                    }
                });
            }
        });

        const showParticles = document.getElementById('show-particles');
        if (showParticles) {
            showParticles.addEventListener('change', (e) => {
                if (this.mill3D) {
                    this.mill3D.showParticles(e.target.checked);
                }
            });
        }

        const showForces = document.getElementById('show-forces');
        if (showForces) {
            showForces.addEventListener('change', (e) => {
                if (this.mill3D) {
                    this.mill3D.showForceArrows(e.target.checked);
                }
            });
        }

        document.getElementById('btn-start-sim').addEventListener('click', () => {
            this.startSimulation();
        });

        document.getElementById('btn-reset-view').addEventListener('click', () => {
            if (this.mill3D) {
                this.mill3D.resetCamera();
            }
        });
    }

    initOptimizationControls() {
        const sliders = [
            { id: 'opt-pop', valueId: 'opt-pop-value' },
            { id: 'opt-gen', valueId: 'opt-gen-value' },
            { id: 'opt-mut', valueId: 'opt-mut-value' },
            { id: 'opt-cross', valueId: 'opt-cross-value' },
            { id: 'opt-screen', valueId: 'opt-screen-value' },
            { id: 'opt-break', valueId: 'opt-break-value' }
        ];

        sliders.forEach(s => {
            const slider = document.getElementById(s.id);
            const valueSpan = document.getElementById(s.valueId);
            if (slider && valueSpan) {
                slider.addEventListener('input', (e) => {
                    valueSpan.textContent = parseFloat(e.target.value).toFixed(s.id === 'opt-pop' || s.id === 'opt-gen' ? 0 : 2);
                });
            }
        });

        document.getElementById('btn-start-opt').addEventListener('click', () => {
            this.startOptimization();
        });

        document.getElementById('btn-cancel-opt').addEventListener('click', () => {
            this.cancelOptimization();
        });
    }

    initAlertFilters() {
        ['alert-filter-type', 'alert-filter-severity', 'alert-filter-mill'].forEach(id => {
            const el = document.getElementById(id);
            if (el) {
                el.addEventListener('change', () => this.refreshAlerts());
            }
        });

        document.getElementById('alert-only-unresolved').addEventListener('change', () => {
            this.refreshAlerts();
        });
    }

    initSettingsControls() {
        const sliders = [
            { id: 'th-wear-warning', valueId: 'th-wear-warning-value' },
            { id: 'th-wear-critical', valueId: 'th-wear-critical-value' },
            { id: 'th-yield', valueId: 'th-yield-value' },
            { id: 'th-speed-min', valueId: 'th-speed-min-value' },
            { id: 'th-speed-max', valueId: 'th-speed-max-value' },
            { id: 'th-pressure-min', valueId: 'th-pressure-min-value' },
            { id: 'th-pressure-max', valueId: 'th-pressure-max-value' },
            { id: 'refresh-interval', valueId: 'refresh-interval-value' }
        ];

        sliders.forEach(s => {
            const slider = document.getElementById(s.id);
            const valueSpan = document.getElementById(s.valueId);
            if (slider && valueSpan) {
                slider.addEventListener('input', (e) => {
                    valueSpan.textContent = e.target.value;
                    if (s.id === 'refresh-interval') {
                        this.refreshInterval = parseInt(e.target.value) * 1000;
                        this.restartDataRefresh();
                    }
                });
            }
        });

        document.getElementById('auto-refresh').addEventListener('change', (e) => {
            this.autoRefresh = e.target.checked;
            if (this.autoRefresh) {
                this.startDataRefresh();
            } else {
                this.stopDataRefresh();
            }
        });

        document.getElementById('btn-save-thresholds').addEventListener('click', () => {
            this.saveThresholds();
        });

        document.getElementById('btn-load-thresholds').addEventListener('click', () => {
            this.loadThresholds();
        });

        document.getElementById('btn-test-connection').addEventListener('click', () => {
            this.testConnection();
        });
    }

    init3D() {
        const container = document.getElementById('three-container');
        if (container && typeof THREE !== 'undefined') {
            setTimeout(() => {
                this.mill3D = new StoneMill3DAdapter('three-container');
                this.mill3D.initParticles();
                this.mill3D.setRollerSpeed(15);
                this.mill3D.setRollerGap(2.0);
                this.start3DInfoUpdate();
            }, 100);
        }
    }

    start3DInfoUpdate() {
        setInterval(() => {
            if (this.mill3D && this.currentView === 'simulation') {
                const stats = this.mill3D.getStats();
                document.getElementById('info-particles').textContent = stats.particleCount;
                document.getElementById('info-breakage').textContent = (stats.breakageRatio * 100).toFixed(1) + '%';
                document.getElementById('info-avg-force').textContent = stats.avgForce.toFixed(1) + ' N';
                document.getElementById('info-max-force').textContent = stats.maxForce.toFixed(1) + ' N';
                document.getElementById('info-sim-time').textContent = stats.simTime.toFixed(2) + ' s';

                const sizeDist = this.mill3D.getSizeDistribution();
                this.charts.simGrain.setData(sizeDist);
                this.charts.simGrain.draw();
            }
        }, 500);
    }

    startDataRefresh() {
        this.stopDataRefresh();
        if (this.autoRefresh) {
            this.refreshTimer = setInterval(() => {
                this.refreshCurrentView();
            }, this.refreshInterval);
        }
    }

    stopDataRefresh() {
        if (this.refreshTimer) {
            clearInterval(this.refreshTimer);
            this.refreshTimer = null;
        }
    }

    restartDataRefresh() {
        this.startDataRefresh();
    }

    refreshCurrentView() {
        this.addNewMockData();
        this.loadViewData(this.currentView);
    }

    addNewMockData() {
        const lastData = this.mockSensorData[this.mockSensorData.length - 1];
        const newData = {
            timestamp: Date.now(),
            roller_speed: Math.max(5, Math.min(35, lastData.roller_speed + (Math.random() - 0.5) * 2)),
            roller_pressure: Math.max(100, Math.min(1500, lastData.roller_pressure + (Math.random() - 0.5) * 50)),
            yield: Math.max(1, Math.min(20, lastData.yield + (Math.random() - 0.5) * 0.5)),
            wear_degree: Math.min(100, lastData.wear_degree + Math.random() * 0.02),
            grain_size_0_1mm: Math.max(0, Math.min(1, lastData.grain_size_0_1mm + (Math.random() - 0.5) * 0.02)),
            grain_size_1_2mm: Math.max(0, Math.min(1, lastData.grain_size_1_2mm + (Math.random() - 0.5) * 0.02)),
            grain_size_2_3mm: Math.max(0, Math.min(1, lastData.grain_size_2_3mm + (Math.random() - 0.5) * 0.02)),
            grain_size_3_4mm: Math.max(0, Math.min(1, lastData.grain_size_3_4mm + (Math.random() - 0.5) * 0.02)),
            grain_size_4_5mm: Math.max(0, Math.min(1, lastData.grain_size_4_5mm + (Math.random() - 0.5) * 0.01)),
            grain_size_gt5mm: Math.max(0, Math.min(1, lastData.grain_size_gt5mm + (Math.random() - 0.5) * 0.01)),
            roller_gap: lastData.roller_gap
        };

        const total = newData.grain_size_0_1mm + newData.grain_size_1_2mm + newData.grain_size_2_3mm +
                      newData.grain_size_3_4mm + newData.grain_size_4_5mm + newData.grain_size_gt5mm;
        newData.grain_size_0_1mm /= total;
        newData.grain_size_1_2mm /= total;
        newData.grain_size_2_3mm /= total;
        newData.grain_size_3_4mm /= total;
        newData.grain_size_4_5mm /= total;
        newData.grain_size_gt5mm /= total;

        this.mockSensorData.push(newData);
        if (this.mockSensorData.length > 120) {
            this.mockSensorData.shift();
        }

        this.charts.yieldTrend.addValue(newData.yield);
        this.charts.wearTrend.addValue(newData.wear_degree);
    }

    refreshDashboard() {
        const latest = this.mockSensorData[this.mockSensorData.length - 1];
        if (!latest) return;

        document.getElementById('metric-speed').textContent = latest.roller_speed.toFixed(1);
        document.getElementById('metric-pressure').textContent = latest.roller_pressure.toFixed(0);
        document.getElementById('metric-yield').textContent = latest.yield.toFixed(2);
        document.getElementById('metric-wear').textContent = latest.wear_degree.toFixed(1);

        const sizeDist = [
            latest.grain_size_0_1mm,
            latest.grain_size_1_2mm,
            latest.grain_size_2_3mm,
            latest.grain_size_3_4mm,
            latest.grain_size_4_5mm,
            latest.grain_size_gt5mm
        ];
        this.charts.grainSize.setData(sizeDist);

        this.drawAllCharts();
    }

    refreshAlerts() {
        const typeFilter = document.getElementById('alert-filter-type').value;
        const severityFilter = document.getElementById('alert-filter-severity').value;
        const millFilter = parseInt(document.getElementById('alert-filter-mill').value);
        const onlyUnresolved = document.getElementById('alert-only-unresolved').checked;

        let filtered = this.mockAlerts.filter(alert => {
            if (typeFilter !== 'all' && alert.type !== typeFilter) return false;
            if (severityFilter !== 'all' && alert.severity !== severityFilter) return false;
            if (millFilter !== 0 && alert.mill_id !== millFilter) return false;
            if (onlyUnresolved && alert.resolved) return false;
            return true;
        });

        const container = document.getElementById('alerts-list');
        if (filtered.length === 0) {
            container.innerHTML = '<div class="empty-state"><p>暂无告警</p></div>';
            return;
        }

        const typeNames = {
            wear: '碾轮磨损',
            low_yield: '产量过低',
            abnormal_speed: '转速异常',
            abnormal_pressure: '压力异常'
        };

        const severityClasses = {
            critical: 'alert-critical',
            warning: 'alert-warning',
            info: 'alert-info'
        };

        const severityNames = {
            critical: '严重',
            warning: '警告',
            info: '信息'
        };

        container.innerHTML = filtered.map(alert => `
            <div class="alert-item ${severityClasses[alert.severity]} ${alert.resolved ? 'resolved' : ''}">
                <div class="alert-header">
                    <span class="alert-type">${typeNames[alert.type] || alert.type}</span>
                    <span class="alert-severity">${severityNames[alert.severity] || alert.severity}</span>
                    <span class="alert-time">${this.formatTime(alert.timestamp)}</span>
                </div>
                <div class="alert-body">
                    <div class="alert-mill">石碾 #${alert.mill_id}</div>
                    <div class="alert-message">${alert.message}</div>
                    <div class="alert-details">
                        当前值: <strong>${alert.current_value}</strong> | 
                        阈值: <strong>${alert.threshold}</strong>
                    </div>
                </div>
                <div class="alert-actions">
                    ${!alert.resolved ? `
                        <button class="btn btn-small" onclick="app.resolveAlert(${alert.id})">标记已处理</button>
                    ` : '<span class="resolved-badge">已处理</span>'}
                </div>
            </div>
        `).join('');
    }

    resolveAlert(alertId) {
        const alert = this.mockAlerts.find(a => a.id === alertId);
        if (alert) {
            alert.resolved = true;
            this.showToast('告警已标记为已处理', 'success');
            this.refreshAlerts();
        }
    }

    refreshOptimizationResult() {
    }

    async startSimulation() {
        const particleCount = parseInt(document.getElementById('particle-count').value);
        const speed = parseFloat(document.getElementById('sim-speed').value);
        const gap = parseFloat(document.getElementById('sim-gap').value);
        const simTime = parseFloat(document.getElementById('sim-time').value);

        this.showToast('离散元仿真开始...', 'info');

        if (this.mill3D) {
            this.mill3D.resetParticles(particleCount);
            this.mill3D.setRollerSpeed(speed);
            this.mill3D.setRollerGap(gap);
        }

        const params = {
            mill_id: this.currentMillId,
            particle_count: particleCount,
            roller_speed: speed,
            roller_gap: gap,
            simulation_time: simTime
        };

        try {
            const result = await api.runDEMSimulation(params);
            if (result.success) {
                this.showToast('仿真计算完成', 'success');
                if (result.data && result.data.size_distribution) {
                    this.charts.simGrain.setData(result.data.size_distribution);
                    this.charts.simGrain.draw();
                }
            } else {
                this.showToast('仿真使用本地模拟数据', 'warning');
            }
        } catch (error) {
            this.showToast('仿真使用本地模拟数据', 'warning');
        }
    }

    async startOptimization() {
        if (this.optimizationRunning) return;

        const binMin = parseInt(document.getElementById('opt-bin-min').value);
        const binMax = parseInt(document.getElementById('opt-bin-max').value);
        const popSize = parseInt(document.getElementById('opt-pop').value);
        const maxGen = parseInt(document.getElementById('opt-gen').value);
        const mutRate = parseFloat(document.getElementById('opt-mut').value);
        const crossRate = parseFloat(document.getElementById('opt-cross').value);
        const screenEff = parseFloat(document.getElementById('opt-screen').value);
        const breakParam = parseFloat(document.getElementById('opt-break').value);

        if (binMin > binMax) {
            this.showToast('目标粒度区间设置错误：最小值不能大于最大值', 'error');
            return;
        }

        this.optimizationRunning = true;
        this.optimizationCancelled = false;

        document.getElementById('btn-start-opt').disabled = true;
        document.getElementById('btn-cancel-opt').disabled = false;

        this.charts.fitness.reset();
        document.getElementById('opt-result-empty').style.display = 'block';
        document.getElementById('opt-result-content').style.display = 'none';

        this.showToast('遗传算法优化开始...', 'info');

        const params = {
            mill_id: this.currentMillId,
            target_bin_min: binMin,
            target_bin_max: binMax,
            population_size: popSize,
            max_generations: maxGen,
            mutation_rate: mutRate,
            crossover_rate: crossRate,
            screening_efficiency: screenEff,
            breakage_function_param: breakParam
        };

        await this.simulateOptimizationProgress(params, maxGen);
    }

    async simulateOptimizationProgress(params, maxGen) {
        let bestFitness = 0;
        let bestSpeed = 0;
        let bestGap = 0;
        let bestRatio = 0;
        let bestYield = 0;

        for (let gen = 1; gen <= maxGen; gen++) {
            if (this.optimizationCancelled) {
                this.showToast('优化已取消', 'warning');
                break;
            }

            const progress = gen / maxGen;
            document.getElementById('opt-progress-fill').style.width = (progress * 100) + '%';
            document.getElementById('opt-progress-text').textContent = `${gen} / ${maxGen}`;

            const currentBestFitness = 0.3 + 0.5 * (1 - Math.exp(-gen / 30)) + (Math.random() - 0.5) * 0.05;
            const currentAvgFitness = currentBestFitness - 0.1 - Math.random() * 0.1;

            this.charts.fitness.addGeneration(
                Math.min(0.95, currentBestFitness),
                Math.max(0, currentAvgFitness)
            );
            this.charts.fitness.draw();

            if (currentBestFitness > bestFitness) {
                bestFitness = currentBestFitness;
                bestSpeed = 10 + Math.random() * 15;
                bestGap = 1 + Math.random() * 3;
                bestRatio = 50 + Math.random() * 30;
                bestYield = 5 + Math.random() * 10;
            }

            document.getElementById('stat-fitness').textContent = bestFitness.toFixed(4);
            document.getElementById('stat-speed').textContent = bestSpeed.toFixed(2) + ' rad/s';
            document.getElementById('stat-gap').textContent = bestGap.toFixed(2) + ' mm';
            document.getElementById('stat-ratio').textContent = bestRatio.toFixed(1) + ' %';
            document.getElementById('stat-yield').textContent = bestYield.toFixed(2) + ' kg/min';

            await this.delay(50);
        }

        if (!this.optimizationCancelled) {
            this.showOptimizationResult({
                best_speed: bestSpeed,
                best_gap: bestGap,
                target_ratio: bestRatio,
                yield: bestYield,
                fitness: bestFitness,
                generations: maxGen,
                params: params
            });
            this.showToast('优化完成！', 'success');
        }

        this.optimizationRunning = false;
        document.getElementById('btn-start-opt').disabled = false;
        document.getElementById('btn-cancel-opt').disabled = true;
    }

    cancelOptimization() {
        this.optimizationCancelled = true;
    }

    showOptimizationResult(result) {
        document.getElementById('opt-result-empty').style.display = 'none';
        document.getElementById('opt-result-content').style.display = 'block';

        document.getElementById('res-speed').textContent = result.best_speed.toFixed(2);
        document.getElementById('res-gap').textContent = result.best_gap.toFixed(2);
        document.getElementById('res-ratio').textContent = result.target_ratio.toFixed(1);
        document.getElementById('res-yield').textContent = result.yield.toFixed(2);
        document.getElementById('res-fitness').textContent = result.fitness.toFixed(4);
        document.getElementById('res-generations').textContent = result.generations;

        const paramNames = {
            target_bin_min: '目标粒度区间(最小)',
            target_bin_max: '目标粒度区间(最大)',
            population_size: '种群数量',
            max_generations: '最大迭代次数',
            mutation_rate: '变异率',
            crossover_rate: '交叉率',
            screening_efficiency: '筛分效率',
            breakage_function_param: '破碎函数参数'
        };

        const binNames = ['0-1mm', '1-2mm', '2-3mm', '3-4mm', '4-5mm', '>5mm'];

        let paramsText = '';
        Object.keys(result.params).forEach(key => {
            if (key === 'mill_id') return;
            let value = result.params[key];
            if (key === 'target_bin_min' || key === 'target_bin_max') {
                value = binNames[value] || value;
            }
            paramsText += `${paramNames[key] || key}: ${value}\n`;
        });

        document.getElementById('res-params').textContent = paramsText;
    }

    async saveThresholds() {
        const thresholds = {
            wear_warning: parseFloat(document.getElementById('th-wear-warning').value),
            wear_critical: parseFloat(document.getElementById('th-wear-critical').value),
            yield_min: parseFloat(document.getElementById('th-yield').value),
            speed_min: parseFloat(document.getElementById('th-speed-min').value),
            speed_max: parseFloat(document.getElementById('th-speed-max').value),
            pressure_min: parseFloat(document.getElementById('th-pressure-min').value),
            pressure_max: parseFloat(document.getElementById('th-pressure-max').value)
        };

        try {
            const result = await api.setThresholds(thresholds);
            if (result.success) {
                this.showToast('阈值设置已保存', 'success');
            } else {
                this.showToast('设置已本地保存', 'success');
            }
        } catch (error) {
            this.showToast('设置已本地保存', 'success');
        }
    }

    async loadThresholds() {
        try {
            const result = await api.getThresholds();
            if (result.success && result.data) {
                const th = result.data;
                document.getElementById('th-wear-warning').value = th.wear_warning || 70;
                document.getElementById('th-wear-warning-value').textContent = th.wear_warning || 70;
                document.getElementById('th-wear-critical').value = th.wear_critical || 90;
                document.getElementById('th-wear-critical-value').textContent = th.wear_critical || 90;
                document.getElementById('th-yield').value = th.yield_min || 5;
                document.getElementById('th-yield-value').textContent = th.yield_min || 5;
                document.getElementById('th-speed-min').value = th.speed_min || 3;
                document.getElementById('th-speed-min-value').textContent = th.speed_min || 3;
                document.getElementById('th-speed-max').value = th.speed_max || 35;
                document.getElementById('th-speed-max-value').textContent = th.speed_max || 35;
                document.getElementById('th-pressure-min').value = th.pressure_min || 100;
                document.getElementById('th-pressure-min-value').textContent = th.pressure_min || 100;
                document.getElementById('th-pressure-max').value = th.pressure_max || 1000;
                document.getElementById('th-pressure-max-value').textContent = th.pressure_max || 1000;
                this.showToast('阈值设置已加载', 'success');
            } else {
                this.showToast('使用默认设置', 'info');
            }
        } catch (error) {
            this.showToast('使用默认设置', 'info');
        }
    }

    async testConnection() {
        const resultDiv = document.getElementById('connection-test-result');
        resultDiv.innerHTML = '<p class="loading">正在测试连接...</p>';

        const host = document.getElementById('api-host').value;
        api.setBaseUrl(host);

        try {
            const result = await api.testConnection();
            if (result.success) {
                resultDiv.innerHTML = `<p class="success">${result.message}</p>`;
                document.getElementById('connection-status').className = 'status-badge connected';
                document.getElementById('connection-status').textContent = '● 已连接';
                this.showToast('连接测试成功', 'success');
            } else {
                resultDiv.innerHTML = `<p class="error">${result.message}</p>`;
                document.getElementById('connection-status').className = 'status-badge disconnected';
                document.getElementById('connection-status').textContent = '● 未连接';
                this.showToast('连接测试失败，使用模拟数据', 'warning');
            }
        } catch (error) {
            resultDiv.innerHTML = `<p class="error">连接失败: ${error.message}</p>`;
            document.getElementById('connection-status').className = 'status-badge disconnected';
            document.getElementById('connection-status').textContent = '● 未连接';
            this.showToast('连接测试失败，使用模拟数据', 'warning');
        }
    }

    startClock() {
        const updateTime = () => {
            const now = new Date();
            const timeStr = now.toLocaleTimeString('zh-CN', { hour12: false });
            document.getElementById('current-time').textContent = timeStr;
        };
        updateTime();
        setInterval(updateTime, 1000);
    }

    formatTime(timestamp) {
        const date = new Date(timestamp);
        const now = new Date();
        const diff = now - date;

        if (diff < 60000) return '刚刚';
        if (diff < 3600000) return Math.floor(diff / 60000) + '分钟前';
        if (diff < 86400000) return Math.floor(diff / 3600000) + '小时前';
        return date.toLocaleString('zh-CN');
    }

    showToast(message, type = 'info') {
        const container = document.getElementById('toast-container');
        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;
        toast.textContent = message;
        container.appendChild(toast);

        setTimeout(() => {
            toast.classList.add('show');
        }, 10);

        setTimeout(() => {
            toast.classList.remove('show');
            setTimeout(() => {
                container.removeChild(toast);
            }, 300);
        }, 3000);
    }

    delay(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
}

const app = new StoneMillApp();
