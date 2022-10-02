function startUpdate(event) {
    // Prevent default HTML page refresh
    event.preventDefault();
    _("progress").hidden = false;
    var file = _("file1").files[0];
    var formdata = new FormData();
    formdata.append("file1", file);
    var ajax = new XMLHttpRequest();
    ajax.upload.addEventListener("progress", progressHandler, false);
    ajax.addEventListener("load", completeHandler, false); // doesnt appear to ever get called even upon success
    ajax.addEventListener("error", errorHandler, false);
    ajax.addEventListener("abort", abortHandler, false);
    ajax.open("POST", "/upload-firmware");
    ajax.send(formdata);
    _("upload_form").hidden = true;
}

function progressHandler(event) {
    //_("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes of " + event.total; // event.total doesnt show accurate total file size
    _("loaded_n_total").innerHTML = "Uploaded " + humanReadableSize(event.loaded);
    var percent = (event.loaded / event.total) * 100;
    var roundedPercent = Math.round(percent);
    _("progressBar").style = "width: " + roundedPercent + "%;";
    _("progressBar").setAttribute('aria-valuenow',roundedPercent);
    _("progressBar").innerHTML = roundedPercent + "%";
    if (percent >= 100) {
        _("status").innerHTML = `<div class="badge bg-primary">Please wait while the update is being finished.</div>`;
    }
}
function completeHandler(event) {
    _("progress").hidden = true;
    _("progressBar").style.width = 0;
    _("progressBar").setAttribute('aria-valuenow', 0);
    _("progressBar").innerHTML = "0%";
    _("status").innerHTML = `<div class="alert alert-success alert-dismissible fade show" role="alert">
    <strong>Firmware Uploaded! </strong>Firmware is installed and will become active on the next reboot.<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
    </div>`;
    _("upload_form").reset();
    _("loaded_n_total").innerHTML = "";
    _("upload_form").hidden = false;
}
function errorHandler(event) {
    _("status").innerHTML = `<div class="alert alert-danger alert-dismissible fade show" role="alert">
    <strong>Firmware Update Failed! </strong>Consider rebooting and retry the update afterwards.<button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
    </div>`;
    _("upload_form").reset();
    _("upload_form").hidden = false;
}
function abortHandler(event) {
    _("status").innerHTML = `<div class="alert alert-info alert-dismissible fade show" role="alert">
    <strong>Firmware Update Aborted! </strong><button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
    </div>`;
    _("upload_form").reset();
    _("upload_form").hidden = false;
}