// ESP32 Entry Hub - Admin Panel JavaScript
// WebSocket connection and API handling

let ws;
let reconnectInterval;
let statusRefreshInterval;

// Initialize app
document.addEventListener('DOMContentLoaded', function() {
    initializeNavigation();
    connectWebSocket();
    loadInitialData();
    setupEventListeners();
    startStatusUpdates();
});

// Close modal when clicking outside
window.onclick = function(event) {
    const modal = document.getElementById('personSelectorModal');
    if (event.target === modal) {
        closePersonSelector();
    }
};

// Navigation
function initializeNavigation() {
    const navItems = document.querySelectorAll('.nav-item');
    
    navItems.forEach(item => {
        item.addEventListener('click', function(e) {
            e.preventDefault();
            e.stopPropagation();
            
            const page = this.getAttribute('data-page');
            navigateToPage(page);
            
            // Always close sidebar if it has 'show' class (mobile menu is open)
            const sidebar = document.getElementById('sidebar');
            if (sidebar && sidebar.classList.contains('show')) {
                closeSidebar();
            }
        }, { passive: false });
    });
}

function toggleSidebar() {
    const sidebar = document.getElementById('sidebar');
    const overlay = document.getElementById('sidebarOverlay');
    if (sidebar) sidebar.classList.toggle('show');
    if (overlay) overlay.classList.toggle('show');
}

function closeSidebar() {
    const sidebar = document.getElementById('sidebar');
    const overlay = document.getElementById('sidebarOverlay');
    if (sidebar) sidebar.classList.remove('show');
    if (overlay) overlay.classList.remove('show');
}

function navigateToPage(pageName) {
    // Update active nav item
    document.querySelectorAll('.nav-item').forEach(item => {
        item.classList.remove('active');
    });
    document.querySelector(`[data-page="${pageName}"]`).classList.add('active');
    
    // Show selected page
    document.querySelectorAll('.page').forEach(page => {
        page.classList.remove('active');
    });
    document.getElementById(`page-${pageName}`).classList.add('active');
    
    // Reload weather data when navigating to dashboard
    if (pageName === 'dashboard') {
        loadWeather();
    }
}

// WebSocket Connection
function connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.hostname}/ws`;
    
    console.log('Connecting to WebSocket:', wsUrl);
    ws = new WebSocket(wsUrl);
    
    ws.onopen = function() {
        console.log('WebSocket connected');
        updateConnectionStatus(true);
        clearInterval(reconnectInterval);
    };
    
    ws.onmessage = function(event) {
        handleWebSocketMessage(event.data);
    };
    
    ws.onerror = function(error) {
        console.error('WebSocket error:', error);
        updateConnectionStatus(false);
    };
    
    ws.onclose = function() {
        console.log('WebSocket disconnected');
        updateConnectionStatus(false);
        
        // Attempt to reconnect
        reconnectInterval = setInterval(() => {
            console.log('Attempting to reconnect...');
            connectWebSocket();
        }, 5000);
    };
}

function handleWebSocketMessage(data) {
    try {
        const message = JSON.parse(data);
        console.log('WebSocket message:', message);
        
        switch(message.type) {
            case 'status':
                updateSystemStatus(message);
                break;
            case 'voice_detected':
                handleVoiceDetection(message);
                break;
            case 'presence_update':
                updatePresenceStatus(message);
                break;
            default:
                console.log('Unknown message type:', message.type);
        }
    } catch(e) {
        console.error('Error parsing WebSocket message:', e);
    }
}

function updateConnectionStatus(connected) {
    const statusDot = document.getElementById('connectionStatus');
    const statusText = document.getElementById('connectionText');
    
    if (connected) {
        statusDot.classList.add('connected');
        statusText.textContent = 'Connected';
    } else {
        statusDot.classList.remove('connected');
        statusText.textContent = 'Disconnected';
    }
}

// API Functions
async function apiGet(endpoint) {
    try {
        const response = await fetch(`/api/${endpoint}`);
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
        return await response.json();
    } catch(e) {
        console.error(`API GET error (${endpoint}):`, e);
        return null;
    }
}

async function apiPost(endpoint, data) {
    try {
        const response = await fetch(`/api/${endpoint}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(data)
        });
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
        return await response.json();
    } catch(e) {
        console.error(`API POST error (${endpoint}):`, e);
        return null;
    }
}

