const eventSource = new EventSource('/api/stream');
const connectionStatus = document.getElementById('connection-status');
const pulseDot = document.querySelector('.pulse-dot');

const hrValue = document.getElementById('hr-value');
const spo2Value = document.getElementById('spo2-value');
const tempValue = document.getElementById('temp-value');
const tempFValue = document.getElementById('temp-f-value');
const fallValue = document.getElementById('fall-value');
const accelValue = document.getElementById('accel-value');

const uptimeValue = document.getElementById('uptime-value');
const rssiValue = document.getElementById('rssi-value');
const timeValue = document.getElementById('time-value');

const fallCard = fallValue.closest('.card');

eventSource.onopen = () => {
    connectionStatus.textContent = "Connected. Waiting for device...";
};

eventSource.onmessage = (event) => {
    const data = JSON.parse(event.data);
    
    // Update Connection Status
    connectionStatus.textContent = "Receiving Data";
    pulseDot.classList.add('active');
    pulseDot.style.animation = 'pulse-green 1.5s infinite';

    // Update Heart Rate
    hrValue.textContent = (data.hrValid && data.heartRate > 0) ? data.heartRate : '--';
    spo2Value.textContent = (data.spo2Valid && data.spO2 > 0) ? data.spO2 : '--';

    // Update Temp
    tempValue.textContent = data.bodyTempValid ? data.bodyTempC : '--';
    tempFValue.textContent = data.bodyTempValid ? data.bodyTempF : '--';

    // Update Motion
    accelValue.textContent = data.accelMag;
    if (data.fallDetected) {
        fallValue.textContent = "FALL DETECTED!";
        fallValue.style.color = "#ef4444";
        fallCard.classList.add('fall-alert');
    } else {
        fallValue.textContent = "Safe";
        fallValue.style.color = "inherit";
        fallCard.classList.remove('fall-alert');
    }

    // Update Meta
    uptimeValue.textContent = Math.floor(data.uptime / 1000);
    rssiValue.textContent = data.wifiRSSI;
    timeValue.textContent = data.time || new Date().toLocaleTimeString();
};

eventSource.onerror = () => {
    connectionStatus.textContent = "Disconnected from server";
    pulseDot.classList.remove('active');
    pulseDot.style.animation = 'pulse 1.5s infinite';
};
