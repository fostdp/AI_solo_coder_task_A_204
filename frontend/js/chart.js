class GrainChart {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) return;
        this.ctx = this.canvas.getContext('2d');
        this.data = [0.1, 0.2, 0.3, 0.2, 0.15, 0.05];
        this.labels = ['0-1mm', '1-2mm', '2-3mm', '3-4mm', '4-5mm', '>5mm'];
        this.colors = [
            '#e74c3c',
            '#e67e22',
            '#f1c40f',
            '#2ecc71',
            '#3498db',
            '#9b59b6'
        ];
        this.animationProgress = 1;
        this.targetData = [...this.data];
        this.resize();
        window.addEventListener('resize', () => this.resize());
    }

    resize() {
        if (!this.canvas) return;
        const rect = this.canvas.getBoundingClientRect();
        this.canvas.width = rect.width * window.devicePixelRatio;
        this.canvas.height = rect.height * window.devicePixelRatio;
        this.ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
        this.width = rect.width;
        this.height = rect.height;
    }

    setData(data) {
        if (!data || data.length !== 6) return;
        this.targetData = [...data];
        this.animationProgress = 0;
    }

    draw() {
        if (!this.canvas || !this.ctx) return;

        if (this.animationProgress < 1) {
            this.animationProgress += 0.05;
            if (this.animationProgress > 1) this.animationProgress = 1;

            for (let i = 0; i < 6; i++) {
                this.data[i] += (this.targetData[i] - this.data[i]) * 0.1;
            }
        }

        this.ctx.clearRect(0, 0, this.width, this.height);

        const padding = { top: 20, right: 20, bottom: 40, left: 50 };
        const chartWidth = this.width - padding.left - padding.right;
        const chartHeight = this.height - padding.top - padding.bottom;

        this.ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
        this.ctx.lineWidth = 1;
        for (let i = 0; i <= 5; i++) {
            const y = padding.top + (chartHeight / 5) * i;
            this.ctx.beginPath();
            this.ctx.moveTo(padding.left, y);
            this.ctx.lineTo(this.width - padding.right, y);
            this.ctx.stroke();

            this.ctx.fillStyle = 'rgba(255, 255, 255, 0.5)';
            this.ctx.font = '11px sans-serif';
            this.ctx.textAlign = 'right';
            this.ctx.fillText((100 - i * 20) + '%', padding.left - 5, y + 4);
        }

        const barWidth = (chartWidth / 6) * 0.7;
        const barGap = (chartWidth / 6) * 0.3;

        let maxValue = Math.max(...this.data, 0.01);

        for (let i = 0; i < 6; i++) {
            const x = padding.left + (chartWidth / 6) * i + barGap / 2;
            const value = this.data[i];
            const barHeight = (value / maxValue) * chartHeight;
            const y = padding.top + chartHeight - barHeight;

            const gradient = this.ctx.createLinearGradient(x, y, x, y + barHeight);
            gradient.addColorStop(0, this.colors[i]);
            gradient.addColorStop(1, this.adjustColor(this.colors[i], -30));

            this.ctx.fillStyle = gradient;
            this.ctx.beginPath();
            this.ctx.roundRect(x, y, barWidth, barHeight, 4);
            this.ctx.fill();

            this.ctx.shadowColor = this.colors[i];
            this.ctx.shadowBlur = 10;
            this.ctx.fillStyle = this.colors[i];
            this.ctx.fillRect(x, y, barWidth, 2);
            this.ctx.shadowBlur = 0;

            this.ctx.fillStyle = 'rgba(255, 255, 255, 0.8)';
            this.ctx.font = '12px sans-serif';
            this.ctx.textAlign = 'center';
            const percent = (value * 100).toFixed(1);
            this.ctx.fillText(percent + '%', x + barWidth / 2, y - 8);

            this.ctx.fillStyle = 'rgba(255, 255, 255, 0.6)';
            this.ctx.font = '11px sans-serif';
            this.ctx.fillText(this.labels[i], x + barWidth / 2, this.height - padding.bottom + 20);
        }

        if (this.animationProgress < 1) {
            requestAnimationFrame(() => this.draw());
        }
    }

    adjustColor(color, amount) {
        const hex = color.replace('#', '');
        const r = Math.max(0, Math.min(255, parseInt(hex.substr(0, 2), 16) + amount));
        const g = Math.max(0, Math.min(255, parseInt(hex.substr(2, 2), 16) + amount));
        const b = Math.max(0, Math.min(255, parseInt(hex.substr(4, 2), 16) + amount));
        return `rgb(${r}, ${g}, ${b})`;
    }
}