// Load Initial Data
async function loadInitialData() {
    await Promise.all([
        loadSystemStatus(),
        loadCommands(),
        loadPresence(),
        loadWeather(),
        loadIntegrations()
    ]);
}

async function loadSystemStatus() {
    const status = await apiGet('status');
    if (status) {
        updateSystemStatus(status);
    }
}

function updateSystemStatus(status) {
    // System info
    if (status.device) {
        document.getElementById('uptime').textContent = formatUptime(status.device.uptime);
        document.getElementById('freeHeap').textContent = formatBytes(status.device.free_heap);
    }
    
    // WiFi info
    if (status.wifi) {
        document.getElementById('wifiSignal').textContent = `${status.wifi.rssi} dBm`;
        const wifiSSID = document.getElementById('wifiSSID');
        const ipAddress = document.getElementById('ipAddress');
        if (wifiSSID) wifiSSID.value = status.wifi.ssid;
        if (ipAddress) ipAddress.value = status.wifi.ip;
    }
    
    // Voice status
    if (status.voice) {
        document.getElementById('wakeWord').textContent = status.voice.wake_word;
        const voiceStatus = document.getElementById('voiceStatus');
        if (status.voice.active) {
            voiceStatus.textContent = 'Active';
            voiceStatus.className = 'badge badge-success';
        } else {
            voiceStatus.textContent = 'Inactive';
            voiceStatus.className = 'badge';
        }
    }
}

async function loadCommands() {
    const data = await apiGet('commands');
    if (data && data.commands) {
        renderCommandsTable(data.commands);
    } else {
        // Show sample commands
        renderCommandsTable([
            { id: 1, command: 'Turn on the lights', action: 'scene:lights_on', category: 'Lighting', enabled: true },
            { id: 2, command: 'Lock the door', action: 'scene:lock_door', category: 'Security', enabled: true },
            { id: 3, command: 'What\'s the weather', action: 'query:weather', category: 'Information', enabled: true }
        ]);
    }
}

function renderCommandsTable(commands) {
    const tbody = document.getElementById('commandsTable');
    
    if (commands.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" class="text-center">No commands configured</td></tr>';
        return;
    }
    
    tbody.innerHTML = commands.map(cmd => `
        <tr>
            <td>${cmd.command}</td>
            <td>${cmd.action}</td>
            <td><span class="badge">${cmd.category}</span></td>
            <td>${cmd.enabled ? '✅' : '❌'}</td>
            <td>
                <button class="btn btn-sm btn-secondary" onclick="editCommand(${cmd.id})">Edit</button>
                <button class="btn btn-sm btn-danger" onclick="deleteCommand(${cmd.id})">Delete</button>
            </td>
        </tr>
    `).join('');
}

async function loadPresence() {
    try {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 10000); // Increased timeout to 10s
        
        // Add timestamp to prevent caching
        const response = await fetch(`/api/presence?t=${Date.now()}`, { signal: controller.signal });
        clearTimeout(timeoutId);
        
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const data = await response.json();
        if (data && data.people) {
            renderPresence(data.people);
        } else {
            renderPresence([]);
        }
    } catch (error) {
        console.error('Failed to load presence:', error);
        renderPresence([]);
    }
}

function renderPresence(people) {
    // Quick view on dashboard
    const quickPresence = document.getElementById('presenceQuick');
    quickPresence.innerHTML = people.map(person => `
        <div class="presence-item">
            <span class="presence-avatar">${person.avatar}</span>
            <span class="presence-name">${person.name}</span>
            <span class="presence-badge ${person.present ? 'badge-success' : ''}">${person.present ? 'Home' : 'Away'}</span>
        </div>
    `).join('');
    
    // Detailed view on presence page
    const peopleList = document.getElementById('peopleList');
    if (peopleList) {
        if (people.length === 0) {
            peopleList.innerHTML = `
                <div style="padding: 2rem; text-align: center; color: var(--text-muted);">
                    <div style="font-size: 3rem; margin-bottom: 1rem;">👥</div>
                    <p>No family members tracked yet.</p>
                    <p style="font-size: 0.875rem;">Enable tracking above and click "+ Add Person" to get started.</p>
                </div>
            `;
        } else {
            peopleList.innerHTML = people.map(person => `
                <div class="person-item">
                    <div class="person-info">
                        <span class="person-avatar">${person.avatar}</span>
                        <div>
                            <div style="font-weight: 600;">${person.name}</div>
                            <div style="font-size: 0.875rem; color: var(--text-muted);">${person.location}</div>
                        </div>
                    </div>
                    <div style="display: flex; align-items: center; gap: 10px;">
                        <span class="badge ${person.present ? 'badge-success' : 'badge-warning'}">${person.present ? 'Present' : 'Away'}</span>
                        ${person.entity_id ? `<button class="btn-remove" onclick="removePerson('${person.entity_id}')" title="Remove Person">🗑️</button>` : ''}
                    </div>
                </div>
            `).join('');
        }
    }
}

