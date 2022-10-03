var EncryptionTypes = [
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
]


async function sendWifiConfig(event) {
    // Prevent the form from submitting.
    event.preventDefault();
    event.disabled = true;
    const formData = new FormData(event.target);
    const formJSON = Object.fromEntries(formData.entries());

    var json = JSON.stringify(formJSON);

    var response = await fetch('/api/config/wifi', {
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

    var json = JSON.stringify(formJSON);

    var response = await fetch('/api/config/mqtt', {
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

    var json = JSON.stringify(formJSON);

    var response = await fetch('/api/config/mqtt-topics', {
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

async function getWifiConfig() {
    var response = await fetch("/api/config/wifi");
    var config = await response.json();
    return config;
}

async function getWifiNetworks() {
    var response = await fetch("/api/wifi/networks");
    var networks = await response.json();
    return networks;
}

async function getSystemStatus() {
    var response = await fetch("/api/info");
    var status = await response.json();
    return status;
}

async function getMqttConfig() {
    var response = await fetch("/api/config/mqtt");
    var result = await response.json();
    return result;
}

async function getMqttTopicsConfig() {
    var response = await fetch("/api/config/mqtt-topics");
    var result = await response.json();
    return result;
}