const logState = {
    entries: [],      // live history oldest first, plus client-side restart markers
    last: null,       // newest history entry; live events at or below its seq are duplicates
    boot: null,       // X-Log-Boot of the last history fetch; a different value means the device restarted
    synced: false,    // live events wait in `pending` until the backlog fetch has landed
    pending: [],
    events: null,
    syncRun: 0,
    persistent: [],
    maximumEntries: 5000,
    maximumRows: 1000
};
const LogLevelNames = ["ERROR", "WARN", "INFO", "DEBUG"];

loadNavigation();
initializeLogs();

async function initializeLogs() {
    await i18nReady;
    [0, 1, 2, 3].forEach(level => _(`level-${level}`).addEventListener("change", renderLog));
    _("log-filter").addEventListener("input", renderLog);
    _("log-follow").addEventListener("change", () => { if (_("log-follow").checked) scrollToLatest(); });
    _("log-view").addEventListener("scroll", () => {
        const view = _("log-view");
        _("log-follow").checked = view.scrollHeight - view.scrollTop - view.clientHeight <= 4;
    });
    _("log-download").addEventListener("click", downloadLog);
    _("persistent-refresh").addEventListener("click", loadPersistent);
    document.addEventListener("ui-language-changed", renderCount);
    renderLog();
    connectLog();
    loadPersistent();
}

function connectLog() {
    if (logState.events) logState.events.close();
    logState.synced = false;
    setLogStatus("secondary", "logs.connecting");
    const events = new EventSource("/api/logs/events");
    logState.events = events;
    events.addEventListener("open", syncHistory);
    events.addEventListener("error", () => {
        logState.synced = false;
        logState.syncRun++;
        // Queued lines may predate a restart; the reconnect's history fetch recovers same-session lines.
        logState.pending.length = 0;
        if (events.readyState === EventSource.CLOSED) setLogStatus("danger", "logs.closed", "", true);
        else setLogStatus("warning", "logs.reconnecting");
    });
    events.addEventListener("log", event => {
        const entry = parseEntry(event.data);
        if (entry) receiveEntry(entry);
    });
}

function receiveEntry(entry) {
    if (logState.synced && !(logState.last && entry.seq > logState.last.seq + 1)) { addEntries([entry]); return; }
    logState.pending.push(entry);
    if (logState.pending.length > logState.maximumEntries) logState.pending.shift();
    // History seqs are contiguous per boot, so a skipped seq means the live stream dropped lines: refetch them.
    if (logState.synced) syncHistory();
}

async function syncHistory() {
    const run = ++logState.syncRun;
    logState.synced = false;
    setLogStatus("secondary", "logs.loading");
    try {
        const last = logState.last;
        let log = await fetchLog(last ? `/api/logs?since=${encodeURIComponent(last.seq)}` : "/api/logs");
        const restarted = logState.boot !== null && log.boot !== logState.boot;
        if (restarted && last) log = await fetchLog("/api/logs");
        if (run !== logState.syncRun) return;
        if (restarted) addRestartMarker();
        logState.boot = log.boot;
        addEntries(log.entries);
        logState.synced = true;
        setLogStatus("success", "logs.connected");
        logState.pending.splice(0).forEach(receiveEntry);
    } catch (error) {
        if (run === logState.syncRun) setLogStatus("danger", "logs.load_failed", error.message, true);
    }
}

async function fetchLog(url) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 10000);
    try {
        const response = await fetch(url, {cache: "no-store", signal: controller.signal});
        if (!response.ok) throw new Error(`HTTP ${response.status} ${response.statusText}`.trim());
        return {entries: parseLog(await response.text()), boot: response.headers.get("X-Log-Boot")};
    } catch (error) {
        throw error.name === "AbortError" ? new Error(translate("logs.timeout")) : error;
    } finally {
        clearTimeout(timeout);
    }
}

function parseEntry(line) {
    const match = /^(\d+)\t([0-3])\t([^\t]*)\t(.*)$/.exec(line);
    return match ? {seq: Number(match[1]), level: Number(match[2]), time: match[3], message: match[4]} : null;
}

function parseLog(text) {
    const lines = text.split(/\r?\n/).filter(line => line);
    const entries = lines.map(parseEntry).filter(Boolean);
    if (lines.length && !entries.length) throw new Error(translate("logs.invalid_response"));
    return entries;
}

function addEntries(entries) {
    const fresh = [];
    entries.forEach(entry => {
        if (logState.last && entry.seq <= logState.last.seq) return;
        fresh.push(entry);
        logState.last = entry;
    });
    if (fresh.length) appendEntries(fresh);
}

function addRestartMarker() {
    logState.last = null;
    appendEntries([{restart: true}]);
}