function getWeatherIcon(condition) {
    if (!condition) return '☀️';
    
    const cond = condition.toLowerCase();
    
    // Map weather conditions to emojis
    if (cond.includes('clear') || cond.includes('sunny')) return '☀️';
    if (cond.includes('cloud') || cond.includes('overcast')) return '☁️';
    if (cond.includes('partly')) return '⛅';
    if (cond.includes('rain') || cond.includes('drizzle')) return '🌧️';
    if (cond.includes('thunder') || cond.includes('storm')) return '⛈️';
    if (cond.includes('snow') || cond.includes('sleet')) return '❄️';
    if (cond.includes('fog') || cond.includes('mist') || cond.includes('haze')) return '🌫️';
    if (cond.includes('wind')) return '💨';
    
    return '☀️'; // default
}

async function loadWeather() {
    try {
        const data = await apiGet('weather');
        const weatherIcon = document.querySelector('.weather-icon');
        
        if (data && !data.error && data.temperature !== undefined) {
            const temp = Math.round(data.temperature * 10) / 10; // Round to 1 decimal
            document.getElementById('weatherTemp').textContent = `${temp}°C`;
            const description = data.description || data.state || 'Unknown';
            document.getElementById('weatherCondition').textContent = description;
            
            // Update weather icon based on condition
            if (weatherIcon) {
                weatherIcon.textContent = getWeatherIcon(description);
            }
        } else {
            document.getElementById('weatherTemp').textContent = '--°C';
            document.getElementById('weatherCondition').textContent = 'Not configured';
            if (weatherIcon) {
                weatherIcon.textContent = '☀️';
            }
        }
    } catch (error) {
        console.error('Failed to load weather:', error);
        document.getElementById('weatherTemp').textContent = '--°C';
        document.getElementById('weatherCondition').textContent = 'Error';
        const weatherIcon = document.querySelector('.weather-icon');
        if (weatherIcon) {
            weatherIcon.textContent = '⚠️';
        }
    }
}

async function loadIntegrations() {
    try {
        const config = await apiGet('config');
        if (config) {
            updateIntegrationStatus(config);
            // Also load person config to ensure trackedPersons is initialized
            loadPersonConfig(config);
        }
    } catch (error) {
        console.error('Failed to load integrations:', error);
    }
}

function updateIntegrationStatus(config) {
    // Update Home Assistant status
    if (config.integrations && config.integrations.home_assistant) {
        const ha = config.integrations.home_assistant;
        const haStatusElement = document.getElementById('haStatus');
        
        if (haStatusElement) {
            if (ha.status === 'connected') {
                haStatusElement.textContent = 'Connected';
                haStatusElement.className = 'badge badge-success';
            } else if (ha.status === 'failed') {
                haStatusElement.textContent = 'Connection Failed';
                haStatusElement.className = 'badge badge-danger';
            } else if (ha.status === 'not_configured') {
                haStatusElement.textContent = 'Not Configured';
                haStatusElement.className = 'badge badge-warning';
            } else {
                haStatusElement.textContent = 'Unknown';
                haStatusElement.className = 'badge';
            }
        }
    }
    
    // Update Weather status
    if (config.weather) {
        const weatherStatusElement = document.getElementById('weatherStatus');
        
        if (weatherStatusElement) {
            if (config.weather.status === 'connected') {
                weatherStatusElement.textContent = 'Connected';
                weatherStatusElement.className = 'badge badge-success';
            } else if (config.weather.status === 'failed') {
                weatherStatusElement.textContent = 'Connection Failed';
                weatherStatusElement.className = 'badge badge-danger';
            } else if (config.weather.status === 'not_configured') {
                weatherStatusElement.textContent = 'Not Configured';
                weatherStatusElement.className = 'badge badge-warning';
            } else {
                weatherStatusElement.textContent = 'Not Configured';
                weatherStatusElement.className = 'badge badge-warning';
            }
        }
    }
}

