class ParticlePanel {
    constructor(stoneRoller3D, uiContainerId) {
        this.roller3d = stoneRoller3D;
        this.uiContainer = document.getElementById(uiContainerId);

        this.grainInstanced = null;
        this.particleData = [];
        this.forceArrows = [];

        this.showParticles = true;
        this.showForces = false;
        this.rollerGap = 2.0;
        this.rollerSpeed = 0;
        this.isSimulating = false;

        this.isMobile = stoneRoller3D ? stoneRoller3D.isMobile : false;
        this.maxParticles = this.isMobile ? 500 : 2000;
        this.dummy = new THREE.Object3D();
        this.colorTemp = new THREE.Color();

        this.initUI();
        this.createGrainParticles(200);
        if (stoneRoller3D) {
            stoneRoller3D.addAnimationCallback((angle) => this.updateParticles(angle));
        }
    }

    initUI() {
        if (!this.uiContainer) return;

        this.uiContainer.innerHTML = `
            <div class="particle-panel">
                <h3>谷物粒子控制</h3>
                <div class="control-row">
                    <label>粒子数量: <span id="particleCount">200</span></label>
                    <input type="range" id="particleSlider" min="50" max="${this.maxParticles}" value="200" step="50"/>
                </div>
                <div class="control-row">
                    <label>碾轮间隙: <span id="gapValue">2.0</span> cm</label>
                    <input type="range" id="gapSlider" min="0.5" max="5.0" value="2.0" step="0.1"/>
                </div>
                <div class="control-row">
                    <label>碾轮转速: <span id="speedValue">0</span> rad/s</label>
                    <input type="range" id="speedSlider" min="0" max="30" value="0" step="0.5"/>
                </div>
                <div class="control-row checks">
                    <label><input type="checkbox" id="showParticles" checked/> 显示谷物</label>
                    <label><input type="checkbox" id="showForces"/> 显示受力</label>
                </div>
                <div class="control-row buttons">
                    <button id="generateBtn">重新生成</button>
                    <button id="simBtn">开始仿真</button>
                </div>
                <div class="particle-stats">
                    <div>活跃粒子: <span id="activeCount">0</span></div>
                    <div>破碎速率: <span id="breakRate">0</span></div>
                </div>
            </div>
        `;

        const slider = document.getElementById('particleSlider');
        slider.addEventListener('input', (e) => {
            const count = parseInt(e.target.value);
            document.getElementById('particleCount').textContent = count;
            this.createGrainParticles(count);
        });

        const gapSlider = document.getElementById('gapSlider');
        gapSlider.addEventListener('input', (e) => {
            this.rollerGap = parseFloat(e.target.value);
            document.getElementById('gapValue').textContent = this.rollerGap.toFixed(1);
        });

        const speedSlider = document.getElementById('speedSlider');
        speedSlider.addEventListener('input', (e) => {
            this.rollerSpeed = parseFloat(e.target.value);
            document.getElementById('speedValue').textContent = this.rollerSpeed.toFixed(1);
            if (this.roller3d) this.roller3d.setRollerSpeed(this.rollerSpeed);
        });

        document.getElementById('showParticles').addEventListener('change', (e) => {
            this.showParticles = e.target.checked;
            if (this.grainInstanced) this.grainInstanced.visible = this.showParticles;
        });

        document.getElementById('showForces').addEventListener('change', (e) => {
            this.showForces = e.target.checked;
            for (const a of this.forceArrows) a.visible = this.showForces;
        });

        document.getElementById('generateBtn').addEventListener('click', () => {
            this.createGrainParticles(parseInt(slider.value));
        });

        document.getElementById('simBtn').addEventListener('click', (e) => {
            this.isSimulating = !this.isSimulating;
            e.target.textContent = this.isSimulating ? '停止仿真' : '开始仿真';
        });
    }

    createGrainParticles(count = 200) {
        if (!this.roller3d || !this.roller3d.scene) return;
        if (this.grainInstanced) {
            this.roller3d.removeFromScene(this.grainInstanced);
            this.grainInstanced.geometry.dispose();
            this.grainInstanced.material.dispose();
            this.grainInstanced = null;
        }
        for (const a of this.forceArrows) {
            this.roller3d.removeFromScene(a);
        }
        this.forceArrows = [];

        const actualCount = Math.min(count, this.maxParticles);
        const segments = this.isMobile ? 6 : 8;
        const geometry = new THREE.SphereGeometry(0.03, segments, segments);
        const material = new THREE.MeshStandardMaterial({
            color: 0xd4a574,
            roughness: 0.7,
            metalness: 0.1,
            vertexColors: false
        });

        this.grainInstanced = new THREE.InstancedMesh(geometry, material, actualCount);
        this.grainInstanced.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
        this.grainInstanced.castShadow = !this.isMobile;
        this.grainInstanced.receiveShadow = !this.isMobile;
        this.roller3d.addToScene(this.grainInstanced);

        this.particleData = [];
        for (let i = 0; i < actualCount; i++) {
            const angle = Math.random() * Math.PI * 2;
            const radius = 0.3 + Math.random() * 1.5;
            const particle = {
                x: Math.cos(angle) * radius,
                y: 0.05 + Math.random() * 0.1,
                z: Math.sin(angle) * radius,
                vx: 0, vy: 0, vz: 0,
                radius: 0.02 + Math.random() * 0.03,
                broken: false,
                color: new THREE.Color().setHSL(0.08 + Math.random() * 0.05, 0.6, 0.5 + Math.random() * 0.2),
                age: 0
            };
            this.particleData.push(particle);
            this.updateInstanceMatrix(i, particle);
            this.grainInstanced.setColorAt(i, particle.color);
        }

        this.grainInstanced.instanceColor.needsUpdate = true;
        this.grainInstanced.instanceMatrix.needsUpdate = true;
        this.grainInstanced.count = actualCount;
        this.grainInstanced.visible = this.showParticles;

        this.updateStats();
    }

