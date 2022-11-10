const EncryptionTypes = [
    "OPEN",         /**< authenticate mode : open */
    "WEP",              /**< authenticate mode : WEP */
    "WPA PSK",          /**< authenticate mode : WPA_PSK */
    "WPA2 PSK",         /**< authenticate mode : WPA2_PSK */
    "WPA - WPA2 PSK",     /**< authenticate mode : WPA_WPA2_PSK */
    "WPA2 ENTERPRISE",  /**< authenticate mode : WPA2_ENTERPRISE */
    "WPA3 PSK",         /**< authenticate mode : WPA3_PSK */
    "WPA2 WPA3_PSK",    /**< authenticate mode : WPA2_WPA3_PSK */
    "WAPI PSK",         /**< authenticate mode : WAPI_PSK */
    "MAX"
];


async function sendWifiConfig(event) {
    // Prevent the form from submitting.
    event.preventDefault();
    event.disabled = true;
    const formData = new FormData(event.target);
    const formJSON = Object.fromEntries(formData.entries());

    const json = JSON.stringify(formJSON);

    const response = await fetch('/api/config/wifi', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: json,
    });
    switch (response.status) {
        case 200:
            _("status").innerHTML = `<div class="alert alert-success alert-dismissible fade show" role="alert">
            <strong>Config Saved!</strong><br/>Configuration has been updated.<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
        case 400:
            _("status").innerHTML = `<div class="alert alert-danger alert-dismissible fade show" role="alert">
            <strong>Error</strong><br/>Configuration not saved because the received data was of a wrong format.<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
        default:
            _("status").innerHTML = `<div class="alert alert-danger alert-dismissible fade show" role="alert">
            <strong>Error</strong><br/>Received a status ${response.status} telling ${response.statusText}<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
    }
    event.disabled = false;
}

async function sendMqttConfig(event) {
    // Prevent the form from submitting.
    event.preventDefault();
    event.disabled = true;
    _("config-saving").hidden = false;
    _("save-config-label").innerHTML = "Saving MQTT Config...";
    _("save-config").disabled = true;
    const formData = new FormData(event.target);
    const formJSON = Object.fromEntries(formData.entries());

    const json = JSON.stringify(formJSON);

    const response = await fetch('/api/config/mqtt', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: json,
    });
    switch (response.status) {
        case 200:
            _("mqtt-status").innerHTML = `<div class="alert alert-success alert-dismissible fade show" role="alert">
            <strong>Config Saved!</strong><br/>Configuration has been updated.<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
        case 400:
            _("mqtt-status").innerHTML = `<div class="alert alert-danger alert-dismissible fade show" role="alert">
            <strong>Error</strong><br/>Configuration not saved because the received data was of a wrong format.<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
        default:
            _("mqtt-status").innerHTML = `<div class="alert alert-danger alert-dismissible fade show" role="alert">
            <strong>Error</strong><br/>Received a status ${response.status} telling ${response.statusText}<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
    }
    event.disabled = false;
    _("config-saving").hidden = true;
    _("save-config-label").innerHTML = "Save MQTT Config";
    _("save-config").disabled = false;
}

