loadNavigation();
const controlForm = _("control-form");
const booleanFields = new Set(["Enabled", "OverrideSetpoint", "DynamicAdaption", "ValveScaling", "Boost", "FastHeatup"]);

async function loadControlValues() {
    try {
        const runtime = await fetch('/api/runtime').then(r => r.json());
        Object.entries(runtime.Command).forEach(([key, value]) => {
            const el = _(key); if (!el) return;
            if (el.type === 'checkbox') el.checked = Boolean(value); else el.value = value;
        });
    } catch (error) { showControlStatus(false, "Control values could not be loaded."); }
}

controlForm.addEventListener('submit', async event => {
    event.preventDefault();
    const payload = {};
    new FormData(controlForm).forEach((value, key) => payload[key] = booleanFields.has(key) ? true : Number(value));
    booleanFields.forEach(key => payload[key] = _(key).checked);
    try {
        const response = await fetch('/api/control', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(payload)});
        const result = await response.json();
        if (!response.ok) throw new Error(result.msg || response.statusText);
        showControlStatus(true, "Control values applied.");
        await loadControlValues();
    } catch (error) { showControlStatus(false, error.message); }
});
function showControlStatus(ok, message) { _("control-status").innerHTML = `<div class="alert ${ok ? 'alert-success' : 'alert-danger'}">${translate(message)}</div>`; }
loadControlValues();
