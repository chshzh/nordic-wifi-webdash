/*
 * Nordic WiFi Web Dashboard — Main JavaScript (v2.0)
 */

// Configuration
const API_BASE = '';
const REFRESH_INTERVAL = 500; // ms

const BUTTON_PRESSED_COLOR  = '#4caf50';
const BUTTON_RELEASED_COLOR = '#757575';

// Board display names keyed by CONFIG_BOARD string
const BOARD_NAMES = {
    'nrf7002dk_nrf5340_cpuapp':           'nRF7002DK',
    'nrf54lm20dk_nrf54lm20a_cpuapp':      'nRF54LM20DK + nRF7002EBII',
    'nrf5340dk_nrf5340_cpuapp':           'nRF5340DK',
    'nrf9160dk_nrf9160_ns':               'nRF9160DK',
};

function boardLabel(raw) {
    return BOARD_NAMES[raw] || raw;
}

// Mode banner colors per Wi-Fi mode
const MODE_COLORS = {
    'SoftAP': '#1565c0', // blue
    'STA':    '#2e7d32', // green
    'P2P':    '#6a1b9a', // purple
};

// State
let updateInterval    = null;
let buttonGrid        = null;
let buttonTemplate    = null;
let buttonPlaceholder = null;
let ledGrid           = null;
let ledTemplate       = null;
let ledPlaceholder    = null;

const buttonElements     = new Map();
const ledElements        = new Map();
let availableLedNumbers  = [];

// Initialize on page load
document.addEventListener('DOMContentLoaded', function () {
    console.log('Nordic WiFi Web Dashboard v2.0 initialized');

    buttonGrid        = document.getElementById('button-grid');
    buttonTemplate    = document.getElementById('button-template');
    buttonPlaceholder = document.getElementById('button-placeholder');
    ledGrid           = document.getElementById('led-grid');
    ledTemplate       = document.getElementById('led-template');
    ledPlaceholder    = document.getElementById('led-placeholder');

    startAutoUpdate();
    updateSystemInfo();
    updateButtonStates();
    updateLEDStates();
});

// Start automatic updates
function startAutoUpdate() {
    if (updateInterval) {
        clearInterval(updateInterval);
    }

    updateInterval = setInterval(function () {
        updateSystemInfo();
        updateButtonStates();
        updateLEDStates();
    }, REFRESH_INTERVAL);

    console.log('Auto-update started');
}

// ============================================================================
// GET /api/system — mode banner + system info
// ============================================================================

async function updateSystemInfo() {
    try {
        const response = await fetch(`${API_BASE}/api/system`);
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const data = await response.json();

        // Mode banner
        const banner = document.getElementById('mode-banner');
        const label  = document.getElementById('mode-label');
        if (banner && label && data.mode) {
            const color   = MODE_COLORS[data.mode] || '#37474f';
            banner.style.backgroundColor = color;
            label.textContent = modeLabel(data.mode, data.ip);
        }

        // Info grid
        const boardElem  = document.getElementById('board-name');
        const modeElem   = document.getElementById('wifi-mode');
        const ssidElem   = document.getElementById('wifi-ssid');
        const ipElem     = document.getElementById('wifi-ip');
        const uptimeElem = document.getElementById('uptime');

        if (boardElem)  { boardElem.textContent   = boardLabel(data.board || ''); }
        if (modeElem)   { modeElem.textContent     = data.mode  || '--'; }
        if (ssidElem)   { ssidElem.textContent      = data.ssid  || '--'; }
        if (ipElem)     { ipElem.textContent        = data.ip    || '--'; }
        if (uptimeElem) { uptimeElem.textContent    = formatUptime(data.uptime_s); }

        updateConnectionStatus(true);

    } catch (error) {
        console.error('Failed to update system info:', error);
        updateConnectionStatus(false);
    }
}

