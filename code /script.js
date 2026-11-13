
const BROKER = "wss://broker.hivemq.com:8884/mqtt";

const TOPIC_SENSOR = "aquaculture/lab4/esp32_a8f31c/sensor";
const TOPIC_CONTROL = "aquaculture/lab4/esp32_a8f31c/control";
const TOPIC_STATUS = "aquaculture/lab4/esp32_a8f31c/status";

const client = mqtt.connect(BROKER, {
    clientId: "AQUA_WEB_" + Math.random().toString(16).slice(2),
    clean: true,
    connectTimeout: 10000,
    reconnectPeriod: 3000
});

let pumpState = 0;
let valveState = 0;
let currentMode = "AUTO";

// ==================================
// KET NOI MQTT
// ==================================

client.on("connect", () => {
    document.getElementById("mqttStatus").innerText =
        "Đã kết nối MQTT";

    client.subscribe([TOPIC_SENSOR, TOPIC_STATUS]);

    console.log("MQTT CONNECTED");
});

client.on("reconnect", () => {
    document.getElementById("mqttStatus").innerText =
        "Đang kết nối lại MQTT...";
});

client.on("error", (error) => {
    console.log("MQTT ERROR:", error);
    document.getElementById("mqttStatus").innerText =
        "Lỗi kết nối MQTT";
});

client.on("offline", () => {
    document.getElementById("mqttStatus").innerText =
        "MQTT ngoại tuyến";
});

// ==================================
// NHAN DU LIEU MQTT
// ==================================

client.on("message", (topic, message) => {
    let data;

    try {
        data = JSON.parse(message.toString());
    } catch (error) {
        console.log("Du lieu MQTT khong hop le");
        return;
    }

    // Du lieu cam bien
    if (topic === TOPIC_SENSOR) {

        const level = Number(data.waterLevel);
        const temp = Number(data.temperature);

        if (!Number.isFinite(level) ||
            !Number.isFinite(temp)) return;

        document.getElementById("waterLevel").innerText =
            level.toFixed(2);

        document.getElementById("temperature").innerText =
            temp.toFixed(1);

        updateCharts(level, temp);
    }

    // Trang thai thiet bi
    if (topic === TOPIC_STATUS) {

        pumpState = Number(data.pump);
        valveState = Number(data.valve);
        currentMode = data.mode;

        document.getElementById("pumpStatus").innerText =
            pumpState ? "Đang bật" : "Đang tắt";

        document.getElementById("valveStatus").innerText =
            valveState ? "Đang mở" : "Đang đóng";

        document.getElementById("mode").value = currentMode;

        document.getElementById("pumpButton").innerText =
            pumpState ? "Tắt máy bơm" : "Bật máy bơm";

        document.getElementById("valveButton").innerText =
            valveState ? "Đóng van xả" : "Mở van xả";
    }
});

// ==================================
// BIEU DO
// ==================================

const labels = [];

const waterValues = [];
const tempValues = [];

const waterChart = new Chart(
    document.getElementById("waterChart"),
    {
        type: "line",
        data: {
            labels: labels,
            datasets: [{
                label: "Mực nước (m)",
                data: waterValues,
                borderColor: "#0796d2",
                backgroundColor: "rgba(7,150,210,0.15)",
                fill: true,
                tension: 0.3
            }]
        },
        options: {
            responsive: true,
            animation: false
        }
    }
);

const temperatureChart = new Chart(
    document.getElementById("temperatureChart"),
    {
        type: "line",
        data: {
            labels: labels,
            datasets: [{
                label: "Nhiệt độ (°C)",
                data: tempValues,
                borderColor: "#ff9800",
                backgroundColor: "rgba(255,152,0,0.15)",
                fill: true,
                tension: 0.3
            }]
        },
        options: {
            responsive: true,
            animation: false
        }
    }
);

function updateCharts(level, temp) {

    const time = new Date().toLocaleTimeString();

    labels.push(time);
    waterValues.push(level);
    tempValues.push(temp);

    if (labels.length > 15) {
        labels.shift();
        waterValues.shift();
        tempValues.shift();
    }

    waterChart.update();
    temperatureChart.update();
}

// ==================================
// GUI LENH DIEU KHIEN
// ==================================

function sendControl() {

    if (!client.connected) {
        alert("Chưa kết nối MQTT!");
        return;
    }

    const command = {
        mode: currentMode,
        pump: pumpState,
        valve: valveState
    };

    client.publish(TOPIC_CONTROL, JSON.stringify(command));

    console.log("Da gui lenh:", command);
}

// ==================================
// CHON CHE DO
// ==================================

document.getElementById("mode").addEventListener("change", function() {

    currentMode = this.value;

    if (currentMode === "AUTO") {
        pumpState = 0;
        valveState = 0;
    }

    sendControl();
});

// ==================================
// NUT MAY BOM
// ==================================

function togglePump() {

    if (currentMode !== "MANUAL") {
        alert("Hãy chuyển sang chế độ Thủ công!");
        return;
    }

    pumpState = pumpState ? 0 : 1;

    if (pumpState) valveState = 0;

    sendControl();
}

// ==================================
// NUT VAN XA
// ==================================

function toggleValve() {

    if (currentMode !== "MANUAL") {
        alert("Hãy chuyển sang chế độ Thủ công!");
        return;
    }

    valveState = valveState ? 0 : 1;

    if (valveState) pumpState = 0;

    sendControl();
}