class TrendChart {
    constructor(canvasId, color = '#3498db') {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) return;
        this.ctx = this.canvas.getContext('2d');
        this.data = [];
        this.color = color;
        this.maxPoints = 60;
        this.resize();
        window.addEventListener('resize', () => this.resize());
    }

    resize() {
        if (!this.canvas) return;
        const rect = this.canvas.getBoundingClientRect();
        this.canvas.width = rect.width * window.devicePixelRatio;
        this.canvas.height = rect.height * window.devicePixelRatio;
        this.ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
        this.width = rect.width;
        this.height = rect.height;
    }

    addValue(value) {
        this.data.push(value);
        if (this.data.length > this.maxPoints) {
            this.data.shift();
        }
    }

    draw() {
        if (!this.canvas || !this.ctx || this.data.length < 2) return;

        this.ctx.clearRect(0, 0, this.width, this.height);

        const padding = { top: 20, right: 20, bottom: 20, left: 40 };
        const chartWidth = this.width - padding.left - padding.right;
        const chartHeight = this.height - padding.top - padding.bottom;

        const maxVal = Math.max(...this.data) * 1.1;
        const minVal = Math.min(...this.data) * 0.9;
        const range = maxVal - minVal || 1;

        this.ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
        this.ctx.lineWidth = 1;
        for (let i = 0; i <= 4; i++) {
            const y = padding.top + (chartHeight / 4) * i;
            this.ctx.beginPath();
            this.ctx.moveTo(padding.left, y);
            this.ctx.lineTo(this.width - padding.right, y);
            this.ctx.stroke();

            const val = maxVal - (range / 4) * i;
            this.ctx.fillStyle = 'rgba(255, 255, 255, 0.5)';
            this.ctx.font = '10px sans-serif';
            this.ctx.textAlign = 'right';
            this.ctx.fillText(val.toFixed(1), padding.left - 5, y + 3);
        }

        this.ctx.strokeStyle = this.color;
        this.ctx.lineWidth = 2;
        this.ctx.beginPath();

        for (let i = 0; i < this.data.length; i++) {
            const x = padding.left + (chartWidth / (this.maxPoints - 1)) * (this.maxPoints - this.data.length + i);
            const y = padding.top + chartHeight - ((this.data[i] - minVal) / range) * chartHeight;

            if (i === 0) {
                this.ctx.moveTo(x, y);
            } else {
                this.ctx.lineTo(x, y);
            }
        }
        this.ctx.stroke();

        const gradient = this.ctx.createLinearGradient(0, padding.top, 0, padding.top + chartHeight);
        gradient.addColorStop(0, this.color + '40');
        gradient.addColorStop(1, this.color + '05');

        this.ctx.fillStyle = gradient;
        this.ctx.lineTo(padding.left + (chartWidth / (this.maxPoints - 1)) * (this.maxPoints - 1), padding.top + chartHeight);
        this.ctx.lineTo(padding.left + (chartWidth / (this.maxPoints - 1)) * (this.maxPoints - this.data.length), padding.top + chartHeight);
        this.ctx.closePath();
        this.ctx.fill();

        if (this.data.length > 0) {
            const lastX = padding.left + (chartWidth / (this.maxPoints - 1)) * (this.maxPoints - 1);
            const lastY = padding.top + chartHeight - ((this.data[this.data.length - 1] - minVal) / range) * chartHeight;

            this.ctx.beginPath();
            this.ctx.arc(lastX, lastY, 5, 0, Math.PI * 2);
            this.ctx.fillStyle = this.color;
            this.ctx.fill();
            this.ctx.strokeStyle = '#fff';
            this.ctx.lineWidth = 2;
            this.ctx.stroke();
        }
    }
}