function modeLabel(mode, ip) {
    switch (mode) {
    case 'SoftAP': return `AP Mode \u2014 ${ip || '192.168.7.1'}`;
    case 'STA':    return `Station Mode \u2014 ${ip || '...'}`;
    case 'P2P':    return `P2P Direct \u2014 ${ip || '...'}`;
    default:       return mode;
    }
}

function formatUptime(seconds) {
    if (!Number.isFinite(seconds) || seconds < 0) { return '--'; }
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = seconds % 60;
    if (h > 0) { return `${h}h ${m}m ${s}s`; }
    if (m > 0) { return `${m}m ${s}s`; }
    return `${s}s`;
}

// ============================================================================
// GET /api/buttons
// ============================================================================

async function updateButtonStates() {
    try {
        const response = await fetch(`${API_BASE}/api/buttons`);
        if (!response.ok) {
            throw new Error(`HTTP error ${response.status}`);
        }

        const data = await response.json();

        if (data && Array.isArray(data.buttons)) {
            renderButtonStates(data.buttons);
        }

    } catch (error) {
        console.error('Failed to update button states:', error);
        if (buttonPlaceholder) {
            buttonPlaceholder.textContent = 'Failed to load button data';
            buttonPlaceholder.style.display = 'block';
        }
    }
}

// ============================================================================
// GET /api/leds
// ============================================================================

async function updateLEDStates() {
    try {
        const response = await fetch(`${API_BASE}/api/leds`);
        if (!response.ok) {
            throw new Error(`HTTP error ${response.status}`);
        }

        const data = await response.json();
        renderLedStates(data);

    } catch (error) {
        console.error('Failed to update LED states:', error);
        if (ledPlaceholder) {
            ledPlaceholder.textContent = 'Failed to load LED data';
            ledPlaceholder.style.display = 'block';
        }
    }
}

// ============================================================================
// POST /api/led
// ============================================================================

async function controlLED(ledNumber, action) {
    if (!availableLedNumbers.includes(ledNumber)) {
        console.warn(`LED ${ledNumber} is not available on this board.`);
        return;
    }

    try {
        const response = await fetch(`${API_BASE}/api/led`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ led: ledNumber, action: action }),
        });

        if (!response.ok) {
            throw new Error(`HTTP error ${response.status}`);
        }

        console.log(`LED ${ledNumber} ${action} sent`);
        setTimeout(() => updateLEDStates(), 100);

    } catch (error) {
        console.error('Failed to control LED:', error);
        alert(`Failed to control LED ${ledNumber}: ${error.message}`);
    }
}

// ============================================================================
// Connection status indicator
// ============================================================================

function updateConnectionStatus(isConnected) {
    const statusElem = document.getElementById('connection-status');
    if (statusElem) {
        if (isConnected) {
            statusElem.innerHTML  = '⬤ Connected';
            statusElem.className  = 'info-value status-online';
        } else {
            statusElem.innerHTML  = '⬤ Disconnected';
            statusElem.className  = 'info-value status-offline';
        }
    }
}

// Cleanup on page unload
window.addEventListener('beforeunload', function () {
    if (updateInterval) {
        clearInterval(updateInterval);
    }
});

// ============================================================================
// Render helpers
// ============================================================================

function renderButtonStates(buttons) {
    if (!buttonGrid || !buttonTemplate) {
        return;
    }

    const activeNumbers = new Set();

    if (buttonPlaceholder) {
        buttonPlaceholder.style.display = buttons.length > 0 ? 'none' : 'block';
    }

    buttons.forEach(button => {
        const num = Number(button.number);
        if (!Number.isFinite(num) || num < 0) {
            return;
        }

        activeNumbers.add(num);
        const entry = ensureButtonElement(num);
        if (!entry) {
            return;
        }

        const label = typeof button.name === 'string' && button.name.length > 0
            ? button.name : `Button ${num}`;
        if (entry.nameElem)  { entry.nameElem.textContent  = label; }
        if (entry.stateElem) {
            entry.stateElem.textContent = button.pressed ? 'Pressed' : 'Released';
            entry.stateElem.style.color = button.pressed
                ? BUTTON_PRESSED_COLOR : BUTTON_RELEASED_COLOR;
        }
        if (entry.countElem) { entry.countElem.textContent = button.count || 0; }

        if (entry.container) {
            entry.container.classList.toggle('active', Boolean(button.pressed));
        }
    });

    pruneInactiveButtons(activeNumbers);
}

