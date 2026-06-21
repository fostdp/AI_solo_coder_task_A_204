class StoneMill3D {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
        if (!this.container) return;

        this.scene = null;
        this.camera = null;
        this.renderer = null;
        this.controls = null;
        this.roller = null;
        this.rollerArm = null;
        this.millBase = null;
        this.grainInstanced = null;
        this.forceArrows = [];

        this.particleData = [];
        this.showParticles = true;
        this.showForces = false;
        this.rollerSpeed = 0;
        this.rollerGap = 2.0;
        this.isSimulating = false;
        this.rotationAngle = 0;

        this.isMobile = this.detectMobile();
        this.maxParticles = this.isMobile ? 500 : 2000;
        this.useInstanced = true;

        this.dummy = new THREE.Object3D();
        this.colorTemp = new THREE.Color();

        this.animationId = null;
        this.init();
    }

    detectMobile() {
        const ua = navigator.userAgent;
        return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(ua)
            || window.innerWidth < 768;
    }

    init() {
        const rect = this.container.getBoundingClientRect();
        const width = rect.width;
        const height = rect.height;

        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0x0a0a1a);
        this.scene.fog = new THREE.Fog(0x0a0a1a, 5, 20);

        this.camera = new THREE.PerspectiveCamera(60, width / height, 0.1, 1000);
        this.camera.position.set(4, 3, 4);
        this.camera.lookAt(0, 0, 0);

        this.renderer = new THREE.WebGLRenderer({
            antialias: !this.isMobile,
            alpha: true,
            powerPreference: this.isMobile ? 'low-power' : 'high-performance'
        });
        this.renderer.setSize(width, height);
        this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, this.isMobile ? 1.5 : 2));
        this.renderer.shadowMap.enabled = !this.isMobile;
        this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
        this.container.appendChild(this.renderer.domElement);

        this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
        this.controls.enableDamping = true;
        this.controls.dampingFactor = 0.05;
        this.controls.minDistance = 2;
        this.controls.maxDistance = 15;
        this.controls.maxPolarAngle = Math.PI / 2 - 0.1;

        this.addLights();
        this.createMillModel();
        this.createGround();

        window.addEventListener('resize', () => this.onResize());

        this.animate();
    }

    addLights() {
        const ambientLight = new THREE.AmbientLight(0x404060, 0.5);
        this.scene.add(ambientLight);

        const mainLight = new THREE.DirectionalLight(0xffffff, this.isMobile ? 0.6 : 0.8);
        mainLight.position.set(5, 8, 5);
        mainLight.castShadow = !this.isMobile;
        if (!this.isMobile) {
            mainLight.shadow.mapSize.width = 1024;
            mainLight.shadow.mapSize.height = 1024;
            mainLight.shadow.camera.near = 0.5;
            mainLight.shadow.camera.far = 50;
            mainLight.shadow.camera.left = -10;
            mainLight.shadow.camera.right = 10;
            mainLight.shadow.camera.top = 10;
            mainLight.shadow.camera.bottom = -10;
        }
        this.scene.add(mainLight);

        const fillLight = new THREE.DirectionalLight(0xffa500, 0.3);
        fillLight.position.set(-5, 3, -5);
        this.scene.add(fillLight);

        if (!this.isMobile) {
            const pointLight = new THREE.PointLight(0xff6600, 0.5, 10);
            pointLight.position.set(0, 2, 0);
            this.scene.add(pointLight);
        }
    }

    createMillModel() {
        const baseGroup = new THREE.Group();
        this.millBase = baseGroup;

        const baseGeometry = new THREE.CylinderGeometry(2.1, 2.2, 0.3, this.isMobile ? 32 : 64);
        const stoneMaterial = new THREE.MeshStandardMaterial({
            color: 0x8b7355,
            roughness: 0.9,
            metalness: 0.1
        });
        const base = new THREE.Mesh(baseGeometry, stoneMaterial);
        base.position.y = -0.15;
        base.receiveShadow = !this.isMobile;
        baseGroup.add(base);

        const platformGeometry = new THREE.CylinderGeometry(2.0, 2.0, 0.05, this.isMobile ? 64 : 128);
        const platformMaterial = new THREE.MeshStandardMaterial({
            color: 0x6b5344,
            roughness: 0.8,
            metalness: 0.1
        });
        const platform = new THREE.Mesh(platformGeometry, platformMaterial);
        platform.position.y = 0.025;
        platform.receiveShadow = !this.isMobile;
        baseGroup.add(platform);

        const grooveGeometry = new THREE.TorusGeometry(1.8, 0.02, 8, this.isMobile ? 64 : 128);
        const grooveMaterial = new THREE.MeshStandardMaterial({
            color: 0x3d2817,
            roughness: 0.9,
            metalness: 0.1
        });

        for (let i = 0; i < 3; i++) {
            const groove = new THREE.Mesh(grooveGeometry, grooveMaterial);
            groove.rotation.x = Math.PI / 2;
            groove.position.y = 0.05 + i * 0.005;
            groove.scale.set(0.95 - i * 0.1, 0.95 - i * 0.1, 1);
            baseGroup.add(groove);
        }

        const edgeGeometry = new THREE.TorusGeometry(2.0, 0.08, 16, this.isMobile ? 64 : 128);
        const edgeMaterial = new THREE.MeshStandardMaterial({
            color: 0x4a3728,
            roughness: 0.7,
            metalness: 0.2
        });
        const edge = new THREE.Mesh(edgeGeometry, edgeMaterial);
        edge.rotation.x = Math.PI / 2;
        edge.position.y = 0.05;
        baseGroup.add(edge);

        const centerPoleGeometry = new THREE.CylinderGeometry(0.05, 0.06, 1.5, this.isMobile ? 8 : 16);
        const woodMaterial = new THREE.MeshStandardMaterial({
            color: 0x5c4033,
            roughness: 0.8,
            metalness: 0.1
        });
        const centerPole = new THREE.Mesh(centerPoleGeometry, woodMaterial);
        centerPole.position.y = 0.75;
        centerPole.castShadow = !this.isMobile;
        baseGroup.add(centerPole);

        this.scene.add(baseGroup);
        this.createRoller();
    }

    createRoller() {
        const armGroup = new THREE.Group();
        this.rollerArm = armGroup;

        const armGeometry = new THREE.BoxGeometry(1.5, 0.08, 0.08);
        const woodMaterial = new THREE.MeshStandardMaterial({
            color: 0x6b4423,
            roughness: 0.8,
            metalness: 0.1
        });
        const arm = new THREE.Mesh(armGeometry, woodMaterial);
        arm.position.x = 0.75;
        arm.position.y = 1.2;
        arm.castShadow = !this.isMobile;
        armGroup.add(arm);

        const supportGeometry = new THREE.BoxGeometry(0.1, 1.0, 0.1);
        const support = new THREE.Mesh(supportGeometry, woodMaterial);
        support.position.set(1.5, 0.7, 0);
        support.castShadow = !this.isMobile;
        armGroup.add(support);

        const rollerGroup = new THREE.Group();
        this.roller = rollerGroup;

        const rollerGeometry = new THREE.CylinderGeometry(0.8, 0.8, 0.3, this.isMobile ? 16 : 32);
        const stoneMaterial = new THREE.MeshStandardMaterial({
            color: 0x7a6b5a,
            roughness: 0.85,
            metalness: 0.1
        });
        const rollerMesh = new THREE.Mesh(rollerGeometry, stoneMaterial);
        rollerMesh.rotation.z = Math.PI / 2;
        rollerMesh.castShadow = !this.isMobile;
        rollerMesh.receiveShadow = !this.isMobile;
        rollerGroup.add(rollerMesh);

        const rimGeometry = new THREE.TorusGeometry(0.8, 0.02, 12, this.isMobile ? 32 : 64);
        const rimMaterial = new THREE.MeshStandardMaterial({
            color: 0x5a4a3a,
            roughness: 0.7,
            metalness: 0.2
        });
        const rim1 = new THREE.Mesh(rimGeometry, rimMaterial);
        rim1.rotation.y = Math.PI / 2;
        rim1.position.z = 0.15;
        rollerGroup.add(rim1);

        const rim2 = new THREE.Mesh(rimGeometry, rimMaterial);
        rim2.rotation.y = Math.PI / 2;
        rim2.position.z = -0.15;
        rollerGroup.add(rim2);

        if (!this.isMobile) {
            const patternGeometry = new THREE.CylinderGeometry(0.75, 0.75, 0.31, 32, 1, true);
            const patternMaterial = new THREE.MeshStandardMaterial({
                color: 0x5a4a3a,
                roughness: 0.9,
                metalness: 0.05,
                side: THREE.DoubleSide
            });
            const pattern = new THREE.Mesh(patternGeometry, patternMaterial);
            pattern.rotation.z = Math.PI / 2;
            rollerGroup.add(pattern);
        }

        rollerGroup.position.set(1.5, 0.8, 0);
        armGroup.add(rollerGroup);

        this.scene.add(armGroup);
    }

    createGround() {
        const groundGeometry = new THREE.PlaneGeometry(30, 30);
        const groundMaterial = new THREE.MeshStandardMaterial({
            color: 0x1a1a2e,
            roughness: 1.0,
            metalness: 0.0
        });
        const ground = new THREE.Mesh(groundGeometry, groundMaterial);
        ground.rotation.x = -Math.PI / 2;
        ground.position.y = -0.31;
        ground.receiveShadow = !this.isMobile;
        this.scene.add(ground);

        const gridHelper = new THREE.GridHelper(20, 40, 0x333344, 0x222233);
        gridHelper.position.y = -0.3;
        this.scene.add(gridHelper);
    }

    createGrainParticles(count = 200) {
        const actualCount = Math.min(count, this.maxParticles);

        if (this.grainInstanced) {
            this.scene.remove(this.grainInstanced);
            if (this.grainInstanced.geometry) this.grainInstanced.geometry.dispose();
            if (this.grainInstanced.material) this.grainInstanced.material.dispose();
        }

        this.particleData = [];

        const grainColors = [
            new THREE.Color(0xd4a574),
            new THREE.Color(0xc4956a),
            new THREE.Color(0xb8865a),
            new THREE.Color(0xe8c49c),
            new THREE.Color(0xf0d5a8)
        ];

        const geometry = new THREE.SphereGeometry(0.03, this.isMobile ? 6 : 8, this.isMobile ? 6 : 8);

        const material = new THREE.MeshStandardMaterial({
            vertexColors: false,
            roughness: 0.7,
            metalness: 0.05
        });

        this.grainInstanced = new THREE.InstancedMesh(geometry, material, actualCount);
        this.grainInstanced.castShadow = !this.isMobile;
        this.grainInstanced.receiveShadow = !this.isMobile;
        this.grainInstanced.instanceMatrix.setUsage(THREE.DynamicDrawUsage);

        if (this.grainInstanced.instanceColor) {
            this.grainInstanced.instanceColor.setUsage(THREE.DynamicDrawUsage);
        }

        for (let i = 0; i < actualCount; i++) {
            const angle = Math.random() * Math.PI * 2;
            const radius = 0.3 + Math.random() * 1.4;
            const x = Math.cos(angle) * radius;
            const z = Math.sin(angle) * radius;
            const y = 0.05 + Math.random() * 0.1;

            const size = 0.02 + Math.random() * 0.04;
            const color = grainColors[Math.floor(Math.random() * grainColors.length)];

            this.particleData.push({
                x, y, z,
                vx: 0, vy: 0, vz: 0,
                radius: size,
                mass: size * size * size * 1200,
                originalY: y,
                broken: false,
                colorIndex: Math.floor(Math.random() * grainColors.length),
                color: color.clone()
            });

            this.dummy.position.set(x, y, z);
            this.dummy.scale.set(size / 0.03, size / 0.03, size / 0.03);
            this.dummy.rotation.set(
                Math.random() * Math.PI,
                Math.random() * Math.PI,
                Math.random() * Math.PI
            );
            this.dummy.updateMatrix();
            this.grainInstanced.setMatrixAt(i, this.dummy.matrix);
            this.grainInstanced.setColorAt(i, color);
        }

        this.grainInstanced.count = actualCount;
        this.grainInstanced.instanceMatrix.needsUpdate = true;
        if (this.grainInstanced.instanceColor) {
            this.grainInstanced.instanceColor.needsUpdate = true;
        }

        this.scene.add(this.grainInstanced);
    }

    updateParticlePositions() {
        if (!this.grainInstanced || !this.showParticles) return;

        const rollerX = 1.5 * Math.cos(this.rotationAngle);
        const rollerZ = 1.5 * Math.sin(this.rotationAngle);
        const rollerY = this.rollerGap / 1000 + 0.8;

        for (let i = 0; i < this.particleData.length; i++) {
            const p = this.particleData[i];
            if (p.broken) continue;

            const dx = p.x - rollerX;
            const dz = p.z - rollerZ;
            const dist = Math.sqrt(dx * dx + dz * dz);

            if (dist < 0.9 && Math.abs(p.y - rollerY) < 0.8) {
                const force = (0.9 - dist) * 50;
                const nx = dx / dist;
                const nz = dz / dist;

                p.vx += nx * force * 0.001;
                p.vz += nz * force * 0.001;

                const tangentX = -nz;
                const tangentZ = nx;
                const rollerVel = this.rollerSpeed * 0.8;
                p.vx += tangentX * rollerVel * 0.1;
                p.vz += tangentZ * rollerVel * 0.1;

                if (this.showForces && Math.random() < 0.1) {
                    this.addForceArrow(p.x, p.y, p.z, nx * force, nz * force);
                }

                if (force > 30 && Math.random() < 0.02) {
                    this.breakParticle(i);
                }
            }

            p.vy -= 9.8 * 0.001;

            p.vx *= 0.98;
            p.vz *= 0.98;
            p.vy *= 0.98;

            p.x += p.vx;
            p.y += p.vy;
            p.z += p.vz;

            const distFromCenter = Math.sqrt(p.x * p.x + p.z * p.z);
            if (distFromCenter > 1.95) {
                const nx = p.x / distFromCenter;
                const nz = p.z / distFromCenter;
                p.x = nx * 1.95;
                p.z = nz * 1.95;
                p.vx *= -0.5;
                p.vz *= -0.5;
            }

            if (p.y < 0.05) {
                p.y = 0.05;
                p.vy *= -0.3;
                p.vx *= 0.9;
                p.vz *= 0.9;
            }

            const scale = p.radius / 0.03;
            this.dummy.position.set(p.x, p.y, p.z);
            this.dummy.scale.set(scale, scale, scale);
            this.dummy.rotation.x += p.vx * 0.1;
            this.dummy.rotation.z += p.vz * 0.1;
            this.dummy.updateMatrix();
            this.grainInstanced.setMatrixAt(i, this.dummy.matrix);
        }

        this.grainInstanced.instanceMatrix.needsUpdate = true;
    }

    breakParticle(index) {
        const p = this.particleData[index];
        p.broken = true;

        this.grainInstanced.setColorAt(index, new THREE.Color(0.5, 0.5, 0.5));

        this.dummy.position.set(p.x, -10, p.z);
        this.dummy.updateMatrix();
        this.grainInstanced.setMatrixAt(index, this.dummy.matrix);

        if (this.particleData.length < this.maxParticles) {
            for (let i = 0; i < 2 && this.particleData.length < this.maxParticles; i++) {
                const newIdx = this.particleData.length;
                const angle = Math.random() * Math.PI * 2;
                const newP = {
                    x: p.x + Math.cos(angle) * 0.02,
                    y: p.y + 0.02,
                    z: p.z + Math.sin(angle) * 0.02,
                    vx: p.vx + (Math.random() - 0.5) * 0.1,
                    vy: p.vy + Math.random() * 0.05,
                    vz: p.vz + (Math.random() - 0.5) * 0.1,
                    radius: p.radius * 0.6,
                    mass: p.mass * 0.3,
                    originalY: 0.05,
                    broken: false,
                    colorIndex: p.colorIndex,
                    color: p.color.clone()
                };

                this.particleData.push(newP);

                const scale = newP.radius / 0.03;
                this.dummy.position.set(newP.x, newP.y, newP.z);
                this.dummy.scale.set(scale, scale, scale);
                this.dummy.rotation.set(
                    Math.random() * Math.PI,
                    Math.random() * Math.PI,
                    Math.random() * Math.PI
                );
                this.dummy.updateMatrix();
                this.grainInstanced.setMatrixAt(newIdx, this.dummy.matrix);
                this.grainInstanced.setColorAt(newIdx, newP.color);
            }

            this.grainInstanced.count = this.particleData.length;
            this.grainInstanced.instanceMatrix.needsUpdate = true;
            if (this.grainInstanced.instanceColor) {
                this.grainInstanced.instanceColor.needsUpdate = true;
            }
        }
    }

    addForceArrow(x, y, z, fx, fz) {
        const dir = new THREE.Vector3(fx, 0, fz).normalize();
        const length = Math.min(Math.sqrt(fx * fx + fz * fz) * 0.01, 0.5);
        const arrow = new THREE.ArrowHelper(
            dir,
            new THREE.Vector3(x, y, z),
            length,
            0xff0000,
            0.05,
            0.03
        );
        this.scene.add(arrow);
        this.forceArrows.push({ arrow, time: Date.now() });
    }

    updateForceArrows() {
        const now = Date.now();
        this.forceArrows = this.forceArrows.filter(item => {
            if (now - item.time > 200) {
                this.scene.remove(item.arrow);
                return false;
            }
            const opacity = 1 - (now - item.time) / 200;
            item.arrow.setColor(new THREE.Color(1, 0, 0, opacity));
            return true;
        });
    }

    setRollerSpeed(speed) {
        this.rollerSpeed = speed;
    }

    setRollerGap(gap) {
        this.rollerGap = gap;
        if (this.roller) {
            this.roller.position.y = gap / 1000 + 0.8;
        }
    }

    showParticles(show) {
        this.showParticles = show;
        if (this.grainInstanced) {
            this.grainInstanced.visible = show;
        }
    }

    showForceArrows(show) {
        this.showForces = show;
        if (!show) {
            this.forceArrows.forEach(item => this.scene.remove(item.arrow));
            this.forceArrows = [];
        }
    }

    startSimulation(particleCount = 200) {
        this.createGrainParticles(particleCount);
        this.isSimulating = true;
    }

    stopSimulation() {
        this.isSimulating = false;
    }

    resetCamera() {
        this.camera.position.set(4, 3, 4);
        this.camera.lookAt(0, 0, 0);
        this.controls.reset();
    }

    getBreakageRate() {
        if (this.particleData.length === 0) return 0;
        const broken = this.particleData.filter(p => p.broken).length;
        return broken / this.particleData.length;
    }

    getSizeDistribution() {
        const bins = [0, 0, 0, 0, 0, 0];
        let total = 0;

        for (const p of this.particleData) {
            if (p.broken) continue;
            const diameter = p.radius * 2 * 1000;
            total++;

            if (diameter <= 1) bins[0]++;
            else if (diameter <= 2) bins[1]++;
            else if (diameter <= 3) bins[2]++;
            else if (diameter <= 4) bins[3]++;
            else if (diameter <= 5) bins[4]++;
            else bins[5]++;
        }

        if (total > 0) {
            for (let i = 0; i < 6; i++) {
                bins[i] /= total;
            }
        }

        return bins;
    }

    getStats() {
        let particleCount = 0;
        let brokenCount = 0;
        let totalForce = 0;
        let maxForce = 0;

        for (const p of this.particleData) {
            if (p.broken) {
                brokenCount++;
            } else {
                particleCount++;
                const force = Math.sqrt(p.vx * p.vx + p.vy * p.vy + p.vz * p.vz) * p.mass;
                totalForce += force;
                maxForce = Math.max(maxForce, force);
            }
        }

        return {
            particleCount: particleCount,
            breakageRatio: particleCount > 0 ? brokenCount / particleCount : 0,
            avgForce: particleCount > 0 ? totalForce / particleCount : 0,
            maxForce: maxForce,
            simTime: this.isSimulating ? Date.now() / 1000 % 1000 : 0
        };
    }

    resetParticles(count = 200) {
        this.createGrainParticles(count);
    }

    resize() {
        this.onResize();
    }

    animate() {
        this.animationId = requestAnimationFrame(() => this.animate());

        if (this.rollerArm) {
            this.rotationAngle += this.rollerSpeed * 0.01;
            this.rollerArm.rotation.y = this.rotationAngle;

            if (this.roller) {
                this.roller.rotation.x -= this.rollerSpeed * 0.05;
            }
        }

        if (this.isSimulating) {
            this.updateParticlePositions();
        }

        if (this.showForces) {
            this.updateForceArrows();
        }

        this.controls.update();
        this.renderer.render(this.scene, this.camera);
    }

    onResize() {
        if (!this.container || !this.camera || !this.renderer) return;

        const rect = this.container.getBoundingClientRect();
        const width = rect.width;
        const height = rect.height;

        this.camera.aspect = width / height;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(width, height);
    }

    dispose() {
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
        }

        if (this.grainInstanced) {
            if (this.grainInstanced.geometry) this.grainInstanced.geometry.dispose();
            if (this.grainInstanced.material) this.grainInstanced.material.dispose();
        }

        if (this.renderer) {
            this.renderer.dispose();
            if (this.container && this.renderer.domElement) {
                this.container.removeChild(this.renderer.domElement);
            }
        }
    }
}
