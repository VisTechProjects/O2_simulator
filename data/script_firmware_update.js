// script_firmware_update.js

let uploadInProgress = false;

document.addEventListener("DOMContentLoaded", () => {
    document.querySelectorAll('.container').forEach(el => el.classList.add('fade-in'));

    const dropArea = document.getElementById("drop-area");
    const fileInput = document.getElementById("file_input");
    const progressBar = document.getElementById("progress_bar");
    const progressContainer = document.getElementById("progress_container");
    const statusText = document.getElementById("status");

    fileInput.addEventListener('click', () => {
        fileInput.value = '';
    });

    dropArea.addEventListener("click", () => fileInput.click());

    dropArea.addEventListener("dragover", (e) => {
        e.preventDefault();
        dropArea.classList.add("dragover");
    });

    dropArea.addEventListener("dragleave", () => dropArea.classList.remove("dragover"));

    dropArea.addEventListener("drop", (e) => {
        e.preventDefault();
        dropArea.classList.remove("dragover");
        fileInput.value = "";
        const file = e.dataTransfer.files[0];
        handleUpload(file);
    });

    fileInput.addEventListener("change", () => {
        const file = fileInput.files[0];
        handleUpload(file);
    });

    function handleUpload(file) {
        if (uploadInProgress) return;

        progressContainer.style.display = "block";
        progressBar.style.width = "0%";
        progressBar.classList.remove("error");
        statusText.textContent = "";

        uploadInProgress = true;

        if (!file || !file.name.endsWith(".bin")) {
            statusText.textContent = "Only .bin files are allowed.";
            uploadInProgress = false;
            fileInput.value = '';
            return;
        }

        if (!["firmware.bin", "spiffs.bin"].includes(file.name.toLowerCase())) {
            statusText.innerHTML = `${file.name} is not a valid file.<br>Must be firmware.bin or spiffs.bin`;
            uploadInProgress = false;
            fileInput.value = '';
            return;
        }

        const isSPIFFS = file.name.toLowerCase().includes("spiffs");
        statusText.textContent = `Uploading ${isSPIFFS ? "SPIFFS" : "Firmware"}...`;

        const xhr = new XMLHttpRequest();
        xhr.open("POST", "/update");

        xhr.upload.onprogress = function (e) {
            const pct = Math.round((e.loaded / e.total) * 100);
            progressBar.style.width = pct + "%";
        };

        xhr.onload = function () {
            uploadInProgress = false;
            progressBar.classList.remove("uploading");
            fileInput.value = '';

            if (xhr.status === 200 && xhr.responseText === "OK") {
                progressBar.style.width = "100%";
                showModal("success", "Update Successful", "Your device is now rebooting.");
            } else if (xhr.status === 400 && xhr.responseText === "INVALID_FILE") {
                statusText.textContent = "Invalid file: must be firmware.bin or spiffs.bin";
                progressBar.style.width = "0%";
                progressBar.classList.add("error");
            } else {
                statusText.textContent = "OTA update failed. Please try again.";
                progressBar.style.width = "0%";
                progressBar.classList.add("error");
            }
        };

        xhr.onerror = function () {
            uploadInProgress = false;
            statusText.textContent = "Upload failed (network error)";
            progressBar.style.width = "0%";
            progressBar.classList.add("error");
            fileInput.value = '';
        };

        const form = new FormData();
        form.append("update", file, file.name.toLowerCase());
        xhr.send(form);
    }

    function showModal(type, title, message) {
        const backdrop = document.createElement("div");
        backdrop.classList.add("modal-backdrop");
        document.body.append(backdrop);

        const modal = document.createElement("div");
        modal.classList.add("modal-box", type);
        modal.innerHTML = `
        <h3>${title}</h3>
        <p>${message}</p>
        <button>OK</button>
        `;

        backdrop.append(modal);
        backdrop.style.display = "flex";

        modal.querySelector("button").addEventListener("click", () => {
            backdrop.remove();
            if (type === "success") window.location.href = "/";
        });
    }
});