function ensureButtonElement(number) {
    let entry = buttonElements.get(number);
    if (entry) { return entry; }

    const templateRoot = buttonTemplate.content.firstElementChild;
    if (!templateRoot) { return null; }

    const node      = templateRoot.cloneNode(true);
    node.id         = `button${number}`;
    const nameElem  = node.querySelector('.button-name');
    const stateElem = node.querySelector('.button-state');
    const countElem = node.querySelector('.button-count span');

    if (nameElem)  { nameElem.textContent  = `Button ${number}`; }
    if (stateElem) { stateElem.id          = `btn${number}-state`; }
    if (countElem) { countElem.id          = `btn${number}-count`; }

    buttonGrid.appendChild(node);
    entry = { container: node, nameElem, stateElem, countElem };
    buttonElements.set(number, entry);
    return entry;
}

function pruneInactiveButtons(activeNumbers) {
    for (const [number, entry] of buttonElements.entries()) {
        if (!activeNumbers.has(number)) {
            entry.container.remove();
            buttonElements.delete(number);
        }
    }
}

function renderLedStates(data) {
    if (!ledGrid || !ledTemplate) { return; }

    const entries = Array.isArray(data?.leds) ? data.leds : [];
    const normalized = entries
        .map(item => ({
            number: Number(item.number),
            isOn:   Boolean(item.is_on),
            name:   typeof item.name === 'string' ? item.name : undefined,
        }))
        .filter(item => Number.isFinite(item.number) && item.number >= 0)
        .sort((a, b) => a.number - b.number);

    if (ledPlaceholder) {
        ledPlaceholder.style.display = normalized.length > 0 ? 'none' : 'block';
    }

    const activeNumbers = new Set();

    normalized.forEach(({ number, isOn, name }) => {
        activeNumbers.add(number);
        const entry = ensureLedElement(number);
        if (!entry) { return; }

        const label = name && name.length > 0 ? name : `LED ${number}`;
        if (entry.nameElem) { entry.nameElem.textContent = label; }
        if (entry.indicator) {
            entry.indicator.classList.toggle('on', isOn);
        }
    });

    pruneInactiveLeds(activeNumbers);
    availableLedNumbers = Array.from(activeNumbers).sort((a, b) => a - b);
}

function ensureLedElement(number) {
    let entry = ledElements.get(number);
    if (entry) { return entry; }

    const templateRoot = ledTemplate.content.firstElementChild;
    if (!templateRoot) { return null; }

    const node      = templateRoot.cloneNode(true);
    node.id         = `led${number}`;
    const indicator = node.querySelector('.led-indicator');
    const nameElem  = node.querySelector('.led-name');
    const buttons   = node.querySelectorAll('button[data-action]');

    if (indicator) { indicator.id          = `led${number}-indicator`; }
    if (nameElem)  { nameElem.textContent  = `LED ${number}`; }

    buttons.forEach(button => {
        const action = button.dataset.action;
        if (action) {
            button.addEventListener('click', () => controlLED(number, action));
        }
    });

    ledGrid.appendChild(node);
    entry = { container: node, indicator: indicator || null, nameElem };
    ledElements.set(number, entry);
    return entry;
}

function pruneInactiveLeds(activeNumbers) {
    for (const [number, entry] of ledElements.entries()) {
        if (!activeNumbers.has(number)) {
            entry.container.remove();
            ledElements.delete(number);
        }
    }
}
