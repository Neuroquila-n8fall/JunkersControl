loadNavigation();
const controlForm = _("control-form");
const controlFields = _("control-fields");
const booleanFields = new Set(["Enabled", "OverrideSetpoint", "DynamicAdaption", "ValveScaling", "Boost", "FastHeatup", "ForecastEnabled"]);

async function requestControlJson(url, options = {}) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 5000);
    try {
        const response = await fetch(url, {...options, signal: controller.signal, cache: 'no-store'});
        const result = await response.json();
        if (!response.ok) throw new Error(result && typeof result.msg === 'string' ? result.msg : response.statusText);
        return {status: response.status, result};
    } finally {
        clearTimeout(timeout);
    }
}

async function loadControlValues(expectedSettings) {
    let command;
    let matched = false;
    const attempts = expectedSettings === undefined ? 1 : 4;
    for (let attempt = 0; attempt < attempts; attempt++) {
        // A 202 response precedes main-loop application. Refresh, never repost.
        if (expectedSettings !== undefined) await new Promise(resolve => setTimeout(resolve, 500));
        const {result: runtime} = await requestControlJson('/api/runtime');
        command = runtime && runtime.Command;
        if (!command || typeof command !== 'object' || Array.isArray(command)
            || !Number.isFinite(command.WeatherSupportMaximum)
            || command.WeatherSupportMaximum < 0 || command.WeatherSupportMaximum > 100
            || typeof command.ForecastEnabled !== 'boolean'
            || !Number.isFinite(command.ForecastCoefficient)
            || command.ForecastCoefficient < 0 || command.ForecastCoefficient > 1) {
            throw new Error(translate("control.load_failed"));
        }
        matched = expectedSettings === undefined
            || (command.WeatherSupportMaximum === expectedSettings.WeatherSupportMaximum
                && command.ForecastEnabled === expectedSettings.ForecastEnabled
                && Math.abs(command.ForecastCoefficient - expectedSettings.ForecastCoefficient) < 0.000001);
        if (matched) break;
    }
    Object.entries(command).forEach(([key, value]) => {
        const el = controlForm.elements.namedItem(key); if (!el) return;
        if (el.type === 'checkbox') el.checked = Boolean(value); else el.value = value;
    });
    return matched;
}

controlForm.addEventListener('submit', async event => {
    event.preventDefault();
    if (controlFields.disabled || !controlForm.reportValidity()) return;
    const payload = {};
    new FormData(controlForm).forEach((value, key) => payload[key] = booleanFields.has(key) ? true : Number(value));
    booleanFields.forEach(key => payload[key] = _(key).checked);
    if (Object.values(payload).some(value => typeof value === 'number' && !Number.isFinite(value))) {
        showControlStatus('danger', "control.invalid_values");
        return;
    }
    controlFields.disabled = true;
    showControlStatus('info', "control.sending");
    let accepted = false;
    try {
        const {status} = await requestControlJson('/api/control/queued', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(payload)});
        if (status !== 202) throw new Error(translate("control.queue_unconfirmed"));
        accepted = true;
        showControlStatus('info', "control.queued");
        const refreshed = await loadControlValues(payload);
        showControlStatus(refreshed ? 'info' : 'warning',
            refreshed ? "control.queued_refreshed" : "control.settings_pending");
    } catch (error) {
        showControlStatus(accepted ? 'warning' : 'danger',
            accepted ? "control.accepted_refresh_failed" : "control.request_unconfirmed", error.message);
    } finally {
        controlFields.disabled = false;
    }
});
function showControlStatus(kind, key, detail) {
    const alert = document.createElement('div');
    alert.className = `alert alert-${kind}`;
    const message = document.createElement('span');
    message.dataset.i18n = key;
    message.textContent = translate(key);
    alert.appendChild(message);
    if (detail) {
        const errorDetail = document.createElement('div');
        errorDetail.textContent = detail;
        alert.appendChild(errorDetail);
    }
    _("control-status").replaceChildren(alert);
}
showControlStatus('info', "common.loading");
loadControlValues()
    .then(() => {
        _("control-status").replaceChildren();
        controlFields.disabled = false;
    })
    .catch(() => showControlStatus('danger', "control.load_failed"));
