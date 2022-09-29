function _(el) {
    return document.getElementById(el);
}
function rebootButton() {
    document.getElementById("statusdetails").innerHTML = "Invoking Reboot ...";
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/reboot", true);
    xhr.send();
    window.open("/reboot", "_self");
}
function listFilesButton() {
    xmlhttp = new XMLHttpRequest();
    xmlhttp.open("GET", "/listfiles", false);
    xmlhttp.send();
    _("detailsheader").innerHTML = "<h3>Files<h3>";
    _("details").innerHTML = xmlhttp.responseText;
}
function downloadDeleteButton(filename, action) {
    var urltocall = "/file?name=" + filename + "&action=" + action;
    xmlhttp = new XMLHttpRequest();
    if (action == "delete") {
        xmlhttp.open("GET", urltocall, false);
        xmlhttp.send();
        _("status").innerHTML = xmlhttp.responseText;
        xmlhttp.open("GET", "/listfiles", false);
        xmlhttp.send();
        _("details").innerHTML = xmlhttp.responseText;
    }
    if (action == "download") {
        _("status").innerHTML = "";
        window.open(urltocall, "_blank");
    }
}
function showUsagePercentage() {
    xmlhttp = new XMLHttpRequest();
    xmlhttp.open("GET", "/api/freestorage", false);
    xmlhttp.send();
    var usage = JSON.parse(xmlhttp.responseText);
    _("progUsage").style = "width: " + usage.UsedPercent + "%;";
    _("progUsage").setAttribute('aria-valuenow', usage.UsedPercent);
    _("usedSpaceLabel").innerHTML = usage.UsedPercent + "%";
    if (usage.UsedPercent <= 50) {
        _("progUsage").classList.add("bg-success");
        return;
    }
    if (usage.UsedPercent >= 50) {
        _("progUsage").classList.add("bg-warning");
        return;
    }
    if (usage.UsedPercent >= 90) {
        _("progUsage").classList.add("bg-alert");
        return;
    }    
}
function showUploadButtonFancy() {
    _("detailsheader").innerHTML = "<h3>Upload File<h3>"
    _("status").innerHTML = "";
    var uploadform =
        "<form id=\"upload_form\" enctype=\"multipart/form-data\" method=\"post\">" +
        "<div class=\"mb-5\">" + 
            "<label for=\"file1\" class=\"form-label\">Upload File<\/label>" + 
            "<input class=\"form-control\" type=\"file\" id=\"file1\" name=\"file1\" onchange=\"uploadFile()\"><\/div>" +
            "<div class=\"progress\" id=\"progress\" hidden>" + 
                "<div class=\"progress-bar\" role=\"progressbar\" id=\"progressBar\" aria-label=\"Upload Progress\" style=\"display: none !important;\" aria-valuenow=\"0\" aria-valuemin=\"0\" aria-valuemax=\"100\"><\/div>" + 
            "<\/div>" +
            "<div id=\"loaded_n_total\"></div>" +
        "<\/div>" +
        "</form>";
    _("details").innerHTML = uploadform;
}
function uploadFile() {
    _("progress").setAttribute("hidden", false);
    var file = _("file1").files[0];
    // alert(file.name+" | "+file.size+" | "+file.type);
    var formdata = new FormData();
    formdata.append("file1", file);
    var ajax = new XMLHttpRequest();
    ajax.upload.addEventListener("progress", progressHandler, false);
    ajax.addEventListener("load", completeHandler, false); // doesnt appear to ever get called even upon success
    ajax.addEventListener("error", errorHandler, false);
    ajax.addEventListener("abort", abortHandler, false);
    ajax.open("POST", "/");
    ajax.send(formdata);
}
function progressHandler(event) {
    //_("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes of " + event.total; // event.total doesnt show accurate total file size
    _("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes";
    var percent = (event.loaded / event.total) * 100;
    var roundedPercent = Math.round(percent);
    _("progressBar").style = "width: " + roundedPercent + "%;";
    _("progressBar").setAttribute('aria-valuenow',roundedPercent);
    _("progressBar").innerHTML = roundedPercent + "%";
    _("status").innerHTML = roundedPercent + "% uploaded... please wait";
    if (percent >= 100) {
        _("status").innerHTML = "Please wait, writing file to filesystem";
    }
}
function completeHandler(event) {
    _("status").innerHTML = "Upload Complete";
    _("progress").setAttribute("hidden", true);
    _("progressBar").style.width = 0;
    _("progressBar").setAttribute('aria-valuenow', 0);
    _("progressBar").innerHTML = "0%";
    xmlhttp = new XMLHttpRequest();
    xmlhttp.open("GET", "/listfiles", false);
    xmlhttp.send();
    document.getElementById("status").innerHTML = "File Uploaded";
    document.getElementById("detailsheader").innerHTML = "<h3>Files<h3>";
    document.getElementById("details").innerHTML = xmlhttp.responseText;
}
function errorHandler(event) {
    _("status").innerHTML = "Upload Failed";
}
function abortHandler(event) {
    _("status").innerHTML = "inUpload Aborted";
}