class FitnessChart {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) return;
        this.ctx = this.canvas.getContext('2d');
        this.bestFitness = [];
        this.avgFitness = [];
        this.resize();
        window.addEventListener('resize', () => this.resize());
    }

    resize() {
        if (!this.canvas) return;
        const rect = this.canvas.getBoundingClientRect();
        this.canvas.width = rect.width * window.devicePixelRatio;
        this.canvas.height = rect.height * window.devicePixelRatio;
        this.ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
        this.width = rect.width;
        this.height = rect.height;
    }

    addGeneration(best, avg) {
        this.bestFitness.push(best);
        this.avgFitness.push(avg);
        if (this.bestFitness.length > 100) {
            this.bestFitness.shift();
            this.avgFitness.shift();
        }
    }

    reset() {
        this.bestFitness = [];
        this.avgFitness = [];
    }

    draw() {
        if (!this.canvas || !this.ctx) return;

        this.ctx.clearRect(0, 0, this.width, this.height);

        const padding = { top: 20, right: 20, bottom: 30, left: 45 };
        const chartWidth = this.width - padding.left - padding.right;
        const chartHeight = this.height - padding.top - padding.bottom;

        const allData = [...this.bestFitness, ...this.avgFitness, 0, 1];
        const maxVal = Math.max(...allData) * 1.1;
        const minVal = Math.min(...allData) * 0.9;
        const range = maxVal - minVal || 1;

        this.ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
        this.ctx.lineWidth = 1;
        for (let i = 0; i <= 5; i++) {
            const y = padding.top + (chartHeight / 5) * i;
            this.ctx.beginPath();
            this.ctx.moveTo(padding.left, y);
            this.ctx.lineTo(this.width - padding.right, y);
            this.ctx.stroke();

            const val = maxVal - (range / 5) * i;
            this.ctx.fillStyle = 'rgba(255, 255, 255, 0.5)';
            this.ctx.font = '10px sans-serif';
            this.ctx.textAlign = 'right';
            this.ctx.fillText(val.toFixed(2), padding.left - 5, y + 3);
        }

        this.drawLine(this.bestFitness, '#2ecc71', chartWidth, chartHeight, padding, maxVal, minVal, range);
        this.drawLine(this.avgFitness, '#f39c12', chartWidth, chartHeight, padding, maxVal, minVal, range);

        this.ctx.font = '11px sans-serif';
        this.ctx.textAlign = 'left';
        this.ctx.fillStyle = '#2ecc71';
        this.ctx.fillRect(padding.left, this.height - 18, 12, 12);
        this.ctx.fillStyle = 'rgba(255, 255, 255, 0.8)';
        this.ctx.fillText('最佳适应度', padding.left + 18, this.height - 8);

        this.ctx.fillStyle = '#f39c12';
        this.ctx.fillRect(padding.left + 120, this.height - 18, 12, 12);
        this.ctx.fillStyle = 'rgba(255, 255, 255, 0.8)';
        this.ctx.fillText('平均适应度', padding.left + 138, this.height - 8);
    }

    drawLine(data, color, chartWidth, chartHeight, padding, maxVal, minVal, range) {
        if (data.length < 2) return;

        this.ctx.strokeStyle = color;
        this.ctx.lineWidth = 2;
        this.ctx.beginPath();

        for (let i = 0; i < data.length; i++) {
            const x = padding.left + (chartWidth / (data.length - 1)) * i;
            const y = padding.top + chartHeight - ((data[i] - minVal) / range) * chartHeight;

            if (i === 0) {
                this.ctx.moveTo(x, y);
            } else {
                this.ctx.lineTo(x, y);
            }
        }
        this.ctx.stroke();

        if (data.length > 0) {
            const lastX = padding.left + (chartWidth / (data.length - 1)) * (data.length - 1);
            const lastY = padding.top + chartHeight - ((data[data.length - 1] - minVal) / range) * chartHeight;

            this.ctx.beginPath();
            this.ctx.arc(lastX, lastY, 4, 0, Math.PI * 2);
            this.ctx.fillStyle = color;
            this.ctx.fill();
        }
    }
}