// Event Handlers
function setupEventListeners() {
    // Sensitivity slider
    const slider = document.getElementById('sensitivitySlider');
    if (slider) {
        slider.addEventListener('input', (e) => {
            document.getElementById('sensitivityValue').textContent = e.target.value;
        });
    }
}

function startStatusUpdates() {
    // Load initial status immediately
    loadSystemStatus();
    
    // Only use periodic fallback if WebSocket is disconnected
    statusRefreshInterval = setInterval(() => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            loadSystemStatus();
        }
    }, 10000); // Slower fallback - only when WebSocket is down
}

// Actions
async function triggerScene(sceneName) {
    console.log('Triggering scene:', sceneName);
    const result = await apiPost('scene', { scene: sceneName });
    if (result && result.success) {
        showNotification('Scene activated successfully', 'success');
    } else {
        showNotification('Failed to activate scene', 'error');
    }
}

async function saveWakeWordSettings() {
    const wakeWord = document.getElementById('wakeWordSelect').value;
    const sensitivity = document.getElementById('sensitivitySlider').value;
    
    const result = await apiPost('config', {
        wake_word: wakeWord,
        sensitivity: sensitivity / 100
    });
    
    if (result && result.success) {
        showNotification('Settings saved successfully', 'success');
    } else {
        showNotification('Failed to save settings', 'error');
    }
}

function showAddCommandModal() {
    // TODO: Implement modal for adding commands
    alert('Add Command feature coming soon!');
}

function showAddPersonModal() {
    // TODO: Implement modal for adding people
    alert('Add Person feature coming soon!');
}

function editCommand(id) {
    console.log('Editing command:', id);
    alert('Edit command feature coming soon!');
}

async function deleteCommand(id) {
    if (confirm('Are you sure you want to delete this command?')) {
        const result = await apiPost('commands/delete', { id });
        if (result && result.success) {
            showNotification('Command deleted', 'success');
            loadCommands();
        }
    }
}

function resetWiFi() {
    if (confirm('This will reset WiFi settings and restart the device. Continue?')) {
        apiPost('config/reset-wifi', {});
        showNotification('WiFi settings reset. Device restarting...', 'warning');
    }
}

function rebootDevice() {
    if (confirm('Reboot the device now?')) {
        apiPost('system/reboot', {});
        showNotification('Device rebooting...', 'warning');
    }
}

function factoryReset() {
    if (confirm('WARNING: This will erase all settings! Are you sure?')) {
        if (confirm('Really? All configuration will be lost!')) {
            apiPost('system/factory-reset', {});
            showNotification('Factory reset initiated...', 'danger');
        }
    }
}

function exportConfig() {
    window.location.href = '/api/config/export';
}

function handleVoiceDetection(message) {
    document.getElementById('lastDetection').textContent = 'Just now';
    showNotification(`Wake word detected: ${message.wake_word}`, 'success');
}

function updatePresenceStatus(message) {
    showNotification(`${message.person} is now ${message.present ? 'home' : 'away'}`, 'info');
    loadPresence();
}

// Utilities
function formatUptime(seconds) {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    
    if (days > 0) return `${days}d ${hours}h`;
    if (hours > 0) return `${hours}h ${minutes}m`;
    return `${minutes}m`;
}

function formatBytes(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1048576).toFixed(1) + ' MB';
}

function showNotification(message, type = 'info') {
    console.log(`[${type.toUpperCase()}] ${message}`);
    
    // Create toast element
    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    toast.textContent = message;
    toast.style.cssText = `
        position: fixed;
        bottom: 20px;
        right: 20px;
        padding: 12px 20px;
        background: ${type === 'success' ? '#10b981' : type === 'error' ? '#ef4444' : type === 'warning' ? '#f59e0b' : '#3b82f6'};
        color: white;
        border-radius: 8px;
        box-shadow: 0 4px 12px rgba(0,0,0,0.3);
        z-index: 10000;
        animation: slideIn 0.3s ease-out;
        max-width: 400px;
    `;
    
    document.body.appendChild(toast);
    
    // Auto-remove after 4 seconds
    setTimeout(() => {
        toast.style.animation = 'slideOut 0.3s ease-in';
        setTimeout(() => toast.remove(), 300);
    }, 4000);
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (ws) ws.close();
    clearInterval(statusRefreshInterval);
    clearInterval(reconnectInterval);
});

