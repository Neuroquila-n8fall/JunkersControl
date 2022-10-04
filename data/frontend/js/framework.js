function _(el) {
    return document.getElementById(el);
}

function humanReadableSize(bytes) {
    if (bytes < 1024)
        return bytes + " B";
    else if (bytes < (1024 * 1024))
        return (bytes / 1024.0).toFixed(2) + " KB";
    else if (bytes < (1024 * 1024 * 1024))
        return (bytes / 1024.0 / 1024.0).toFixed(2) + " MB";
    else
        return (bytes / 1024.0 / 1024.0 / 1024.0).toFixed(2) + " GB";
}

function loadNavigation() {
    const nav = _("navigation-container");
    let xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function (e) {
        if (xhr.readyState === 4 && xhr.status === 200) {
            nav.innerHTML = xhr.responseText;
            let curLocation = window.location.pathname
                .replace("/", "")
                .trim();
            if (curLocation.length === 0) {
                curLocation = "home";
            }
            _(curLocation + "-link").classList.add("active");
        }
    }

    xhr.open("GET", "frontend/navigation.html", true);
    xhr.setRequestHeader('Content-type', 'text/html');
    xhr.send();
}

function getDeepKeys(obj) {
    var keys = [];
    for (var key in obj) {
        if (typeof obj[key] === "object" && !Array.isArray(obj[key])) {
            var subkeys = getDeepKeys(obj[key]);
            keys = keys.concat(subkeys.map(function (subkey) {
                return key + "." + subkey;
            }));
        } else if (Array.isArray(obj[key])) {
            for (var i = 0; i < obj[key].length; i++) {
                var subkeys = getDeepKeys(obj[key][i]);
                keys = keys.concat(subkeys.map(function (subkey) {
                    return key + "[" + i + "]" + "." + subkey;
                }));
            }
        } else {

            keys.push(key);
        }
    }
    return keys;
}

/**
* Converts a string path to a value that is existing in a json object.
* 
* @param {Object} jsonData Json data to use for searching the value.
* @param {Object} path the path to use to find the value.
* @returns {valueOfThePath|null}
*/
function jsonPathToValue(jsonData, path) {
    if (!(jsonData instanceof Object) || typeof (path) === "undefined") {
        throw "Not valid argument:jsonData:" + jsonData + ", path:" + path;
    }
    path = path.replace(/\[(\w+)\]/g, '.$1'); // convert indexes to properties
    path = path.replace(/^\./, ''); // strip a leading dot
    var pathArray = path.split('.');
    for (var i = 0, n = pathArray.length; i < n; ++i) {
        var key = pathArray[i];
        if (key in jsonData) {
            if (jsonData[key] !== null) {
                jsonData = jsonData[key];
            } else {
                return null;
            }
        } else {
            return key;
        }
    }
    return jsonData;
}

function serializeForm(formId) {
    const elements = document.querySelectorAll(`#${formId} input`);
    const data = {};
    for (let i = 0; i < elements.length; i++) {
        const el = elements[i];
        var val = el.value;
        if (!val) val = "";
        const fullName = el.getAttribute("name");
        if (!fullName) continue;
        const fullNameParts = fullName.split('.');
        let prefix = '';
        let stack = data;
        for (let k = 0; k < fullNameParts.length - 1; k++) {
            prefix = fullNameParts[k];
            if (!stack[prefix]) {
                stack[prefix] = {};
            }
            stack = stack[prefix];
        }
        prefix = fullNameParts[fullNameParts.length - 1];
        if (stack[prefix]) {

            var newVal = `${stack[prefix]},${val}`;
            stack[prefix] += newVal;
        } else {
            stack[prefix] = val;
        }
    }
    return data;
}