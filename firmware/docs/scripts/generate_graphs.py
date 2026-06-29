import matplotlib.pyplot as plt
import numpy as np
import os

# Create images directory if not exists
os.makedirs('docs/images', exist_ok=True)

# 1. CPU Utilization Bar Chart
tasks = ['Heart Rate', 'Body Temp', 'Motion', 'Display', 'API/WiFi', 'RTC', 'OTA']
cpu_usage = [35, 15, 20, 10, 12, 3, 5]

plt.figure(figsize=(10, 6))
bars = plt.bar(tasks, cpu_usage, color='skyblue')
plt.title('Simulated FreeRTOS Task CPU Utilization')
plt.xlabel('Tasks')
plt.ylabel('CPU Utilization (%)')
plt.ylim(0, 50)
for bar in bars:
    yval = bar.get_height()
    plt.text(bar.get_x() + bar.get_width()/2, yval + 1, f'{yval}%', ha='center', va='bottom')
plt.savefig('docs/images/cpu_utilization.png', dpi=300)
plt.close()

# 2. Simulated Heart Rate and SpO2 Data
time = np.arange(0, 60, 1) # 60 seconds
hr = 75 + 5 * np.sin(0.2 * time) + np.random.normal(0, 2, len(time))
spo2 = 98 + np.random.normal(0, 0.5, len(time))
spo2 = np.clip(spo2, 90, 100)

fig, ax1 = plt.subplots(figsize=(10, 6))

color = 'tab:red'
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Heart Rate (bpm)', color=color)
ax1.plot(time, hr, color=color, label='Heart Rate')
ax1.tick_params(axis='y', labelcolor=color)

ax2 = ax1.twinx()  
color = 'tab:blue'
ax2.set_ylabel('SpO2 (%)', color=color)  
ax2.plot(time, spo2, color=color, linestyle='--', label='SpO2')
ax2.tick_params(axis='y', labelcolor=color)

fig.tight_layout()  
plt.title('Real-time Heart Rate and SpO2 Monitoring')
plt.savefig('docs/images/hr_spo2_plot.png', dpi=300)
plt.close()

# 3. Simulated Temperature Data
temp = 36.5 + 0.1 * np.sin(0.05 * time) + np.random.normal(0, 0.05, len(time))

plt.figure(figsize=(10, 6))
plt.plot(time, temp, color='orange')
plt.title('Body Temperature Tracking (MAX30205)')
plt.xlabel('Time (s)')
plt.ylabel('Temperature (°C)')
plt.ylim(35.5, 38.0)
plt.grid(True, linestyle='--', alpha=0.7)
plt.savefig('docs/images/temperature_plot.png', dpi=300)
plt.close()

print("Graphs generated successfully.")
