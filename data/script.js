let tempChart;
let fanChart;
const maxDataPoints = 90; // Max points to display on graph
const maxRawRecords = 900; // 15 minutes history @ 1s interval
let rawHistory = []; // Buffer for raw 1s data

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

function toggleFanDatasets() {
    if (!fanChart) return;
    
    const showRpm = document.getElementById('show-rpm').checked;
    const showDuty = document.getElementById('show-duty').checked;

    fanChart.data.datasets.forEach(ds => {
        if (ds.label.includes('RPM')) {
            ds.hidden = !showRpm;
        } else if (ds.label.includes('Duty')) {
            ds.hidden = !showDuty;
        }
    });
    fanChart.update();
}

function refreshCharts(labels, tData, fData) {
    if (tempChart) {
        tempChart.data.labels = labels;
        Object.keys(tData).forEach((id, index) => {
            let dataset = tempChart.data.datasets.find(ds => ds.label === id);
            if (!dataset) {
                const colors = ['#FF6384', '#36A2EB', '#FFCE56'];
                dataset = {
                    label: id,
                    data: [],
                    borderColor: colors[index % colors.length],
                    backgroundColor: colors[index % colors.length],
                    fill: false,
                    tension: 0.4,
                    pointRadius: 0,
                    pointHoverRadius: 5
                };
                tempChart.data.datasets.push(dataset);
            }
            dataset.data = tData[id];
        });
        tempChart.update();
    }
    
    if (fanChart) {
        fanChart.data.labels = labels;
        Object.keys(fData).forEach((label) => {
             let dataset = fanChart.data.datasets.find(ds => ds.label === label);
             if (!dataset) {
                 const isRpm = label.includes('RPM');
                 const fanIndex = parseInt(label.match(/\d+/)[0]) - 1;
                 const colors = ['#4BC0C0', '#9966FF', '#FF9F40', '#C9CBCF'];
                 dataset = {
                    label: label,
                    data: [],
                    borderColor: colors[fanIndex % colors.length],
                    backgroundColor: colors[fanIndex % colors.length],
                    fill: false,
                    tension: 0.4,
                    pointRadius: 0,
                    pointHoverRadius: 5,
                    yAxisID: isRpm ? 'y' : 'y1',
                    hidden: isRpm ? !document.getElementById('show-rpm').checked : !document.getElementById('show-duty').checked
                };
                if (!isRpm) dataset.borderDash = [5, 5];
                fanChart.data.datasets.push(dataset);
             }
             dataset.data = fData[label];
        });
        fanChart.update();
    }
}

function processHistory() {
    const count = rawHistory.length;
    const target = maxDataPoints;
    
    const labels = [];
    const tData = {}; 
    const fData = {}; 
    
    if (count > 0) {
        rawHistory[0].thermistors.forEach(t => tData[t.id] = []);
        rawHistory[0].fans.forEach((f, i) => {
            fData[`Fan ${i+1} RPM`] = [];
            fData[`Fan ${i+1} Duty`] = [];
        });
    }
    
    if (count <= target) {
        rawHistory.forEach(record => {
            labels.push(record.timestamp);
            record.thermistors.forEach(t => tData[t.id].push(parseFloat(t.temp)));
            record.fans.forEach((f, i) => {
                fData[`Fan ${i+1} RPM`].push(parseInt(f.rpm));
                fData[`Fan ${i+1} Duty`].push(parseFloat(f.duty));
            });
        });
    } else {
        // Weighted resampling (Kernel Smoothing) to prevent flicker
        const ratio = (count - 1) / (target - 1);
        const radius = ratio; // Window radius

        for (let i = 0; i < target; i++) {
            const center = i * ratio;
            
            // Determine range of input indices that contribute to this output point
            const start = Math.ceil(center - radius);
            const end = Math.floor(center + radius);
            
            const safeStart = Math.max(0, start);
            const safeEnd = Math.min(count - 1, end);
            
            let totalWeight = 0;
            const tSums = {};
            const fSums = {};
            
            // Initialize sums
            Object.keys(tData).forEach(k => tSums[k] = 0);
            Object.keys(fData).forEach(k => fSums[k] = 0);

            // Accumulate weighted values
            for (let j = safeStart; j <= safeEnd; j++) {
                const weight = 1 - Math.abs(j - center) / radius;
                if (weight <= 0) continue;
                
                totalWeight += weight;
                
                const record = rawHistory[j];
                
                record.thermistors.forEach(t => {
                    tSums[t.id] += parseFloat(t.temp) * weight;
                });
                
                record.fans.forEach((f, idx) => {
                    fSums[`Fan ${idx+1} RPM`] += parseInt(f.rpm) * weight;
                    fSums[`Fan ${idx+1} Duty`] += parseFloat(f.duty) * weight;
                });
            }
            
            // Normalize and store
            if (totalWeight > 0) {
                const nearestIndex = Math.round(center);
                labels.push(rawHistory[Math.min(count-1, nearestIndex)].timestamp);
                
                Object.keys(tSums).forEach(id => {
                    tData[id].push((tSums[id] / totalWeight).toFixed(1));
                });
                
                Object.keys(fSums).forEach(label => {
                    if (label.includes('RPM')) {
                        fData[label].push(Math.round(fSums[label] / totalWeight));
                    } else {
                        fData[label].push((fSums[label] / totalWeight).toFixed(1));
                    }
                });
            }
        }
    }
    
    refreshCharts(labels, tData, fData);
}

function updateStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            // Update Text UI immediately
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

            document.getElementById('logger-output').textContent = data.logs;

            const form = document.getElementById('control-form');
            if (data.overrideEnabled) {
                form.style.display = 'block';
            } else {
                form.style.display = 'none';
            }

            // Add to history and update charts
            const now = new Date().toLocaleTimeString();
            data.timestamp = now;
            
            rawHistory.push(data);
            if (rawHistory.length > maxRawRecords) {
                rawHistory.shift();
            }
            
            processHistory();
        })
        .catch(console.error);
}

// Initialize
initCharts();
// Update every second
setInterval(updateStatus, 1000);
// Initial call
updateStatus();