// Weather Configuration
function toggleWeatherProvider() {
    const provider = document.getElementById('weatherProvider').value;
    const noneSettings = document.getElementById('noneSettings');
    const owmSettings = document.getElementById('owmSettings');
    const haWeatherSettings = document.getElementById('haWeatherSettings');
    
    noneSettings.style.display = provider === 'none' ? 'block' : 'none';
    owmSettings.style.display = provider === 'openweathermap' ? 'block' : 'none';
    haWeatherSettings.style.display = provider === 'homeassistant' ? 'block' : 'none';
}

async function loadWeatherConfig() {
    try {
        const response = await fetch('/api/config');
        const config = await response.json();
        
        // Set weather provider
        const provider = config.weather?.provider || 'homeassistant';
        document.getElementById('weatherProvider').value = provider;
        toggleWeatherProvider(); // Ensure UI updates based on default
        
        // OpenWeatherMap settings
        if (config.weather?.openweathermap) {
            document.getElementById('owmApiKey').value = config.weather.openweathermap.api_key || '';
            document.getElementById('owmCity').value = config.weather.openweathermap.city || '';
            document.getElementById('owmUnits').value = config.weather.openweathermap.units || 'metric';
        }
        
        // Home Assistant weather settings
        if (config.weather?.home_assistant) {
            document.getElementById('haWeatherEntity').value = config.weather.home_assistant.entity_id || 'weather.forecast_home';
        } else {
            document.getElementById('haWeatherEntity').value = 'weather.forecast_home';
        }
        
        // Home Assistant integration settings
        if (config.integrations?.home_assistant) {
            document.getElementById('haUrl').value = config.integrations.home_assistant.url || '';
            document.getElementById('haToken').value = config.integrations.home_assistant.token || '';
        }
        
        // Person tracking settings
        try {
            loadPersonConfig(config);
        } catch (error) {
            console.error('Failed to load person config:', error);
        }
        
        toggleWeatherProvider();
        updateWeatherStatus(provider);
    } catch (error) {
        console.error('Failed to load weather config:', error);
        showNotification('Failed to load configuration', 'error');
    }
}

function updateWeatherStatus(provider) {
    const statusBadge = document.getElementById('weatherStatus');
    if (provider === 'none') {
        statusBadge.className = 'badge badge-warning';
        statusBadge.textContent = 'Not Configured';
    } else {
        statusBadge.className = 'badge badge-success';
        statusBadge.textContent = 'Configured';
    }
}

async function saveWeatherSettings() {
    const provider = document.getElementById('weatherProvider').value;
    
    const weatherConfig = {
        provider: provider
    };
    
    if (provider === 'openweathermap') {
        weatherConfig.openweathermap = {
            api_key: document.getElementById('owmApiKey').value,
            city: document.getElementById('owmCity').value,
            units: document.getElementById('owmUnits').value
        };
        
        if (!weatherConfig.openweathermap.api_key || !weatherConfig.openweathermap.city) {
            showNotification('Please fill in API key and city', 'error');
            return;
        }
    } else if (provider === 'homeassistant') {
        weatherConfig.home_assistant = {
            entity_id: document.getElementById('haWeatherEntity').value || 'weather.forecast_home'
        };
    }
    
    try {
        const response = await fetch('/api/config/weather', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(weatherConfig)
        });
        
        if (response.ok) {
            showNotification('Weather settings saved successfully', 'success');
            updateWeatherStatus(provider);
            // Reload config to ensure UI reflects saved state
            setTimeout(() => loadWeatherConfig(), 500);
        } else {
            showNotification('Failed to save weather settings', 'error');
        }
    } catch (error) {
        console.error('Failed to save weather settings:', error);
        showNotification('Failed to save weather settings', 'error');
    }
}