async function sendMqttTopicsConfig(event) {
    // Prevent the form from submitting.
    event.preventDefault();
    event.disabled = true;
    _("topics-saving").hidden = false;
    _("save-topics-label").innerHTML = "Saving Topics...";
    _("save-topics").disabled = true;
    const formData = new FormData(event.target);
    const formJSON = Object.fromEntries(formData.entries());

    const json = JSON.stringify(formJSON);

    const response = await fetch('/api/config/mqtt-topics', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: json,
    });
    switch (response.status) {
        case 200:
            _("mqtt-topics-status").innerHTML = `<div class="alert alert-success alert-dismissible fade show" role="alert">
            <strong>Config Saved!</strong><br/>Configuration has been updated.<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
        case 400:
            _("mqtt-topics-status").innerHTML = `<div class="alert alert-danger alert-dismissible fade show" role="alert">
            <strong>Error</strong><br/>Configuration not saved because the received data was of a wrong format.<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
        default:
            _("mqtt-topics-status").innerHTML = `<div class="alert alert-danger alert-dismissible fade show" role="alert">
            <strong>Error</strong><br/>Received a status ${response.status} telling ${response.statusText}<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
    }
    event.disabled = false;
    _("topics-saving").hidden = true;
    _("save-topics-label").innerHTML = "Save Topics";
    _("save-topics").disabled = false;
}

async function sendGeneralConfig(event) {
    // Prevent the form from submitting.
    event.preventDefault();
    event.disabled = true;
    _("form-fieldset").disabled = true;
    _("general-saving").hidden = false;
    _("save-general-label").innerHTML = "Saving Settings...";
    _("save-general").disabled = true;
    const formData = new FormData(event.target);

    // We have to get the values of the form by ourselves when options and selects are involved
    formData.append("heatingvalues", _("heatingvalues").checked);
    formData.append("watervalues", _("watervalues").checked);
    formData.append("auxvalues", _("auxvalues").checked);
    formData.append("overrideot", _("overrideot").checked);
    formData.append("tz", _("tz").value);
    formData.append("debug", _("debug").checked);
    formData.append("sniffing", _("sniffing").checked);
    const formJSON = Object.fromEntries(formData.entries());

    const json = JSON.stringify(formJSON);
    const response = await fetch('/api/config/general', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: json,
    });
    switch (response.status) {
        case 200:
            _("status").innerHTML = `<div class="alert alert-success alert-dismissible fade show" role="alert">
            <strong>Config Saved!</strong><br/>Configuration has been updated.<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
        case 400:
            _("status").innerHTML = `<div class="alert alert-danger alert-dismissible fade show" role="alert">
            <strong>Error</strong><br/>Configuration not saved because the received data was of a wrong format.<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
        default:
            _("status").innerHTML = `<div class="alert alert-danger alert-dismissible fade show" role="alert">
            <strong>Error</strong><br/>Received a status ${response.status} telling ${response.statusText}<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
    }
    event.disabled = false;
    _("general-saving").hidden = true;
    _("save-general-label").innerHTML = "Save Settings";
    _("save-general").disabled = false;
    _("form-fieldset").disabled = false;
}

async function sendCanbusConfig(json) {
    const response = await fetch('/api/config/canbus', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: json,
    });
    switch (response.status) {
        case 200:
            _("status").innerHTML = `<div class="alert alert-success alert-dismissible fade show" role="alert">
            <strong>Config Saved!</strong><br/>Configuration has been updated.<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
        case 400:
            _("status").innerHTML = `<div class="alert alert-danger alert-dismissible fade show" role="alert">
            <strong>Error</strong><br/>Configuration not saved because the received data was of a wrong format.<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
        default:
            _("status").innerHTML = `<div class="alert alert-danger alert-dismissible fade show" role="alert">
            <strong>Error</strong><br/>Received a status ${response.status} telling ${response.statusText}<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
            </div>`;
            break;
    }
}

async function getWifiConfig() {
    const response = await fetch("/api/config/wifi");
    return await response.json();
}

async function getWifiNetworks() {
    const response = await fetch("/api/wifi/networks");
    return await response.json();
}

async function getSystemStatus() {
    const response = await fetch("/api/info");
    return await response.json();
}

async function getMqttConfig() {
    const response = await fetch("/api/config/mqtt");
    return await response.json();
}

async function getMqttTopicsConfig() {
    const response = await fetch("/api/config/mqtt-topics");
    return await response.json();
}

async function getGeneralConfig() {
    const response = await fetch("/api/config/general");
    return await response.json();
}

async function getConfigJson(apiEndpoint) {
    const response = await fetch(apiEndpoint);
    return await response.json();
}