    updateInstanceMatrix(index, p) {
        this.dummy.position.set(p.x, p.y, p.z);
        const scale = p.radius / 0.03;
        this.dummy.scale.set(scale, scale, scale);
        this.dummy.rotation.set(Math.random(), Math.random(), Math.random());
        this.dummy.updateMatrix();
        this.grainInstanced.setMatrixAt(index, this.dummy.matrix);
    }

    updateParticles(rotationAngle) {
        if (!this.grainInstanced) return;
        const dt = 0.016;
        let brokenCount = 0;

        for (let i = 0; i < this.particleData.length; i++) {
            const p = this.particleData[i];
            if (p.broken) continue;
            p.age += dt;

            if (this.isSimulating && this.rollerSpeed > 0) {
                p.vy -= 9.8 * dt * 0.05;
                const dist = Math.sqrt(p.x * p.x + p.z * p.z);
                if (dist < 0.5 + this.rollerGap * 0.1) {
                    const rollerInfluence = Math.max(0, 1 - dist / (0.5 + this.rollerGap * 0.1));
                    p.vx += -p.z * this.rollerSpeed * rollerInfluence * dt * 0.5;
                    p.vz += p.x * this.rollerSpeed * rollerInfluence * dt * 0.5;
                    if (Math.random() < 0.001 * rollerInfluence * this.rollerSpeed) {
                        this.breakParticle(i);
                        brokenCount++;
                    }
                }
                p.vx *= 0.98; p.vy *= 0.98; p.vz *= 0.98;
                p.x += p.vx * dt;
                p.y += p.vy * dt;
                p.z += p.vz * dt;
            } else {
                p.x += Math.sin(p.age * 2 + i) * 0.0005;
                p.z += Math.cos(p.age * 2 + i * 0.7) * 0.0005;
            }

            if (p.y < 0.02) { p.y = 0.02; p.vy = Math.abs(p.vy) * 0.3; }
            const r = Math.sqrt(p.x * p.x + p.z * p.z);
            if (r > 1.95) {
                const inv = 1.95 / r;
                p.x *= inv; p.z *= inv;
                p.vx *= -0.3; p.vz *= -0.3;
            }
            if (!p.broken) this.updateInstanceMatrix(i, p);
        }

        this.grainInstanced.instanceMatrix.needsUpdate = true;
        if (brokenCount > 0) {
            document.getElementById('breakRate').textContent = brokenCount;
        }
        this.updateStats();
    }

    breakParticle(index) {
        const p = this.particleData[index];
        if (!p || p.broken) return;
        p.broken = true;

        this.dummy.position.set(-1000, -1000, -1000);
        this.dummy.scale.set(0, 0, 0);
        this.dummy.updateMatrix();
        this.grainInstanced.setMatrixAt(index, this.dummy.matrix);

        const color = new THREE.Color(0x8b6914);
        this.grainInstanced.setColorAt(index, color);
        this.grainInstanced.instanceColor.needsUpdate = true;
    }

    updateStats() {
        const activeEl = document.getElementById('activeCount');
        if (!activeEl) return;
        let active = 0;
        for (const p of this.particleData) if (!p.broken) active++;
        activeEl.textContent = active;
    }

    getParticlePositions() {
        return this.particleData
            .filter(p => !p.broken)
            .map(p => ({ x: p.x, y: p.y, z: p.z, radius: p.radius }));
    }

    setParticlePositions(positions) {
        if (!this.grainInstanced) return;
        const n = Math.min(positions.length, this.particleData.length);
        for (let i = 0; i < n; i++) {
            const pos = positions[i];
            const p = this.particleData[i];
            p.x = pos.x; p.y = pos.y; p.z = pos.z;
            if (pos.radius) p.radius = pos.radius;
            p.broken = false;
            this.updateInstanceMatrix(i, p);
        }
        for (let i = n; i < this.particleData.length; i++) {
            this.particleData[i].broken = true;
            this.dummy.position.set(-1000, -1000, -1000);
            this.dummy.scale.set(0, 0, 0);
            this.dummy.updateMatrix();
            this.grainInstanced.setMatrixAt(i, this.dummy.matrix);
        }
        this.grainInstanced.instanceMatrix.needsUpdate = true;
        this.updateStats();
    }

    dispose() {
        if (this.grainInstanced) {
            this.roller3d.removeFromScene(this.grainInstanced);
            this.grainInstanced.geometry.dispose();
            this.grainInstanced.material.dispose();
        }
        for (const a of this.forceArrows) this.roller3d.removeFromScene(a);
    }
}
