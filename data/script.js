let tempChart;
let fanChart;
const maxDataPoints = 900; // 15 minutes history

function initCharts() {
    // Temperature Chart
    const tempCtx = document.getElementById('tempChart').getContext('2d');
    tempChart = new Chart(tempCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: []
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: {
                    display: true,
                    title: { display: true, text: 'Time' }
                },
                y: {
                    display: true,
                    title: { display: true, text: 'Temperature (°C)' },
                    suggestedMin: 20,
                    suggestedMax: 40
                }
            },
            animation: false,
            interaction: {
                mode: 'index',
                intersect: false,
            },
        }
    });

    // Fan Chart
    const fanCtx = document.getElementById('fanChart').getContext('2d');
    fanChart = new Chart(fanCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: []
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: {
                    display: true,
                    title: { display: true, text: 'Time' }
                },
                y: {
                    type: 'linear',
                    display: true,
                    position: 'left',
                    title: { display: true, text: 'Fan Speed (RPM)' },
                    suggestedMin: 0,
                    suggestedMax: 2000
                },
                y1: {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    title: { display: true, text: 'Duty Cycle (%)' },
                    min: 0,
                    max: 100,
                    grid: {
                        drawOnChartArea: false,
                    },
                }
            },
            animation: false,
            interaction: {
                mode: 'index',
                intersect: false,
            },
        }
    });
}

function updateCharts(thermistors, fans) {
    const now = new Date().toLocaleTimeString();
    
    // --- Update Temp Chart ---
    if (tempChart) {
        // Add label
        tempChart.data.labels.push(now);
        if (tempChart.data.labels.length > maxDataPoints) {
            tempChart.data.labels.shift();
        }

        // Update thermistor datasets
        thermistors.forEach((t, index) => {
            let dataset = tempChart.data.datasets.find(ds => ds.label === t.id);
            if (!dataset) {
                const colors = ['#FF6384', '#36A2EB', '#FFCE56'];
                dataset = {
                    label: t.id,
                    data: [],
                    borderColor: colors[index % colors.length],
                    backgroundColor: colors[index % colors.length],
                    fill: false,
                    tension: 0.1
                };
                tempChart.data.datasets.push(dataset);
            }
            const tempVal = parseFloat(t.temp);
            dataset.data.push(isNaN(tempVal) ? null : tempVal);
            if (dataset.data.length > maxDataPoints) dataset.data.shift();
        });
        tempChart.update();
    }

    // --- Update Fan Chart ---
    if (fanChart) {
        // Add label
        fanChart.data.labels.push(now);
        if (fanChart.data.labels.length > maxDataPoints) {
            fanChart.data.labels.shift();
        }

        // Update fan datasets
        fans.forEach((f, index) => {
            const colors = ['#4BC0C0', '#9966FF', '#FF9F40', '#C9CBCF'];
            
            // RPM
            const labelRpm = `Fan ${index + 1} RPM`;
            let datasetRpm = fanChart.data.datasets.find(ds => ds.label === labelRpm);
            if (!datasetRpm) {
                datasetRpm = {
                    label: labelRpm,
                    data: [],
                    borderColor: colors[index % colors.length],
                    backgroundColor: colors[index % colors.length],
                    fill: false,
                    tension: 0.1,
                    yAxisID: 'y'
                };
                fanChart.data.datasets.push(datasetRpm);
            }
            const rpmVal = parseInt(f.rpm);
            datasetRpm.data.push(isNaN(rpmVal) ? null : rpmVal);
            if (datasetRpm.data.length > maxDataPoints) datasetRpm.data.shift();

            // Duty Cycle
            const labelDuty = `Fan ${index + 1} Duty`;
            let datasetDuty = fanChart.data.datasets.find(ds => ds.label === labelDuty);
            if (!datasetDuty) {
                datasetDuty = {
                    label: labelDuty,
                    data: [],
                    borderColor: colors[index % colors.length],
                    backgroundColor: colors[index % colors.length],
                    fill: false,
                    tension: 0.1,
                    borderDash: [5, 5],
                    yAxisID: 'y1'
                };
                fanChart.data.datasets.push(datasetDuty);
            }
            const dutyVal = parseFloat(f.duty);
            datasetDuty.data.push(isNaN(dutyVal) ? null : dutyVal);
            if (datasetDuty.data.length > maxDataPoints) datasetDuty.data.shift();
        });
        fanChart.update();
    }
}

function updateStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            // Update Temperatures
            const tempGrid = document.getElementById('temp-grid');
            tempGrid.innerHTML = '';
            data.thermistors.forEach(t => {
                tempGrid.innerHTML += `
                    <div class="fan-card">
                        <h3>${t.id}</h3>
                        <div class="fan-status">
                            <strong>Temp:</strong> ${t.temp} &deg;C
                        </div>
                    </div>`;
            });

            // Update Chart
            updateCharts(data.thermistors, data.fans);

            // Update Fans
            const fanGrid = document.getElementById('fan-grid');
            fanGrid.innerHTML = '';
            data.fans.forEach((f, index) => {
                fanGrid.innerHTML += `
                    <div class="fan-card">
                        <h3>Fan ${index + 1}</h3>
                        <div class="fan-status">
                            <strong>Duty Cycle:</strong> ${f.duty}%<br>
                            <strong>RPM:</strong> ${f.rpm}
                        </div>
                    </div>`;
            });

            // Update Logger
            document.getElementById('logger-output').textContent = data.logs;

            // Show/Hide Control Form
            const form = document.getElementById('control-form');
            if (data.overrideEnabled) {
                form.style.display = 'block';
            } else {
                form.style.display = 'none';
            }
            
            // Update Form Values (optional, but good for sync)
            // This assumes the form inputs have ids like 'fan1', 'fan2', etc.
            // and we want to show the current duty cycle as the default value.
            // However, the user might be typing, so updating the input value might be annoying.
            // Let's skip updating the input values automatically for now.
        })
        .catch(console.error);
}

// Initialize
initCharts();
// Update every second
setInterval(updateStatus, 1000);
// Initial call
updateStatus();
