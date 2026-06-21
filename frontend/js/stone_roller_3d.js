class StoneRoller3D {
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

        this.rotationAngle = 0;
        this.rollerSpeed = 0;

        this.isMobile = this.detectMobile();
        this.dummy = new THREE.Object3D();

        this.animationCallbacks = [];
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

        const stoneMaterial = new THREE.MeshStandardMaterial({
            color: 0x8b7355, roughness: 0.9, metalness: 0.1
        });
        const platformMaterial = new THREE.MeshStandardMaterial({
            color: 0x6b5344, roughness: 0.8, metalness: 0.1
        });
        const grooveMaterial = new THREE.MeshStandardMaterial({
            color: 0x3d2817, roughness: 0.9, metalness: 0.1
        });
        const edgeMaterial = new THREE.MeshStandardMaterial({
            color: 0x4a3728, roughness: 0.7, metalness: 0.2
        });
        const woodMaterial = new THREE.MeshStandardMaterial({
            color: 0x5c4033, roughness: 0.8, metalness: 0.1
        });

        const baseGeometry = new THREE.CylinderGeometry(2.1, 2.2, 0.3, this.isMobile ? 32 : 64);
        const base = new THREE.Mesh(baseGeometry, stoneMaterial);
        base.position.y = -0.15;
        base.receiveShadow = !this.isMobile;
        baseGroup.add(base);

        const platformGeometry = new THREE.CylinderGeometry(2.0, 2.0, 0.05, this.isMobile ? 64 : 128);
        const platform = new THREE.Mesh(platformGeometry, platformMaterial);
        platform.position.y = 0.025;
        platform.receiveShadow = !this.isMobile;
        baseGroup.add(platform);

        const grooveGeometry = new THREE.TorusGeometry(1.8, 0.02, 8, this.isMobile ? 64 : 128);
        for (let i = 0; i < 3; i++) {
            const groove = new THREE.Mesh(grooveGeometry, grooveMaterial);
            groove.rotation.x = Math.PI / 2;
            groove.position.y = 0.05 + i * 0.005;
            groove.scale.set(0.95 - i * 0.1, 0.95 - i * 0.1, 1);
            baseGroup.add(groove);
        }

        const edgeGeometry = new THREE.TorusGeometry(2.0, 0.08, 16, this.isMobile ? 64 : 128);
        const edge = new THREE.Mesh(edgeGeometry, edgeMaterial);
        edge.rotation.x = Math.PI / 2;
        edge.position.y = 0.05;
        baseGroup.add(edge);

        const centerPoleGeometry = new THREE.CylinderGeometry(0.05, 0.06, 1.5, this.isMobile ? 8 : 16);
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

        const woodMaterial = new THREE.MeshStandardMaterial({
            color: 0x6b4423, roughness: 0.8, metalness: 0.1
        });
        const stoneMaterial = new THREE.MeshStandardMaterial({
            color: 0x7a6248, roughness: 0.95, metalness: 0.05
        });
        const metalMaterial = new THREE.MeshStandardMaterial({
            color: 0x888888, roughness: 0.4, metalness: 0.8
        });

        const armGeometry = new THREE.BoxGeometry(1.5, 0.08, 0.08);
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

        const cylinderGeometry = new THREE.CylinderGeometry(0.35, 0.35, 0.6, this.isMobile ? 16 : 32);
        const cylinder = new THREE.Mesh(cylinderGeometry, stoneMaterial);
        cylinder.rotation.z = Math.PI / 2;
        cylinder.castShadow = !this.isMobile;
        cylinder.receiveShadow = !this.isMobile;
        rollerGroup.add(cylinder);

        const shaftGeometry = new THREE.CylinderGeometry(0.04, 0.04, 0.7, 8);
        const shaft = new THREE.Mesh(shaftGeometry, metalMaterial);
        shaft.rotation.z = Math.PI / 2;
        rollerGroup.add(shaft);

        const hubGeometry = new THREE.CylinderGeometry(0.1, 0.1, 0.05, 16);
        const hub1 = new THREE.Mesh(hubGeometry, metalMaterial);
        hub1.rotation.z = Math.PI / 2;
        hub1.position.x = -0.32;
        rollerGroup.add(hub1);
        const hub2 = hub1.clone();
        hub2.position.x = 0.32;
        rollerGroup.add(hub2);

        rollerGroup.position.set(1.5, 0.3, 0);
        armGroup.add(rollerGroup);

        this.scene.add(armGroup);
    }

    createGround() {
        const groundGeometry = new THREE.PlaneGeometry(30, 30);
        const groundMaterial = new THREE.MeshStandardMaterial({
            color: 0x1a1a2e, roughness: 1.0, metalness: 0
        });
        const ground = new THREE.Mesh(groundGeometry, groundMaterial);
        ground.rotation.x = -Math.PI / 2;
        ground.position.y = -0.3;
        ground.receiveShadow = !this.isMobile;
        this.scene.add(ground);

        const gridHelper = new THREE.GridHelper(20, 40, 0x2a2a4a, 0x1a1a2e);
        gridHelper.position.y = -0.29;
        this.scene.add(gridHelper);
    }

    setRollerSpeed(speed) {
        this.rollerSpeed = speed;
    }

    addAnimationCallback(fn) {
        this.animationCallbacks.push(fn);
    }

    removeAnimationCallback(fn) {
        this.animationCallbacks = this.animationCallbacks.filter(f => f !== fn);
    }

    onResize() {
        if (!this.container) return;
        const rect = this.container.getBoundingClientRect();
        this.camera.aspect = rect.width / rect.height;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(rect.width, rect.height);
    }

    animate() {
        this.animationId = requestAnimationFrame(() => this.animate());

        if (this.rollerSpeed && this.rollerArm) {
            this.rotationAngle += this.rollerSpeed * 0.001;
            this.rollerArm.rotation.y = this.rotationAngle;
            if (this.roller) {
                this.roller.rotation.y -= this.rollerSpeed * 0.01;
            }
        }

        for (const cb of this.animationCallbacks) {
            try { cb(this.rotationAngle); } catch (e) {}
        }

        if (this.controls) this.controls.update();
        if (this.renderer && this.scene && this.camera) {
            this.renderer.render(this.scene, this.camera);
        }
    }

    addToScene(obj) {
        if (this.scene) this.scene.add(obj);
    }

    removeFromScene(obj) {
        if (this.scene) this.scene.remove(obj);
    }

    dispose() {
        if (this.animationId) cancelAnimationFrame(this.animationId);
        if (this.renderer) {
            this.renderer.dispose();
            if (this.renderer.domElement.parentNode) {
                this.renderer.domElement.parentNode.removeChild(this.renderer.domElement);
            }
        }
    }
}