async function saveHomeAssistant() {
    const haConfig = {
        url: document.getElementById('haUrl').value,
        token: document.getElementById('haToken').value,
        enabled: true,
        discovery: true
    };
    
    if (!haConfig.url) {
        showNotification('Please enter Home Assistant URL', 'error');
        return;
    }
    
    try {
        const response = await fetch('/api/config/homeassistant', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(haConfig)
        });
        
        if (response.ok) {
            showNotification('Home Assistant settings saved successfully', 'success');
            // Auto-test connection after save
            setTimeout(() => testHomeAssistantConnection(), 500);
        } else {
            showNotification('Failed to save Home Assistant settings', 'error');
        }
    } catch (error) {
        console.error('Failed to save HA settings:', error);
        showNotification('Failed to save Home Assistant settings', 'error');
    }
}

async function testHomeAssistantConnection() {
    try {
        const response = await fetch('/api/homeassistant/test');
        const data = await response.json();
        
        const statusBadge = document.getElementById('haStatus');
        const connectionInfo = document.getElementById('haConnectionInfo');
        
        if (data.connected) {
            statusBadge.className = 'badge badge-success';
            statusBadge.textContent = 'Connected';
            
            document.getElementById('haVersion').textContent = data.version || '--';
            document.getElementById('haLocation').textContent = data.location_name || '--';
            connectionInfo.style.display = 'block';
            
            showNotification('Connected to Home Assistant v' + data.version, 'success');
        } else {
            statusBadge.className = 'badge badge-warning';
            statusBadge.textContent = 'Not Connected';
            connectionInfo.style.display = 'none';
            
            const errorMsg = data.error || 'Connection failed';
            showNotification('Home Assistant: ' + errorMsg, 'error');
        }
    } catch (error) {
        console.error('Failed to test HA connection:', error);
        const statusBadge = document.getElementById('haStatus');
        statusBadge.className = 'badge badge-danger';
        statusBadge.textContent = 'Error';
        document.getElementById('haConnectionInfo').style.display = 'none';
        showNotification('Failed to test Home Assistant connection', 'error');
    }
}

// Person Tracking Functions
let trackedPersons = [];

function loadPersonConfig(config) {
    const entityIds = config.presence?.home_assistant?.entity_ids || [];
    trackedPersons = entityIds;
}

function renderTrackedPersons() {
    // Removed
}

function getPersonAvatar(name) {
    const lowerName = name.toLowerCase();
    if (lowerName.includes('john') || lowerName.includes('dad') || lowerName.includes('father')) return '👨';
    if (lowerName.includes('jane') || lowerName.includes('mom') || lowerName.includes('mother')) return '👩';
    if (lowerName.includes('kid') || lowerName.includes('child') || lowerName.includes('son') || lowerName.includes('daughter')) return '👶';
    if (lowerName.includes('boy')) return '👦';
    if (lowerName.includes('girl')) return '👧';
    return '👤';
}

function updatePersonStatus(enabled, count) {
    // Removed
}

async function showPersonSelector() {
    const modal = document.getElementById('personSelectorModal');
    modal.style.display = 'block';
    
    const container = document.getElementById('availablePersonsList');
    container.innerHTML = '<p class="help-text">Loading available persons from Home Assistant...</p>';
    
    try {
        const response = await fetch('/api/homeassistant/persons');
        const data = await response.json();
        
        if (data.error) {
            container.innerHTML = `<p class="help-text" style="color: var(--danger);">${data.error}</p>`;
            return;
        }
        
        if (!data.persons || data.persons.length === 0) {
            container.innerHTML = '<p class="help-text">No person entities found in Home Assistant.</p>';
            return;
        }
        
        container.innerHTML = data.persons.map(person => {
            const isTracked = trackedPersons.includes(person.entity_id);
            const name = person.name || person.entity_id.replace('person.', '').replace(/_/g, ' ');
            const avatar = getPersonAvatar(name);
            
            return `
                <div class="available-person-item ${isTracked ? 'disabled' : ''}" 
                     onclick="${isTracked ? '' : `addPerson('${person.entity_id}', '${name}')`}">
                    <div class="available-person-info">
                        <span class="available-person-avatar">${avatar}</span>
                        <div>
                            <div style="font-weight: 600;">${name.charAt(0).toUpperCase() + name.slice(1)}</div>
                            <div style="font-size: 0.875rem; color: var(--text-muted);">${person.entity_id}</div>
                        </div>
                    </div>
                    ${isTracked ? '<span class="badge badge-success">✓ Already tracked</span>' : '<span style="color: var(--text-muted);">Click to add</span>'}
                </div>
            `;
        }).join('');
    } catch (error) {
        console.error('Failed to load persons:', error);
        container.innerHTML = '<p class="help-text" style="color: var(--danger);">Failed to load persons. Make sure Home Assistant is configured.</p>';
    }
}

