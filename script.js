
// ===================================
// 1. KHOI TAO DU LIEU
// ===================================

let waterLevel = 1.30;
let temperature = 28.5;

let pumpState = false;
let valveState = false;

// ===================================
// 2. HIEN THI DU LIEU
// ===================================

function updateDisplay() {
    document.getElementById("waterLevel").textContent =
        waterLevel.toFixed(2);

    document.getElementById("temperature").textContent =
        temperature.toFixed(1);
}

// ===================================
// 3. KHOI TAO BIEU DO
// ===================================

const labels = [];

const waterValues = [];
const temperatureValues = [];

const waterChart = new Chart(
    document.getElementById("waterChart"),
    {
        type: "line",
        data: {
            labels: labels,
            datasets: [{
                label: "Mực nước (m)",
                data: waterValues,
                borderColor: "#0284c7",
                backgroundColor: "rgba(2,132,199,0.15)",
                fill: true,
                tension: 0.4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: {
                    min: 0,
                    max: 2.5
                }
            }
        }
    }
);

const temperatureChart = new Chart(
    document.getElementById("temperatureChart"),
    {
        type: "line",
        data: {
            labels: [],
            datasets: [{
                label: "Nhiệt độ (°C)",
                data: [],
                borderColor: "#f59e0b",
                backgroundColor: "rgba(245,158,11,0.15)",
                fill: true,
                tension: 0.4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false
        }
    }
);

// ===================================
// 4. CAP NHAT BIEU DO
// ===================================

function updateCharts() {
    const time = new Date().toLocaleTimeString();

    labels.push(time);
    waterValues.push(waterLevel);

    temperatureChart.data.labels.push(time);
    temperatureChart.data.datasets[0].data.push(temperature);

    if (labels.length > 10) {
        labels.shift();
        waterValues.shift();

        temperatureChart.data.labels.shift();
        temperatureChart.data.datasets[0].data.shift();
    }

    waterChart.update();
    temperatureChart.update();
}

// ===================================
// 5. DU LIEU MO PHONG
// ===================================

// Dữ liệu mẫu, chưa phải dữ liệu từ ESP32

function simulateSensorData() {
    waterLevel += (Math.random() - 0.5) * 0.06;
    temperature += (Math.random() - 0.5) * 0.4;

    waterLevel = Math.max(0, Math.min(2.5, waterLevel));

    updateDisplay();
    updateCharts();
}

updateDisplay();
updateCharts();

setInterval(simulateSensorData, 2000);

// ===================================
// 6. DIEU KHIEN MAY BOM
// ===================================

function togglePump() {
    if (document.getElementById("mode").value !== "MANUAL") {
        alert("Vui lòng chuyển sang chế độ thủ công!");
        return;
    }

    pumpState = !pumpState;

    document.getElementById("pumpStatus").textContent =
        pumpState ? "Đang bật" : "Đang tắt";

    document.getElementById("pumpButton").textContent =
        pumpState ? "Tắt máy bơm" : "Bật máy bơm";
}

// ===================================
// 7. DIEU KHIEN VAN XA
// ===================================

function toggleValve() {
    if (document.getElementById("mode").value !== "MANUAL") {
        alert("Vui lòng chuyển sang chế độ thủ công!");
        return;
    }

    valveState = !valveState;

    document.getElementById("valveStatus").textContent =
        valveState ? "Đang mở" : "Đang đóng";

    document.getElementById("valveButton").textContent =
        valveState ? "Đóng van xả" : "Mở van xả";
}

// ===================================
// 8. THAY DOI CHE DO
// ===================================

document.getElementById("mode").addEventListener("change", function() {
    const manual = this.value === "MANUAL";

    document.getElementById("pumpButton").disabled = !manual;
    document.getElementById("valveButton").disabled = !manual;

    if (!manual) {
        pumpState = false;
        valveState = false;

        document.getElementById("pumpStatus").textContent = "Đang tắt";
        document.getElementById("valveStatus").textContent = "Đang đóng";

        document.getElementById("pumpButton").textContent = "Bật máy bơm";
        document.getElementById("valveButton").textContent = "Mở van xả";
    }
});

// Ban đầu ở chế độ tự động
document.getElementById("pumpButton").disabled = true;
document.getElementById("valveButton").disabled = true;

// ===================================
// 9. TRANG THAI MQTT
// ===================================

document.getElementById("mqttStatus").textContent =
    "Chưa kết nối ESP32 - MQTT";

