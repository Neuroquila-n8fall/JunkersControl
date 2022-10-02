var CurrentPath ="/";

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

function listFiles(path) {
    xmlhttp = new XMLHttpRequest();
    if (path) {
        xmlhttp.open("GET", `/api/listfiles?path=${path}`);
        CurrentPath = path;
    } else {
        xmlhttp.open("GET", "/api/listfiles");
        CurrentPath = "/";
    }

    xmlhttp.addEventListener("readystatechange", function () {
        if (this.readyState === 4) {
            var currentPath;
            var table = `<table class="table text-start"><thead><tr><th scope="col">Name</th><th scope="col">Size</th><th></th><th></th></tr></thead>`;
            table += `<tbody class="table-group-divider">`;
            var files = JSON.parse(this.responseText);
            for(var prop in files) {
                var entries = files[prop];
                entries.sort(sortByDirectory);
                for (var key in entries) {
                    currentPath = prop.split("/");
                    
                    var fileName = files[prop][key].Name;
                    var size = files[prop][key].Size;
                    var isDir = files[prop][key].Directory;                    
                    if(!isDir) {
                        table += `<tr><td>${fileName}</td>`;
                        table += `<td>${humanReadableSize(size)}</td>`;
                        table += `<td><button class="btn btn-primary btn-sm" onclick="downloadDeleteButton('${prop}${fileName}', 'download')">Download</button>`;
                        table += `<td><button class="ml-2 btn btn-danger btn-sm" onclick="deleteFile('${prop}${fileName}')">Delete</button></tr>`;
                    } else {
                        table += `<tr><td><div class="badge rounded-pill bg-secondary me-3">dir</div><a href="#" onclick="listFiles('${prop}${fileName}/')">${fileName}</a></td>`;
                        table += `<td/><td/><td/></tr>`;
                    }
                    
                }
                table += `</tbody></table>`;
                var breadCrumbs = `<nav style="--bs-breadcrumb-divider: '>';" aria-label="breadcrumb">
                <ol class="breadcrumb">
                  <li class="breadcrumb-item"><a href="#" onclick="listFiles()">Root</a></li>`;
                var itemPath = "";
                for (var item in currentPath) {
                    var segment = currentPath[item];
                    if (segment.trim().length !== 0) {
                        itemPath += `/${segment}`;
                        breadCrumbs += `<li class="breadcrumb-item"><a href="#" onclick="listFiles('${itemPath}/')">${segment}</a></li>`
                    }
                }
                breadCrumbs += `</ol></nav>`;
                _("details").innerHTML = breadCrumbs + `<br>` + table;
                break;
            }
        }
    });

    xmlhttp.send();
}
function sortByDirectory(a, b) {
    if (a.Directory)
        return -1;
    if (!a.Directory)
        return 1;
    return 0;
}

function deleteFile(path)
{
    var dialog = new bootstrap.Modal(_("confirm-delete-modal"), null);
    _("delete-confirm-filename").innerHTML = path;
    _("btn-confirm-delete").onclick = function () { downloadDeleteButton(path, "delete") };
    dialog.show();
}
function downloadDeleteButton(filename, action) {
    var urltocall = "/file?name=" + filename + "&action=" + action;
    xmlhttp = new XMLHttpRequest();
    if (action == "delete") {
        xmlhttp.open("GET", urltocall, false);
        xmlhttp.addEventListener("readystatechange", function () {
            if (this.readyState === 4) {
                var dialog = bootstrap.Modal.getInstance(_("confirm-delete-modal"));
                dialog.hide();
                listFiles(CurrentPath);
                showUsagePercentage();
                _("status").innerHTML = `<div class="alert alert-success alert-dismissible fade show" role="alert">
                <strong>File Deleted!</strong> The File ${filename} has been successfully deleted from ${CurrentPath}
                <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
                </div>`;
            }
        });
        xmlhttp.send();
    }
    if (action == "download") {
        _("status").innerHTML = "";
        window.open(urltocall, "_blank");
    }
}
function showUsagePercentage() {
    xmlhttp = new XMLHttpRequest();
    xmlhttp.open("GET", "/api/freestorage", false);
    xmlhttp.addEventListener("readystatechange", function () {
        if (this.readyState === 4) {
            var usage = JSON.parse(xmlhttp.responseText);
            _("progUsage").style = "width: " + usage.UsedPercent + "%;";
            _("progUsage").setAttribute('aria-valuenow', usage.UsedPercent);
            _("usedSpaceLabel").innerHTML = usage.UsedPercent + "%";
            _("space-free").innerHTML = humanReadableSize(usage.Free);
            _("space-occupied").innerHTML = humanReadableSize(usage.Used);
            _("space-total").innerHTML = humanReadableSize(usage.Total);

            // Colorize progressbar depending on usage
            _("progUsage").setAttribute("class", "progress-bar");
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
    });
    xmlhttp.send();
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
    _("progress").hidden = false;
    var file = _("file1").files[0];
    var fileName = file.name;
    var modFile = renameFile(file, `${CurrentPath}${fileName}`);
    var formdata = new FormData();
    formdata.append("file1", modFile);
    var ajax = new XMLHttpRequest();
    ajax.upload.addEventListener("progress", progressHandler, false);
    ajax.addEventListener("load", completeHandler, false); // doesnt appear to ever get called even upon success
    ajax.addEventListener("error", errorHandler, false);
    ajax.addEventListener("abort", abortHandler, false);
    ajax.open("POST", "/filemanager/upload");
    ajax.send(formdata);
}

function renameFile(originalFile, newName) {
    return new File([originalFile], newName, {
        type: originalFile.type,
        lastModified: originalFile.lastModified,
    });
}

function progressHandler(event) {
    _("loaded_n_total").innerHTML = "Uploaded " + humanReadableSize(event.loaded);
    var percent = (event.loaded / event.total) * 100;
    var roundedPercent = Math.round(percent);
    _("progressBar").style = "width: " + roundedPercent + "%;";
    _("progressBar").setAttribute('aria-valuenow',roundedPercent);
    _("progressBar").innerHTML = roundedPercent + "%";
    if (percent >= 100) {
        _("status").innerHTML = `<div class="badge rounded-pill bg-info text-dark">Please wait, writing file to filesystem</div>`;
    }
}
function completeHandler(event) {
    _("progress").hidden = true;
    _("progressBar").style.width = 0;
    _("progressBar").setAttribute('aria-valuenow', 0);
    _("progressBar").innerHTML = "0%";
    var file = _("file1").files[0];
    listFiles(CurrentPath);
    showUsagePercentage();
    _("status").innerHTML = `<div class="alert alert-success alert-dismissible fade show" role="alert">
    <strong>File Uploaded!</strong> The File ${file.name} has been successfully uploaded into "${CurrentPath}"
    <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
    </div>`;
    _("upload_form").reset();
    _("loaded_n_total").innerHTML = "";
}
function errorHandler(event) {
    _("status").innerHTML = `<div class="alert alert-danger alert-dismissible fade show" role="alert">
    <strong>File Upload Failed!</strong><button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
    </div>`;
    _("upload_form").reset();
    listFiles(CurrentPath);
}
function abortHandler(event) {
    _("status").innerHTML = `<div class="alert alert-info alert-dismissible fade show" role="alert">
    <strong>File Upload Aborted!</strong><button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
    </div>`;
    _("upload_form").reset();
    listFiles(CurrentPath);
}