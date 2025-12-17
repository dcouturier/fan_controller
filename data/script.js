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

// Update every second
setInterval(updateStatus, 1000);
// Initial call
updateStatus();
