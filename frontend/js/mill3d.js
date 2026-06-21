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
        this.grainParticles = null;
        this.forceArrows = [];

        this.particleData = [];
        this.showParticles = true;
        this.showForces = false;
        this.rollerSpeed = 0;
        this.rollerGap = 2.0;
        this.isSimulating = false;
        this.rotationAngle = 0;

        this.animationId = null;
        this.init();
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

        this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
        this.renderer.setSize(width, height);
        this.renderer.setPixelRatio(window.devicePixelRatio);
        this.renderer.shadowMap.enabled = true;
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

        const mainLight = new THREE.DirectionalLight(0xffffff, 0.8);
        mainLight.position.set(5, 8, 5);
        mainLight.castShadow = true;
        mainLight.shadow.mapSize.width = 2048;
        mainLight.shadow.mapSize.height = 2048;
        mainLight.shadow.camera.near = 0.5;
        mainLight.shadow.camera.far = 50;
        mainLight.shadow.camera.left = -10;
        mainLight.shadow.camera.right = 10;
        mainLight.shadow.camera.top = 10;
        mainLight.shadow.camera.bottom = -10;
        this.scene.add(mainLight);

        const fillLight = new THREE.DirectionalLight(0xffa500, 0.3);
        fillLight.position.set(-5, 3, -5);
        this.scene.add(fillLight);

        const pointLight = new THREE.PointLight(0xff6600, 0.5, 10);
        pointLight.position.set(0, 2, 0);
        this.scene.add(pointLight);
    }

    createMillModel() {
        const baseGroup = new THREE.Group();
        this.millBase = baseGroup;

        const baseGeometry = new THREE.CylinderGeometry(2.1, 2.2, 0.3, 64);
        const stoneMaterial = new THREE.MeshStandardMaterial({
            color: 0x8b7355,
            roughness: 0.9,
            metalness: 0.1
        });
        const base = new THREE.Mesh(baseGeometry, stoneMaterial);
        base.position.y = -0.15;
        base.receiveShadow = true;
        baseGroup.add(base);

        const platformGeometry = new THREE.CylinderGeometry(2.0, 2.0, 0.05, 128);
        const platformMaterial = new THREE.MeshStandardMaterial({
            color: 0x6b5344,
            roughness: 0.8,
            metalness: 0.1
        });
        const platform = new THREE.Mesh(platformGeometry, platformMaterial);
        platform.position.y = 0.025;
        platform.receiveShadow = true;
        baseGroup.add(platform);

        const grooveGeometry = new THREE.TorusGeometry(1.8, 0.02, 8, 128);
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

        const edgeGeometry = new THREE.TorusGeometry(2.0, 0.08, 16, 128);
        const edgeMaterial = new THREE.MeshStandardMaterial({
            color: 0x4a3728,
            roughness: 0.7,
            metalness: 0.2
        });
        const edge = new THREE.Mesh(edgeGeometry, edgeMaterial);
        edge.rotation.x = Math.PI / 2;
        edge.position.y = 0.05;
        baseGroup.add(edge);

        const centerPoleGeometry = new THREE.CylinderGeometry(0.05, 0.06, 1.5, 16);
        const woodMaterial = new THREE.MeshStandardMaterial({
            color: 0x5c4033,
            roughness: 0.8,
            metalness: 0.1
        });
        const centerPole = new THREE.Mesh(centerPoleGeometry, woodMaterial);
        centerPole.position.y = 0.75;
        centerPole.castShadow = true;
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
        arm.castShadow = true;
        armGroup.add(arm);

        const supportGeometry = new THREE.BoxGeometry(0.1, 1.0, 0.1);
        const support = new THREE.Mesh(supportGeometry, woodMaterial);
        support.position.set(1.5, 0.7, 0);
        support.castShadow = true;
        armGroup.add(support);

        const rollerGroup = new THREE.Group();
        this.roller = rollerGroup;

        const rollerGeometry = new THREE.CylinderGeometry(0.8, 0.8, 0.3, 32);
        const stoneMaterial = new THREE.MeshStandardMaterial({
            color: 0x7a6b5a,
            roughness: 0.85,
            metalness: 0.1
        });
        const rollerMesh = new THREE.Mesh(rollerGeometry, stoneMaterial);
        rollerMesh.rotation.z = Math.PI / 2;
        rollerMesh.castShadow = true;
        rollerMesh.receiveShadow = true;
        rollerGroup.add(rollerMesh);

        const rimGeometry = new THREE.TorusGeometry(0.8, 0.02, 12, 64);
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
        ground.receiveShadow = true;
        this.scene.add(ground);

        const gridHelper = new THREE.GridHelper(20, 40, 0x333344, 0x222233);
        gridHelper.position.y = -0.3;
        this.scene.add(gridHelper);
    }

    createGrainParticles(count = 200) {
        if (this.grainParticles) {
            this.scene.remove(this.grainParticles);
        }

        this.particleData = [];
        const positions = new Float32Array(count * 3);
        const colors = new Float32Array(count * 3);
        const sizes = new Float32Array(count);

        const grainColors = [
            new THREE.Color(0xd4a574),
            new THREE.Color(0xc4956a),
            new THREE.Color(0xb8865a),
            new THREE.Color(0xe8c49c),
            new THREE.Color(0xf0d5a8)
        ];

        for (let i = 0; i < count; i++) {
            const angle = Math.random() * Math.PI * 2;
            const radius = 0.3 + Math.random() * 1.4;
            const x = Math.cos(angle) * radius;
            const z = Math.sin(angle) * radius;
            const y = 0.05 + Math.random() * 0.1;

            positions[i * 3] = x;
            positions[i * 3 + 1] = y;
            positions[i * 3 + 2] = z;

            const color = grainColors[Math.floor(Math.random() * grainColors.length)];
            colors[i * 3] = color.r;
            colors[i * 3 + 1] = color.g;
            colors[i * 3 + 2] = color.b;

            const size = 0.02 + Math.random() * 0.04;
            sizes[i] = size;

            this.particleData.push({
                x, y, z,
                vx: 0, vy: 0, vz: 0,
                radius: size,
                mass: size * size * size * 1200,
                originalY: y,
                broken: false,
                colorIndex: Math.floor(Math.random() * grainColors.length)
            });
        }

        const geometry = new THREE.BufferGeometry();
        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));

        const material = new THREE.PointsMaterial({
            size: 0.05,
            vertexColors: true,
            transparent: true,
            opacity: 0.9,
            sizeAttenuation: true
        });

        this.grainParticles = new THREE.Points(geometry, material);
        this.scene.add(this.grainParticles);
    }

    updateParticlePositions() {
        if (!this.grainParticles || !this.showParticles) return;

        const positions = this.grainParticles.geometry.attributes.position.array;
        const colors = this.grainParticles.geometry.attributes.color.array;

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

            positions[i * 3] = p.x;
            positions[i * 3 + 1] = p.y;
            positions[i * 3 + 2] = p.z;
        }

        this.grainParticles.geometry.attributes.position.needsUpdate = true;
    }

    breakParticle(index) {
        const p = this.particleData[index];
        p.broken = true;

        const colors = this.grainParticles.geometry.attributes.color.array;
        colors[index * 3] = 0.5;
        colors[index * 3 + 1] = 0.5;
        colors[index * 3 + 2] = 0.5;

        const positions = this.grainParticles.geometry.attributes.position.array;
        positions[index * 3 + 1] = -10;

        for (let i = 0; i < 3; i++) {
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
                colorIndex: p.colorIndex
            };

            this.particleData.push(newP);

            const newPositions = new Float32Array((newIdx + 1) * 3);
            const newColors = new Float32Array((newIdx + 1) * 3);
            newPositions.set(positions);
            newColors.set(colors);

            newPositions[newIdx * 3] = newP.x;
            newPositions[newIdx * 3 + 1] = newP.y;
            newPositions[newIdx * 3 + 2] = newP.z;

            const grainColors = [
                new THREE.Color(0xd4a574),
                new THREE.Color(0xc4956a),
                new THREE.Color(0xb8865a),
                new THREE.Color(0xe8c49c),
                new THREE.Color(0xf0d5a8)
            ];
            const color = grainColors[newP.colorIndex];
            newColors[newIdx * 3] = color.r;
            newColors[newIdx * 3 + 1] = color.g;
            newColors[newIdx * 3 + 2] = color.b;

            this.grainParticles.geometry.setAttribute('position', new THREE.BufferAttribute(newPositions, 3));
            this.grainParticles.geometry.setAttribute('color', new THREE.BufferAttribute(newColors, 3));
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

    setShowParticles(show) {
        this.showParticles = show;
        if (this.grainParticles) {
            this.grainParticles.visible = show;
        }
    }

    setShowForces(show) {
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

    resetView() {
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

        if (this.renderer) {
            this.renderer.dispose();
            if (this.container && this.renderer.domElement) {
                this.container.removeChild(this.renderer.domElement);
            }
        }
    }
}
