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
    var nav = _("navigation-container");
    xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function (e) {
        if (xhr.readyState == 4 && xhr.status == 200) {
            nav.innerHTML = xhr.responseText;
            var curLocation = window.location.pathname
                .replace("/", "")
                .trim();
            if(curLocation.length == 0) {
                curLocation = "home";
            }
            _(curLocation + "-link").classList.add("active");
        }
    }

    xhr.open("GET", "frontend/navigation.html", true);
    xhr.setRequestHeader('Content-type', 'text/html');
    xhr.send();
}