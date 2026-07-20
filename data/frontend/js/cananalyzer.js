const capture = {
    active: true,
    frames: [],
    stats: new Map(),
    previous: new Map(),
    knownNames: new Map(),
    startedAt: null,
    maximumFrames: 10000
};

loadNavigation();
initializeAnalyzer();

async function initializeAnalyzer() {
    await i18nReady;
    bindAnalyzerControls();
    await loadCanAddressNames();
    startCanEvents();
}

function bindAnalyzerControls() {
    _("capture-toggle").addEventListener("click", () => {
        capture.active = !capture.active;
        _("capture-toggle").classList.toggle("btn-primary", capture.active);
        _("capture-toggle").classList.toggle("btn-outline-primary", !capture.active);
        _("capture-toggle").innerHTML = capture.active
            ? `<i class="bi bi-pause-fill"></i> ${translate("can_analyzer.pause")}`
            : `<i class="bi bi-play-fill"></i> ${translate("can_analyzer.resume")}`;
    });
    _("clear-capture").addEventListener("click", clearCapture);
    _("reset-filters").addEventListener("click", resetFilters);
    _("export-json").addEventListener("click", () => exportCapture("json"));
    _("export-csv").addEventListener("click", () => exportCapture("csv"));
    ["filter-text", "direction-filter", "row-limit", "changed-only"].forEach(id => _(id).addEventListener(id === "filter-text" ? "input" : "change", renderAnalyzer));
}

function resetFilters() {
    _("filter-text").value = "";
    _("direction-filter").value = "all";
    _("changed-only").checked = false;
    renderAnalyzer();
}

async function loadCanAddressNames() {
    const config = await getConfigJson("/api/config/canbus");
    const addresses = config.Addresses || config;
    getDeepKeys(addresses).forEach(path => {
        const id = normalizeId(jsonPathToValue(addresses, path));
        if (!capture.knownNames.has(id)) capture.knownNames.set(id, []);
        capture.knownNames.get(id).push(formatAddressPath(path));
    });
}

function formatAddressPath(path) {
    const keyMap = {
        "Controller.FlameStatus":"can_value.controller_flame", "Controller.Error":"can_value.controller_error", "Controller.DateTime":"can_value.controller_datetime",
        "Heating.FeedCurrent":"can_value.heating_feed_current", "Heating.FeedMax":"can_value.heating_feed_max", "Heating.FeedSetpoint":"can_value.heating_feed_setpoint",
        "Heating.OutsideTemperature":"can_value.heating_outside", "Heating.Pump":"can_value.heating_pump", "Heating.Season":"can_value.heating_season",
        "Heating.Operation":"can_value.heating_operation", "Heating.Power":"can_value.heating_power", "Heating.Mode":"can_value.heating_mode", "Heating.Economy":"can_value.heating_economy",
        "HotWater.SetpointTemperature":"can_value.water_setpoint", "HotWater.MaxTemperature":"can_value.water_max", "HotWater.CurrentTemperature":"can_value.water_current",
        "HotWater.Now":"can_value.water_now", "HotWater.BufferOperation":"can_value.water_buffer", "HotWater.ContinousFlow.SetpointTemperature":"can_value.water_flow",
        "MixedCircuit.Pump":"can_value.mixed_pump", "MixedCircuit.FeedSetpoint":"can_value.mixed_setpoint", "MixedCircuit.FeedCurrent":"can_value.mixed_current", "MixedCircuit.Economy":"can_value.mixed_economy"
    };
    return translate(keyMap[path] || path);
}

function startCanEvents() {
    const events = new EventSource("/events");
    events.addEventListener("open", () => setConnectionState(true));
    events.addEventListener("error", () => setConnectionState(false));
    events.addEventListener("can", event => {
        if (!capture.active) return;
        try { recordFrame(JSON.parse(event.data)); }
        catch (error) { console.error("Invalid CAN event", error, event.data); }
    });
}

function setConnectionState(connected) {
    const element = _("connection-status");
    element.className = `alert py-2 ${connected ? "alert-success" : "alert-warning"}`;
    element.textContent = translate(connected ? "can_analyzer.connected" : "can_analyzer.disconnected");
}

