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
            try {
                _(curLocation + "-link").classList.add("active");
            } catch (error) {
                console.log("Missing Nav-link to activate.");
            }
            
        }
    }

    xhr.open("GET", "frontend/navigation.html", true);
    xhr.setRequestHeader('Content-type', 'text/html');
    xhr.send();
}

/**
 * Gets all keys of a json object
 * 
 * @param {Object} jsonObj Input json to look through
 * @returns {Array<string>} An array of point separated keys as string
 */
function getDeepKeys(jsonObj) {
    let keys = [];
    for (var key in jsonObj) {
        if (typeof jsonObj[key] === "object" && !Array.isArray(jsonObj[key])) {
            var subkeys = getDeepKeys(jsonObj[key]);
            keys = keys.concat(subkeys.map(function (subkey) {
                return `${key}.${subkey}`;
            }));
        } else if (Array.isArray(jsonObj[key])) {
            for (var i = 0; i < jsonObj[key].length; i++) {
                var subkeys = getDeepKeys(jsonObj[key][i]);
                keys = keys.concat(subkeys.map((subkey) => `${key}[${i}].${subkey}`));
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
* @param {Object} jsonObj Json to use for searching the value.
* @param {Object} path the path to use to find the value.
* @returns {valueOfThePath|null}
*/
function jsonPathToValue(jsonObj, path) {
    if (!(jsonObj instanceof Object) || typeof (path) === "undefined") {
        throw `Not valid argument:jsonData:${jsonObj}, path:${path}`;
    }
    path = path.replace(/\[(\w+)\]/g, '.$1'); // convert indexes to properties
    path = path.replace(/^\./, ''); // strip a leading dot
    const pathArray = path.split('.');
    for (let i = 0, n = pathArray.length; i < n; ++i) {
        const key = pathArray[i];
        if (key in jsonObj) {
            if (jsonObj[key] !== null) {
                jsonObj = jsonObj[key];
            } else {
                return null;
            }
        } else {
            return key;
        }
    }
    return jsonObj;
}

/**
 * Converts a form into a nested json object. Form input fields need to have a 'name' attribute and nested values need to have a dot-separated name.
 * Example:
 * ```
 * <form>
 *  <input name="parent.subkey1"/>
 *  <input name="parent.subkey2.subkey3"/>
 * </form>
 * ```
 * @param {string} formId The Id of the form to process
 * @returns {Object} A json object containing the forms data
 */
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

function rebootButton() {
    _("statusdetails").innerHTML = "Invoking Reboot ...";
    const xhr = new XMLHttpRequest();
    xhr.open("GET", "/reboot", true);
    xhr.send();
    window.open("/reboot", "_self");
}