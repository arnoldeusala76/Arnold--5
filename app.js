import {
    initializeApp
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";

import {
    getDatabase,
    ref,
    onValue
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-database.js";

const firebaseConfig = {
    apiKey: "AIzaSyB9-FYcrAgyAPitLZrJpxyMZs27TEtc9AQ",
    authDomain: "arnold-28517.firebaseapp.com",
    databaseURL: "https://arnold-28517-default-rtdb.europe-west1.firebasedatabase.app/",
    projectId: "arnold-28517",
    storageBucket: "arnold-28517.firebasestorage.app",
    messagingSenderId: "443115176489",
    appId: "1:443115176489:web:844e596c22d9e63acb5238",
    measurementId: "G-LG8ZRN9EWD"
};

const firebaseApp = initializeApp(firebaseConfig);
const database = getDatabase(firebaseApp);
const dataRef = ref(database, "ESP32_Data");

let allSensorData = {};
let selectedDate = "";
let sensorChart = null;

const statusDot = document.getElementById("statusDot");
const statusText = document.getElementById("statusText");
const currentTemperature = document.getElementById("currentTemperature");
const currentHumidity = document.getElementById("currentHumidity");
const graphDate = document.getElementById("graphDate");
const historyDate = document.getElementById("historyDate");
const historyBody = document.getElementById("historyTableBody");
const recordCount = document.getElementById("recordCount");
const toggleHistory = document.getElementById("toggleHistory");
const historyContent = document.getElementById("historyContent");
const liveClock = document.getElementById("liveClock");

function setStatus(text, state) {
    statusText.textContent = text;
    statusDot.classList.remove("active", "offline", "error");
    statusDot.classList.add(state);
}

function toNumber(value) {
    const number = Number(value);
    return Number.isFinite(number) ? number : null;
}

function formatNumber(value) {
    return value === null || value === undefined
        ? "--"
        : Number(value).toFixed(1);
}

function updateClock() {
    liveClock.textContent = new Date().toLocaleTimeString(
        "en-PH",
        { hour12: false }
    );
}

updateClock();
setInterval(updateClock, 1000);

function getLatestReading(data) {
    let latest = null;

    const dates = Object.keys(data || {}).sort();

    for (const date of dates) {
        const times = Object.keys(data[date] || {}).sort();

        for (const time of times) {
            const reading = data[date][time];

            if (!reading || typeof reading !== "object") {
                continue;
            }

            const temperature = toNumber(reading.temperature);
            const humidity = toNumber(reading.humidity);

            if (temperature === null && humidity === null) {
                continue;
            }

            latest = {
                date,
                time,
                temperature,
                humidity
            };
        }
    }

    return latest;
}

function getDateList(data) {
    return Object.keys(data || {})
        .filter(key => data[key] && typeof data[key] === "object")
        .sort()
        .reverse();
}

function populateDateSelect() {
    const dates = getDateList(allSensorData);

    [graphDate, historyDate].forEach(select => {
        if (!select) return;

        select.innerHTML = "";

        if (dates.length === 0) {
            const option = document.createElement("option");
            option.value = "";
            option.textContent = "No dates available";
            select.appendChild(option);
            return;
        }

        dates.forEach(date => {
            const option = document.createElement("option");
            option.value = date;
            option.textContent = date;
            select.appendChild(option);
        });

        select.value = selectedDate || dates[0];
    });
}

function updateCurrentReading() {
    const latest = getLatestReading(allSensorData);

    if (!latest) {
        currentTemperature.textContent = "-- °C";
        currentHumidity.textContent = "-- %";
        return;
    }

    currentTemperature.textContent =
        formatNumber(latest.temperature) + " °C";

    currentHumidity.textContent =
        formatNumber(latest.humidity) + " %";
}

function getReadingsForDate(date) {
    const result = [];

    if (!date) return result;

    const dayData = allSensorData[date];

    if (!dayData || typeof dayData !== "object") {
        return result;
    }

    Object.keys(dayData)
        .filter(time =>
            dayData[time] &&
            typeof dayData[time] === "object"
        )
        .sort()
        .forEach(time => {
            const reading = dayData[time];

            const temperature = toNumber(reading.temperature);
            const humidity = toNumber(reading.humidity);

            if (temperature === null && humidity === null) {
                return;
            }

            result.push({
                time,
                temperature,
                humidity
            });
        });

    return result;
}

function updateChart() {
    if (typeof Chart === "undefined") {
        console.error("Chart.js is not loaded.");
        return;
    }

    const date = graphDate.value || selectedDate;
    const readings = getReadingsForDate(date);

    const canvas = document.getElementById("sensorChart");

    if (!canvas) return;

    if (sensorChart) {
        sensorChart.destroy();
        sensorChart = null;
    }

    sensorChart = new Chart(canvas, {
        type: "line",

        data: {
            labels: readings.map(item => item.time),

            datasets: [
                {
                    label: "Temperature (°C)",
                    data: readings.map(item => item.temperature),
                    yAxisID: "temperature",
                    borderColor: "#22d3ee",
                    backgroundColor: "rgba(34,211,238,.08)",
                    tension: 0.3,
                    borderWidth: 2,
                    pointRadius: 3,
                    pointBackgroundColor: "#22d3ee"
                },
                {
                    label: "Humidity (%)",
                    data: readings.map(item => item.humidity),
                    yAxisID: "humidity",
                    borderColor: "#60a5fa",
                    backgroundColor: "rgba(96,165,250,.08)",
                    tension: 0.3,
                    borderWidth: 2,
                    pointRadius: 3,
                    pointBackgroundColor: "#60a5fa"
                }
            ]
        },

        options: {
            responsive: true,
            maintainAspectRatio: false,

            interaction: {
                mode: "index",
                intersect: false
            },

            plugins: {
                legend: {
                    labels: {
                        color: "#94a3b8",
                        font: {
                            family: "monospace"
                        }
                    }
                }
            },

            scales: {
                x: {
                    ticks: {
                        color: "#475569",
                        maxRotation: 45,
                        minRotation: 0
                    },
                    grid: {
                        color: "rgba(23,32,51,.8)"
                    }
                },

                temperature: {
                    type: "linear",
                    position: "left",
                    ticks: {
                        color: "#22d3ee"
                    },
                    title: {
                        display: true,
                        text: "Temperature (°C)",
                        color: "#22d3ee"
                    },
                    grid: {
                        color: "rgba(23,32,51,.8)"
                    }
                },

                humidity: {
                    type: "linear",
                    position: "right",
                    ticks: {
                        color: "#60a5fa"
                    },
                    title: {
                        display: true,
                        text: "Humidity (%)",
                        color: "#60a5fa"
                    },
                    grid: {
                        drawOnChartArea: false
                    }
                }
            }
        }
    });
}

function updateHistory() {
    if (!historyBody) return;

    const date = historyDate.value || selectedDate;
    const readings = getReadingsForDate(date);

    historyBody.innerHTML = "";

    if (readings.length === 0) {
        const row = document.createElement("tr");
        row.innerHTML = `
            <td colspan="4" class="empty-state">
                NO SENSOR DATA AVAILABLE
            </td>
        `;
        historyBody.appendChild(row);
        recordCount.textContent = "0 records";
        return;
    }

    readings.slice().reverse().forEach((reading, index) => {
        const row = document.createElement("tr");

        row.innerHTML = `
            <td>${index + 1}</td>
            <td>${reading.time}</td>
            <td>${formatNumber(reading.temperature)} °C</td>
            <td>${formatNumber(reading.humidity)} %</td>
        `;

        historyBody.appendChild(row);
    });

    recordCount.textContent =
        `${readings.length} ${
            readings.length === 1 ? "record" : "records"
        }`;
}

function updateDashboard() {
    const dates = getDateList(allSensorData);

    if (dates.length === 0) {
        currentTemperature.textContent = "-- °C";
        currentHumidity.textContent = "-- %";

        historyBody.innerHTML = `
            <tr>
                <td colspan="4" class="empty-state">
                    NO DATA AVAILABLE
                </td>
            </tr>
        `;

        recordCount.textContent = "0 records";
        return;
    }

    if (!selectedDate || !dates.includes(selectedDate)) {
        selectedDate = dates[0];
    }

    populateDateSelect();
    updateCurrentReading();
    updateChart();
    updateHistory();
}

setStatus("CONNECTING", "offline");

onValue(
    dataRef,

    snapshot => {
        allSensorData = snapshot.val() || {};
        setStatus("ONLINE", "active");
        updateDashboard();
    },

    error => {
        console.error("Firebase read error:", error);
        setStatus("ERROR", "error");
    }
);

graphDate.addEventListener("change", function () {
    selectedDate = this.value;
    historyDate.value = selectedDate;
    updateChart();
    updateHistory();
});

historyDate.addEventListener("change", function () {
    selectedDate = this.value;
    graphDate.value = selectedDate;
    updateHistory();
    updateChart();
});

toggleHistory.addEventListener("click", function () {
    const hidden = historyContent.classList.contains("hidden");

    if (hidden) {
        historyContent.classList.remove("hidden");
        toggleHistory.textContent = "HIDE HISTORY";
        updateHistory();
    } else {
        historyContent.classList.add("hidden");
        toggleHistory.textContent = "SHOW HISTORY";
    }
});