function recordFrame(raw) {
    const data = Array.isArray(raw.data) ? raw.data.slice(0, 8).map(value => Number(value) & 0xff) : [];
    const frame = {
        sequence: capture.frames.length + 1,
        timestamp: Number.isFinite(Number(raw.ts)) ? Number(raw.ts) : performance.now(),
        received: raw.rcv !== false,
        id: Number(raw.id),
        idHex: normalizeId(raw.id),
        length: Number(raw.len ?? data.length),
        data
    };
    const previous = capture.previous.get(`${frame.received}:${frame.id}`);
    frame.delta = data.map((value, index) => previous && index < previous.data.length ? value - previous.data[index] : null);
    frame.changed = Boolean(previous) && (previous.length !== frame.length || data.some((value, index) => previous.data[index] !== value));
    capture.previous.set(`${frame.received}:${frame.id}`, frame);
    capture.frames.push(frame);
    if (capture.frames.length > capture.maximumFrames) capture.frames.shift();
    if (capture.startedAt === null) capture.startedAt = frame.timestamp;

    let stat = capture.stats.get(frame.id);
    if (!stat) stat = {id: frame.id, idHex: frame.idHex, count: 0, changed: 0, first: frame.timestamp, last: frame.timestamp, intervals: [], lengths: new Set(), lastFrame: frame};
    if (stat.count > 0) stat.intervals.push(frame.timestamp - stat.last);
    if (stat.intervals.length > 200) stat.intervals.shift();
    stat.count++;
    if (frame.changed) stat.changed++;
    stat.last = frame.timestamp;
    stat.lengths.add(frame.length);
    stat.lastFrame = frame;
    capture.stats.set(frame.id, stat);
    renderAnalyzer();
}

function clearCapture() {
    capture.frames = [];
    capture.stats.clear();
    capture.previous.clear();
    capture.startedAt = null;
    renderAnalyzer();
}

function renderAnalyzer() {
    const filter = _("filter-text").value.trim().toLowerCase();
    const direction = _("direction-filter").value;
    const changedOnly = _("changed-only").checked;
    const rowLimit = Number(_("row-limit").value);
    _("reset-filters").disabled = !filter && direction === "all" && !changedOnly;
    const filtered = capture.frames.filter(frame => {
        const names = getNames(frame.idHex).toLowerCase();
        const textMatches = !filter || frame.idHex.toLowerCase().includes(filter) || names.includes(filter);
        const directionMatches = direction === "all" || (direction === "received") === frame.received;
        return textMatches && directionMatches && (!changedOnly || frame.changed);
    });

    _("frame-count").textContent = capture.frames.length;
    _("id-count").textContent = capture.stats.size;
    _("changed-count").textContent = capture.frames.filter(frame => frame.changed).length;
    const end = capture.frames.length ? capture.frames[capture.frames.length - 1].timestamp : capture.startedAt;
    _("capture-duration").textContent = capture.startedAt === null ? "0.0 s" : `${((end - capture.startedAt) / 1000).toFixed(1)} s`;
    renderSummary(filter);
    renderFrames(filtered.slice(-rowLimit));
}

function renderSummary(filter) {
    const rows = [...capture.stats.values()].filter(stat => !filter || stat.idHex.toLowerCase().includes(filter) || getNames(stat.idHex).toLowerCase().includes(filter)).sort((a, b) => a.id - b.id);
    _("summary-body").innerHTML = rows.length ? rows.map(stat => {
        const mean = stat.intervals.length ? stat.intervals.reduce((sum, value) => sum + value, 0) / stat.intervals.length : null;
        return `<tr role="button" data-can-id="${stat.idHex}"><td><code>${stat.idHex}</code></td><td>${escapeHtml(getNames(stat.idHex) || translate("can_analyzer.unknown"))}</td><td>${stat.count}${stat.changed ? ` <span class="badge text-bg-warning">${stat.changed} Δ</span>` : ""}</td><td>${mean === null ? "—" : formatInterval(mean)}</td><td>${[...stat.lengths].sort().join(", ")}</td><td class="can-bytes">${formatBytes(stat.lastFrame.data)}</td><td class="small">${candidateValues(stat.lastFrame.data)}</td></tr>`;
    }).join("") : emptyRow(7);
    _("summary-body").querySelectorAll("tr[data-can-id]").forEach(row => row.addEventListener("click", () => { _("filter-text").value = row.dataset.canId; renderAnalyzer(); }));
}

