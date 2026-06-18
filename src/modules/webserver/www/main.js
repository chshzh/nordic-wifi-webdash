/*
 * Nordic Wi-Fi WebDash — Main JavaScript 
 */

// Configuration
const API_BASE = '';
const REFRESH_INTERVAL        = 500;    // ms — buttons + leds
const SYSTEM_REFRESH_INTERVAL = 30000;  // ms — system info re-sync (mode/IP/board)
const SYSMON_INTERVAL         = 5000;   // ms — threads + heap polling (matches CONFIG_ZEGO_MEMONITOR_INTERVAL_MS)

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
    'SoftAP':     '#1565c0', // blue
    'STA':        '#2e7d32', // green
    'P2P_GO':     '#6a1b9a', // purple
    'P2P_CLIENT': '#e65100', // orange
};

// State
let updateInterval    = null;
let systemInterval    = null;
let sysmonInterval    = null;
let refreshInFlight   = false;

// System info — local uptime ticker
let localUptimeSec    = 0;
let uptimeTicker      = null;
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
    console.log('Nordic Wi-Fi WebDash initialized');

    buttonGrid        = document.getElementById('button-grid');
    buttonTemplate    = document.getElementById('button-template');
    buttonPlaceholder = document.getElementById('button-placeholder');
    ledGrid           = document.getElementById('led-grid');
    ledTemplate       = document.getElementById('led-template');
    ledPlaceholder    = document.getElementById('led-placeholder');

    initThemeToggle();
    initSysmonToggles();
    startAutoUpdate();    // buttons + leds at 500 ms
    startSystemUpdate();  // system info once + every 30 s
    startSysmonUpdate();  // threads + heap
});

// 500 ms loop — buttons and LEDs only
function startAutoUpdate() {
    if (updateInterval) { clearInterval(updateInterval); }

    refreshAllSections();

    updateInterval = setInterval(function () {
        refreshAllSections();
    }, REFRESH_INTERVAL);
}

async function refreshAllSections() {
    if (refreshInFlight) { return; }

    refreshInFlight = true;
    try {
        await Promise.allSettled([
            updateButtonStates(),
            updateLEDStates(),
        ]);
    } finally {
        refreshInFlight = false;
    }
}

// System info — fetch once + every 30 s; uptime ticked locally every 1 s
function startSystemUpdate() {
    if (systemInterval) { clearInterval(systemInterval); }
    updateSystemInfo();
    systemInterval = setInterval(updateSystemInfo, SYSTEM_REFRESH_INTERVAL);
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
            label.textContent = modeLabel(data.mode, data.device_ip);
        }

        // Static fields — written every 30 s
        const modeElem    = document.getElementById('app-wifi-mode');
        const boardElem   = document.getElementById('board-name');
        const ssidElem    = document.getElementById('wifi-ssid');
        const deviceIpElem = document.getElementById('device-ip');
        const clientIpElem = document.getElementById('client-ip');
        const clientMacElem = document.getElementById('client-mac');

        if (modeElem)     { modeElem.textContent     = data.mode       || '--'; }
        if (boardElem)    { boardElem.textContent    = boardLabel(data.board || ''); }
        if (ssidElem)     { ssidElem.textContent     = data.ssid       || '--'; }
        if (deviceIpElem) { deviceIpElem.textContent = data.device_ip  || '--'; }
        if (clientIpElem) { clientIpElem.textContent = data.client_ip  || '--'; }
        if (clientMacElem){ clientMacElem.textContent = data.client_mac || '--'; }

        // Sync local uptime counter from server, then tick locally every 1 s
        localUptimeSec = typeof data.uptime_s === 'number' ? data.uptime_s : 0;
        if (uptimeTicker) { clearInterval(uptimeTicker); }
        const uptimeElem = document.getElementById('uptime');
        if (uptimeElem) { uptimeElem.textContent = formatUptime(localUptimeSec); }
        uptimeTicker = setInterval(function () {
            localUptimeSec++;
            const el = document.getElementById('uptime');
            if (el) { el.textContent = formatUptime(localUptimeSec); }
        }, 1000);

        updateConnectionStatus(true);

    } catch (error) {
        console.error('Failed to update system info:', error);
        updateConnectionStatus(false);
    }
}