function closePersonSelector() {
    document.getElementById('personSelectorModal').style.display = 'none';
    // Reload presence when closing modal to ensure list is up to date
    loadPresence();
}

async function addPerson(entityId, name) {
    if (trackedPersons.includes(entityId)) {
        return;
    }
    
    trackedPersons.push(entityId);
    
    await savePersonSettings();
    
    // Update the clicked item in the modal without reloading from API
    const container = document.getElementById('availablePersonsList');
    const items = container.querySelectorAll('.available-person-item');
    items.forEach(item => {
        if (item.querySelector(`[onclick*="${entityId}"]`) || item.innerHTML.includes(entityId)) {
            const avatar = getPersonAvatar(name);
            item.classList.add('disabled');
            item.setAttribute('onclick', '');
            item.innerHTML = `
                <div class="available-person-info">
                    <span class="available-person-avatar">${avatar}</span>
                    <div>
                        <div style="font-weight: 600;">${name.charAt(0).toUpperCase() + name.slice(1)}</div>
                        <div style="font-size: 0.875rem; color: var(--text-muted);">${entityId}</div>
                    </div>
                </div>
                <span class="badge badge-success">✓ Already tracked</span>
            `;
        }
    });
    
    showNotification(`Added ${name} to tracking`, 'success');
    
    // Refresh dashboard presence and people list
    loadPresence();
}

async function removePerson(entityId) {
    trackedPersons = trackedPersons.filter(id => id !== entityId);
    await savePersonSettings();
    // renderTrackedPersons(); // Removed
    
    const name = entityId.replace('person.', '').replace(/_/g, ' ');
    showNotification(`Removed ${name} from tracking`, 'success');
    
    // Refresh dashboard presence
    loadPresence();
}

async function savePersonSettings() {
    // Load full config
    try {
        const response = await fetch('/api/config');
        const config = await response.json();
        
        // Update presence section
        config.presence = {
            enabled: true, // Always enabled if using this feature
            home_assistant: {
                entity_ids: trackedPersons
            }
        };
        
        // Save full config
        const saveResponse = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });
        
        if (saveResponse.ok) {
            // showNotification('Person tracking settings saved successfully', 'success'); // Too verbose for every add/remove
            // updatePersonStatus(true, trackedPersons.length); // Removed
            // Reload presence data
            loadPresence();
        } else {
            showNotification('Failed to save person tracking settings', 'error');
        }
    } catch (error) {
        console.error('Failed to save person settings:', error);
        showNotification('Failed to save person tracking settings', 'error');
    }
}

// Calendar Functions

async function testWeather() {
    try {
        const response = await fetch('/api/weather');
        const data = await response.json();
        
        if (data.configured === false) {
            showNotification('Weather not configured yet', 'warning');
        } else if (data.error) {
            let errorMsg = data.error;
            // Add helpful hints for common errors
            if (data.code === 401 || data.code === 403) {
                errorMsg = 'Home Assistant authentication failed. Please check your access token in the Home Assistant section above.';
            } else if (data.code === 404) {
                errorMsg = 'Weather entity not found. Please verify the entity ID exists in Home Assistant.';
            } else if (data.code < 0) {
                errorMsg = 'Cannot connect to Home Assistant. Please check the URL in the Home Assistant section above.';
            } else if (data.error.includes('not configured')) {
                errorMsg = data.error + '. Please configure Home Assistant URL and access token above.';
            }
            showNotification(errorMsg, 'error');
        } else {
            let message = 'Weather test successful! ';
            if (data.temperature !== undefined) {
                message += `Temperature: ${data.temperature}°C`;
            }
            showNotification(message, 'success');
            console.log('Weather data:', data);
        }
    } catch (error) {
        console.error('Failed to test weather:', error);
        showNotification('Failed to test weather connection', 'error');
    }
}

// Load weather config when navigating to integrations page
const originalNavigateToPage = navigateToPage;
navigateToPage = function(pageName) {
    originalNavigateToPage(pageName);
    if (pageName === 'integrations') {
        loadWeatherConfig();
    }
};