function renderFrames(frames) {
    _("frame-body").innerHTML = frames.length ? frames.map(frame => {
        const delta = frame.delta.map(value => value === null || value === 0 ? "" : `${value > 0 ? "+" : ""}${value}`).join(" · ");
        return `<tr class="${frame.changed ? "can-changed" : ""}"><td class="text-nowrap">${formatTime(frame.timestamp - capture.startedAt)}</td><td title="${translate(frame.received ? "can_analyzer.received" : "can_analyzer.sent")}"><i class="bi ${frame.received ? "bi-arrow-left" : "bi-arrow-right"}"></i></td><td><code>${frame.idHex}</code></td><td>${escapeHtml(getNames(frame.idHex) || translate("can_analyzer.unknown"))}</td><td>${frame.length}</td><td class="can-bytes">${formatBytes(frame.data)}</td><td><code>${delta}</code></td></tr>`;
    }).join("") : emptyRow(7);
    if (_("auto-scroll").checked) _("frame-log-wrap").scrollTop = _("frame-log-wrap").scrollHeight;
}

function candidateValues(data) {
    if (!data.length) return translate("can_analyzer.empty_payload");
    const values = [`u8=${data[0]}`, `u8/2=${(data[0] / 2).toFixed(1)} °C`, `bool=${data[0] ? 1 : 0}`];
    if (data.length >= 2) {
        let signed = (data[0] << 8) | data[1];
        if (signed & 0x8000) signed -= 0x10000;
        values.push(`s16be/100=${(signed / 100).toFixed(2)} °C`);
    }
    return values.join(" · ");
}

function exportCapture(format) {
    const metadata = {name: _("capture-name").value.trim(), exportedAt: new Date().toISOString(), frameCount: capture.frames.length, addressNames: Object.fromEntries(capture.knownNames)};
    let content;
    let mime;
    if (format === "json") {
        content = JSON.stringify({metadata, frames: capture.frames}, null, 2);
        mime = "application/json";
    } else {
        const rows = [["sequence", "timestamp_ms", "direction", "id", "name", "dlc", "data_hex", "data_decimal", "changed"]];
        capture.frames.forEach(frame => rows.push([frame.sequence, frame.timestamp, frame.received ? "rx" : "tx", frame.idHex, getNames(frame.idHex), frame.length, frame.data.map(hexByte).join(" "), frame.data.join(" "), frame.changed]));
        content = rows.map(row => row.map(csvValue).join(",")).join("\r\n");
        mime = "text/csv";
    }
    const safeName = (metadata.name || "cerasmarter-can-capture").replace(/[^a-z0-9_-]+/gi, "-");
    const link = document.createElement("a");
    link.href = URL.createObjectURL(new Blob([content], {type: mime}));
    link.download = `${safeName}.${format}`;
    link.click();
    setTimeout(() => URL.revokeObjectURL(link.href), 1000);
}

function normalizeId(value) { return `0x${Number(typeof value === "string" ? parseInt(value, 0) : value).toString(16).padStart(3, "0").toUpperCase()}`; }
function getNames(id) { return (capture.knownNames.get(id) || []).join(" / "); }
function hexByte(value) { return Number(value).toString(16).padStart(2, "0").toUpperCase(); }
function formatBytes(data) { return data.length ? data.map(value => `<code>${hexByte(value)}</code>`).join("") : "<code>—</code>"; }
function formatInterval(milliseconds) { return milliseconds >= 1000 ? `${(milliseconds / 1000).toFixed(2)} s` : `${milliseconds.toFixed(0)} ms`; }
function formatTime(milliseconds) { const minutes = Math.floor(milliseconds / 60000); return `${String(minutes).padStart(2, "0")}:${((milliseconds % 60000) / 1000).toFixed(3).padStart(6, "0")}`; }
function emptyRow(columns) { return `<tr><td colspan="${columns}" class="text-center text-muted py-4">${translate("can_analyzer.no_frames")}</td></tr>`; }
function escapeHtml(value) { const element = document.createElement("span"); element.textContent = value; return element.innerHTML; }
function csvValue(value) { const text = String(value ?? ""); return /[",\r\n]/.test(text) ? `"${text.replaceAll('"', '""')}"` : text; }