function appendEntries(entries) {
    logState.entries.push(...entries);
    const excess = logState.entries.length - logState.maximumEntries;
    if (excess > 0) logState.entries.splice(0, excess).forEach(entry => entry.row && entry.row.remove());
    const matches = currentFilter();
    const rows = entries.filter(matches).map(entryRow);
    const view = _("log-view");
    if (rows.length) {
        if (view.querySelector(".log-empty")) view.replaceChildren();
        view.append(...rows);
        while (view.childElementCount > logState.maximumRows) view.firstElementChild.remove();
    }
    if (_("log-follow").checked) scrollToLatest();
    renderCount();
}

function currentFilter() {
    const levels = [0, 1, 2, 3].map(level => _(`level-${level}`).checked);
    const text = _("log-filter").value.trim().toLowerCase();
    return entry => entry.restart
        || (levels[entry.level] && (!text || entry.message.toLowerCase().includes(text) || entry.time.includes(text)));
}

function renderLog() {
    renderRows(_("log-view"), logState.entries.filter(currentFilter()).slice(-logState.maximumRows), "logs.empty");
    if (_("log-follow").checked) scrollToLatest();
    renderCount();
}

function renderRows(view, entries, emptyKey) {
    if (entries.length) { view.replaceChildren(...entries.map(entryRow)); return; }
    const empty = i18nElement("div", emptyKey);
    empty.className = "log-empty text-muted";
    view.replaceChildren(empty);
}

function renderCount() {
    const shown = _("log-view").getElementsByClassName("log-entry").length;
    const total = logState.entries.filter(entry => !entry.restart).length;
    _("log-count").textContent = translate("logs.count", {shown, total});
}

function entryRow(entry) {
    if (entry.row) return entry.row;
    const row = document.createElement("div");
    if (entry.restart) {
        row.className = "log-row log-restart";
        row.append(i18nElement("span", "logs.restarted"));
    } else {
        // Firmware boot markers in the persistent buffer start with "▶".
        row.className = `log-row log-entry log-row-${entry.level}${entry.message.startsWith("▶") ? " log-boot" : ""}`;
        const time = document.createElement("code");
        time.className = "log-time";
        time.textContent = entry.time;
        const level = i18nElement("span", `logs.level_${entry.level}`);
        level.className = `badge log-level log-level-${entry.level}`;
        const message = document.createElement("code");
        message.className = "log-message";
        message.textContent = entry.message;
        row.append(time, level, message);
    }
    entry.row = row;
    return row;
}

function i18nElement(tag, key) {
    const element = document.createElement(tag);
    element.dataset.i18n = key;
    element.textContent = translate(key);
    return element;
}

function scrollToLatest() {
    const view = _("log-view");
    view.scrollTop = view.scrollHeight;
}

function statusAlert(kind, key, detail, reconnect) {
    const alert = document.createElement("div");
    alert.className = `alert alert-${kind} py-2 my-2 d-flex flex-wrap align-items-center gap-2`;
    alert.append(i18nElement("span", key));
    if (detail) {
        const detailText = document.createElement("span");
        detailText.className = "small";
        detailText.textContent = detail;
        alert.append(detailText);
    }
    if (reconnect) {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "btn btn-sm btn-outline-secondary ms-auto";
        button.append(i18nElement("span", "logs.reconnect"));
        button.addEventListener("click", connectLog);
        alert.append(button);
    }
    return alert;
}

function setLogStatus(kind, key, detail, reconnect) {
    _("log-status").replaceChildren(statusAlert(kind, key, detail, reconnect));
}

async function loadPersistent() {
    const button = _("persistent-refresh");
    button.disabled = true;
    _("persistent-status").replaceChildren(statusAlert("secondary", "common.loading"));
    try {
        logState.persistent = (await fetchLog("/api/logs/persistent")).entries;
        renderRows(_("persistent-view"), logState.persistent, "logs.persistent_empty");
        _("persistent-view").scrollTop = _("persistent-view").scrollHeight;
        _("persistent-status").replaceChildren();
    } catch (error) {
        _("persistent-status").replaceChildren(statusAlert("danger", "logs.persistent_failed", error.message));
    } finally {
        button.disabled = false;
    }
}

function downloadLog() {
    const format = entry => entry.restart ? "--- device restarted ---" : `${entry.seq}\t${LogLevelNames[entry.level]}\t${entry.time}\t${entry.message}`;
    const content = [`# Cerasmarter system log, exported ${new Date().toISOString()}`, "", "## Live history",
        ...logState.entries.map(format), "", "## Events across reboots", ...logState.persistent.map(format), ""].join("\n");
    const link = document.createElement("a");
    link.href = URL.createObjectURL(new Blob([content], {type: "text/plain;charset=utf-8"}));
    link.download = `cerasmarter-log-${new Date().toISOString().slice(0, 19).replace(/[:T]/g, "-")}.txt`;
    link.click();
    setTimeout(() => URL.revokeObjectURL(link.href), 1000);
}