function modeLabel(mode, serverIp) {
    switch (mode) {
    case 'SoftAP':     return `SoftAP Mode \u2014 ${serverIp || '192.168.7.1'}`;
    case 'STA':        return `Station Mode \u2014 ${serverIp || '...'}`;
    case 'P2P_GO':     return `P2P GO \u2014 ${serverIp || '192.168.7.1'}`;
    case 'P2P_CLIENT': return `P2P Client \u2014 ${serverIp || '...'}`;
    default:           return mode;
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
            if (buttonPlaceholder && data.buttons.length === 0) {
                buttonPlaceholder.textContent = 'No buttons available on this board';
                buttonPlaceholder.style.display = 'block';
            }
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
    if (updateInterval)  { clearInterval(updateInterval);  }
    if (systemInterval)  { clearInterval(systemInterval);  }
    if (uptimeTicker)    { clearInterval(uptimeTicker);    }
    if (sysmonInterval)  { clearInterval(sysmonInterval);  }
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

// ============================================================================
// Dark mode toggle (FR-105)
// ============================================================================

function initThemeToggle() {
    const btn = document.getElementById('theme-toggle');
    if (!btn) { return; }

    function isDark() {
        const attr = document.documentElement.getAttribute('data-theme');
        if (attr === 'dark')  { return true; }
        if (attr === 'light') { return false; }
        return window.matchMedia('(prefers-color-scheme: dark)').matches;
    }

    function syncLabel() {
        btn.textContent = isDark() ? '☀️' : '🌙';
        btn.title = isDark() ? 'Switch to light mode' : 'Switch to dark mode';
    }

    btn.addEventListener('click', function () {
        document.documentElement.setAttribute('data-theme', isDark() ? 'light' : 'dark');
        syncLabel();
    });

    syncLabel();
}

// ============================================================================
// Sysmon — Thread Monitor (FR-107) + Heap Monitor (FR-108)
// ============================================================================

function startSysmonUpdate() {
    if (sysmonInterval) { clearInterval(sysmonInterval); }
    sysmonInterval = setInterval(updateSysmon, SYSMON_INTERVAL);
}

async function updateSysmon() {
    await Promise.allSettled([updateThreads(), updateHeap()]);
}

// ── Thread monitor ──────────────────────────────────────────────────────────

async function updateThreads() {
    try {
        const res = await fetch(`${API_BASE}/api/threads`);
        if (res.status === 404) {
            setSysmonSectionVisible('threads-section', false);
            return;
        }
        if (!res.ok) { return; }
        const data = await res.json();
        setSysmonSectionVisible('threads-section', true);
        if (data.interval_ms) {
            setSysmonInterval('threads-interval', data.interval_ms);
        }
        if (Array.isArray(data.threads)) { renderThreads(data.threads); }
    } catch (e) {
        console.error('threads poll:', e);
    }
}

function renderThreads(threads) {
    const tbody = document.getElementById('threads-tbody');
    if (!tbody) { return; }

    if (threads.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" class="sysmon-empty">No threads</td></tr>';
        return;
    }

    tbody.innerHTML = threads.map(t => {
        const hwm   = t.stack_hwm  || 0;
        const total = t.stack_size || 0;
        const pct   = total > 0 ? Math.round((hwm / total) * 100) : 0;
        // Warn at 80%; for large stacks (>2048 B) the practical threshold is 90%
        const warn  = pct >= (total > 2048 ? 90 : 80);
        const bar   = total > 0
            ? `<div class="stack-bar-track"><div class="stack-bar-fill ${warn ? 'stack-bar-warn' : ''}" style="width:${pct}%"></div></div> <span class="stack-bar-pct">${pct}%</span>`
            : '--';
        return `<tr>
            <td class="thread-name">${esc(t.name)}</td>
            <td class="thread-state ${t.state}">${esc(t.state)}</td>
            <td>${formatBytes(hwm)}</td>
            <td>${formatBytes(total)}</td>
            <td class="stack-bar-cell">${bar}</td>
        </tr>`;
    }).join('');
}

// ── Heap monitor ────────────────────────────────────────────────────────────

async function updateHeap() {
    try {
        const res = await fetch(`${API_BASE}/api/heaps`);
        if (res.status === 404) {
            setSysmonSectionVisible('heap-section', false);
            return;
        }
        if (!res.ok) { return; }
        const d = await res.json();
        setSysmonSectionVisible('heap-section', true);
        if (d.interval_ms) {
            setSysmonInterval('heap-interval', d.interval_ms);
        }
        renderHeap(d);
    } catch (e) {
        console.error('heap poll:', e);
    }
}

function setSysmonSectionVisible(sectionId, visible) {
    const el = document.getElementById(sectionId);
    if (el) { el.style.display = visible ? '' : 'none'; }
}

function setSysmonInterval(spanId, ms) {
    const el = document.getElementById(spanId);
    if (el) { el.textContent = `${Math.round(ms / 1000)}s`; }
}



function renderHeap(d) {
    const heaps = Array.isArray(d.heaps) ? d.heaps : [];
    const tbody = document.getElementById('heap-tbody');
    if (!tbody) { return; }

    if (heaps.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" class="sysmon-empty">No data</td></tr>';
        return;
    }

    tbody.innerHTML = heaps.map(h => {
        const used  = h.used  || 0;
        const free  = h.free  || 0;
        const hwm   = h.watermark || 0;
        const total = h.total || (used + free);
        const usedPct = total > 0 ? Math.round((used  / total) * 100) : 0;
        const hwmPct  = total > 0 ? Math.round((hwm   / total) * 100) : 0;
        const warn = usedPct >= 70 && usedPct < 90;
        const crit = usedPct >= 90;
        const bar = total > 0
            ? `<div class="heap-bar-track"><div class="heap-bar-fill ${crit ? 'heap-bar-crit' : warn ? 'heap-bar-warn' : ''}" style="width:${usedPct}%"></div></div> <span class="stack-bar-pct">${usedPct}%</span>`
            : '--';
        return `<tr>
            <td class="thread-name">${esc(h.name)}</td>
            <td>${formatBytes(used)}</td>
            <td>${formatBytes(free)}</td>
            <td>${formatBytes(hwm)} (${hwmPct}%)</td>
            <td class="stack-bar-cell">${bar}</td>
        </tr>`;
    }).join('');
}

// ── Sysmon collapse toggles ──────────────────────────────────────────────────

function initSysmonToggles() {
    [['threads-toggle', 'threads-panel'], ['heap-toggle', 'heap-panel']].forEach(([btnId, panelId]) => {
        const btn   = document.getElementById(btnId);
        const panel = document.getElementById(panelId);
        if (!btn || !panel) { return; }
        btn.style.cursor = 'pointer';
        btn.addEventListener('click', function () {
            const collapsed = panel.style.display === 'none';
            panel.style.display = collapsed ? '' : 'none';
            const icon = btn.querySelector('.collapse-icon');
            if (icon) { icon.textContent = collapsed ? '▼' : '▶'; }
        });
    });
}

// ── Utilities ───────────────────────────────────────────────────────────────

function formatBytes(n) {
    if (!Number.isFinite(n) || n < 0) { return '--'; }
    return `${n} B`;
}

function setText(id, val) {
    const el = document.getElementById(id);
    if (el) { el.textContent = val; }
}

function esc(s) {
    return String(s)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;');
}
