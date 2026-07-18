const dashboardSamples = [];
loadNavigation();
i18nReady.then(() => {
    refreshDashboard();
    setInterval(refreshDashboard, 5000);
});

function temperature(value) { return Number.isFinite(Number(value)) ? `${Number(value).toFixed(1)} °C` : "--"; }
function setBadge(id, ok, good, bad) {
    const el = _(id); el.textContent = translate(ok ? good : bad);
    el.className = `badge ${ok ? "bg-success" : "bg-danger"}`;
}

async function refreshDashboard() {
    try {
        const [runtime, status] = await Promise.all([fetch('/api/runtime').then(r => r.json()), getSystemStatus()]);
        _( "outside").textContent = temperature(runtime.General.OutsideTemperature);
        _("feed-current").textContent = temperature(runtime.Heating.FeedCurrent);
        _("feed-target").textContent = temperature(runtime.Heating.CalculatedFeedSetpoint);
        _("water-current").textContent = temperature(runtime.HotWater.Current);
        _("water-target").textContent = temperature(runtime.HotWater.Setpoint);
        _("flame").textContent = translate(runtime.General.FlameLit ? "common.on" : "common.off");
        _("flame-icon").className = `bi bi-fire ${runtime.General.FlameLit ? "text-danger" : "text-muted"}`;
        _("power").textContent = `${runtime.Heating.Power}%`;
        const healthy = runtime.System.Wifi && runtime.System.CanErrors === 0;
        _("runtime-status").textContent = translate(runtime.System.FailSafe ? "dashboard.failsafe" : (healthy ? "dashboard.live" : "dashboard.degraded"));
        _("runtime-status").className = `badge ${runtime.System.FailSafe ? "bg-warning text-dark" : (healthy ? "bg-success" : "bg-danger")}`;
        dashboardSamples.push([runtime.Heating.FeedCurrent, runtime.Heating.CalculatedFeedSetpoint, runtime.General.OutsideTemperature]);
        if (dashboardSamples.length > 60) dashboardSamples.shift();
        drawChart();
        _("model").textContent = `${status.model} r${status.revision} · ${status.cores} cores`;
        const usedHeap = status.heap - status.freeheap;
        setUsageBar("heap", "prog-heap", usedHeap, status.heap);
        setUsageBar("sketch", "prog-sketch", status.sketchsize, status.freesketch);
        setUsageBar("filesystem", "prog-filesystem", status.filesystemused, status.filesystemsize);
        const canOk = status.canstatus === 0;
        setBadge("can", canOk, "common.connected", CanErrorCodes[status.canstatus] || "common.error");
        setBadge("canerrorcount", status.canerrorcount === 0, "0", String(status.canerrorcount));
        setBadge("mqtt", status.mqtt, "common.connected", "common.disconnected");
    } catch (error) {
        _("runtime-status").textContent = translate("common.unavailable");
        _("runtime-status").className = "badge bg-danger";
    }
}

function setUsageBar(labelId, barId, used, total) {
    const percent = total > 0 ? Math.min(100, used / total * 100) : 0;
    _(labelId).textContent = `${humanReadableSize(used)} / ${humanReadableSize(total)} · ${percent.toFixed(1)}%`;
    const bar = _(barId);
    bar.style.width = `${percent}%`;
    bar.textContent = `${Math.round(percent)}%`;
    bar.setAttribute('aria-valuenow', percent.toFixed(1));
    bar.className = `progress-bar ${percent >= 90 ? 'bg-danger' : (percent >= 75 ? 'bg-warning text-dark' : 'bg-primary')}`;
}

function drawChart() {
    const canvas = _("temperature-chart"), rect = canvas.getBoundingClientRect(), scale = devicePixelRatio || 1;
    canvas.width = rect.width * scale; canvas.height = rect.height * scale;
    const ctx = canvas.getContext('2d'); ctx.scale(scale, scale);
    const w = rect.width, h = rect.height, pad = 25;
    const values = dashboardSamples.flat().filter(Number.isFinite);
    const min = Math.min(...values, 0) - 2, max = Math.max(...values, 50) + 2;
    ctx.strokeStyle = getComputedStyle(document.documentElement).getPropertyValue('--ui-border') || '#dee2e6'; ctx.lineWidth = 1;
    for (let i = 0; i < 5; i++) { const y = pad + i * (h - 2 * pad) / 4; ctx.beginPath(); ctx.moveTo(pad, y); ctx.lineTo(w - pad, y); ctx.stroke(); }
    ['#dc3545','#0d6efd','#198754'].forEach((color, series) => {
        ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.beginPath();
        dashboardSamples.forEach((sample, i) => { const x = pad + i * (w - 2 * pad) / Math.max(59, dashboardSamples.length - 1); const y = h - pad - (sample[series] - min) * (h - 2 * pad) / (max - min); i ? ctx.lineTo(x,y) : ctx.moveTo(x,y); }); ctx.stroke();
    });
}
window.addEventListener('resize', drawChart);
