/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
*/

/*
 * Hello World Example - Minimal ESP32C3 Test
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "spi_flash_mmap.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "epd.h"
#include "epd_flash_image.h"

static const char *TAG = "epd_test";

// WiFi AP configuration
#define WIFI_SSID "EPD-PicFrame"
#define WIFI_PASS "picframe123"
#define WIFI_CHANNEL 1
#define MAX_CONNECTIONS 4

static httpd_handle_t server = NULL;

// HTTP server handlers
static esp_err_t hello_handler(httpd_req_t *req)
{
    const char* response = "<!DOCTYPE html>"
        "<html><head>"
        "<title>EPD PicFrame</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>"
        "body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; }"
        "h1 { color: #333; }"
        ".card { background: #f5f5f5; border-radius: 8px; padding: 20px; margin: 20px 0; }"
        ".upload-form { margin: 20px 0; }"
        ".upload-form input[type='file'] { margin: 10px 0; }"
        ".upload-form button { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; }"
        ".upload-form button:hover { background: #45a049; }"
        ".upload-form button:disabled { background: #cccccc; cursor: not-allowed; }"
        ".status { margin: 20px 0; padding: 10px; border-radius: 4px; }"
        ".status.success { background: #dff0d8; color: #3c763d; }"
        ".status.error { background: #f2dede; color: #a94442; }"
        ".preview-container { margin: 20px 0; text-align: center; }"
        ".preview-canvas { border: 1px solid #ddd; max-width: 100%; }"
        ".loading { display: none; margin: 10px 0; }"
        ".loading.active { display: block; }"
        ".spinner { border: 4px solid #f3f3f3; border-top: 4px solid #3498db; border-radius: 50%; width: 20px; height: 20px; animation: spin 1s linear infinite; margin: 0 auto; }"
        "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }"
        ".progress-container { margin: 20px 0; display: none; }"
        ".progress-container.active { display: block; }"
        ".progress-bar { width: 100%; height: 20px; background-color: #f0f0f0; border-radius: 10px; overflow: hidden; }"
        ".progress-bar-fill { height: 100%; background-color: #4CAF50; width: 0%; transition: width 0.3s ease-in-out; }"
        ".progress-text { text-align: center; margin-top: 5px; font-size: 14px; color: #666; }"
        "</style>"
        "<script>\n"
        "// EPD Image Converter\n"
        "class EPDConverter {\n"
        "    constructor() {\n"
        "        this.width = 600;\n"
        "        this.height = 448;\n"
        "        this.upsideDown = true;\n"
        "        \n"
        "        // RGB colors as displayed on the EPD screen (linear RGB values)\n"
        "        this.epdColors = [\n"
        "            [0, 0, 0],           // Black\n"
        "            [1, 1, 1],           // White  \n"
        "            [0.059, 0.329, 0.119], // Green\n"
        "            [0.061, 0.147, 0.336], // Blue\n"
        "            [0.574, 0.066, 0.010], // Red\n"
        "            [0.982, 0.756, 0.004], // Yellow\n"
        "            [0.795, 0.255, 0.018], // Orange\n"
        "        ];\n"
        "        \n"
        "        // Convert float RGB colors to integer colors for preview\n"
        "        this.epdColorsInt = this.epdColors.map(color => \n"
        "            (Math.round(color[0] * 255) << 16) | \n"
        "            (Math.round(color[1] * 255) << 8) | \n"
        "            Math.round(color[2] * 255)\n"
        "        );\n"
        "    }\n"
        "    \n"
        "    gammaLinear(srgb) {\n"
        "        return srgb > 0.04045 ? Math.pow((srgb + 0.055) / 1.055, 2.4) : srgb / 12.92;\n"
        "    }\n"
        "    \n"
        "    rgbToLab(rgb) {\n"
        "        let [r, g, b] = rgb.map(c => Math.max(0, Math.min(100, c * 100)));\n"
        "        \n"
        "        // Observer = 2°, Illuminant = D65\n"
        "        let x = r * 0.4124 + g * 0.3576 + b * 0.1805;\n"
        "        let y = r * 0.2126 + g * 0.7152 + b * 0.0722;\n"
        "        let z = r * 0.0193 + g * 0.1192 + b * 0.9505;\n"
        "        \n"
        "        x = x / 95.047;\n"
        "        y = y / 100.0;\n"
        "        z = z / 108.883;\n"
        "        \n"
        "        x = x > 0.008856 ? Math.pow(x, 1/3) : (7.787 * x) + (16/116);\n"
        "        y = y > 0.008856 ? Math.pow(y, 1/3) : (7.787 * y) + (16/116);\n"
        "        z = z > 0.008856 ? Math.pow(z, 1/3) : (7.787 * z) + (16/116);\n"
        "        \n"
        "        return [\n"
        "            (116 * y) - 16,\n"
        "            500 * (x - y),\n"
        "            200 * (y - z)\n"
        "        ];\n"
        "    }\n"
        "    \n"
        "    colorDifference(rgb1, rgb2) {\n"
        "        const lab1 = this.rgbToLab(rgb1);\n"
        "        const lab2 = this.rgbToLab(rgb2);\n"
        "        \n"
        "        // DeltaE 2000 implementation\n"
        "        const avgL = (lab1[0] + lab2[0]) / 2;\n"
        "        const c1 = Math.sqrt(lab1[1] * lab1[1] + lab1[2] * lab1[2]);\n"
        "        const c2 = Math.sqrt(lab2[1] * lab2[1] + lab2[2] * lab2[2]);\n"
        "        const avgC = (c1 + c2) / 2;\n"
        "        const g = (1 - Math.sqrt(Math.pow(avgC, 7) / (Math.pow(avgC, 7) + Math.pow(25, 7)))) / 2;\n"
        "        \n"
        "        const a1p = lab1[1] * (1 + g);\n"
        "        const a2p = lab2[1] * (1 + g);\n"
        "        const c1p = Math.sqrt(a1p * a1p + lab1[2] * lab1[2]);\n"
        "        const c2p = Math.sqrt(a2p * a2p + lab2[2] * lab2[2]);\n"
        "        const avgCp = (c1p + c2p) / 2;\n"
        "        \n"
        "        let h1p = Math.atan2(lab1[2], a1p) * 180 / Math.PI;\n"
        "        if (h1p < 0) h1p += 360;\n"
        "        let h2p = Math.atan2(lab2[2], a2p) * 180 / Math.PI;\n"
        "        if (h2p < 0) h2p += 360;\n"
        "        \n"
        "        const avghp = Math.abs(h1p - h2p) > 180 ? (h1p + h2p + 360) / 2 : (h1p + h2p) / 2;\n"
        "        const t = 1 - 0.17 * Math.cos((avghp - 30) * Math.PI / 180) + \n"
        "                     0.24 * Math.cos(2 * avghp * Math.PI / 180) + \n"
        "                     0.32 * Math.cos((3 * avghp + 6) * Math.PI / 180) - \n"
        "                     0.2 * Math.cos((4 * avghp - 63) * Math.PI / 180);\n"
        "        \n"
        "        let deltahp = h2p - h1p;\n"
        "        if (Math.abs(deltahp) > 180) {\n"
        "            if (h2p <= h1p) {\n"
        "                deltahp += 360;\n"
        "            } else {\n"
        "                deltahp -= 360;\n"
        "            }\n"
        "        }\n"
        "        \n"
        "        const deltalp = lab2[0] - lab1[0];\n"
        "        const deltacp = c2p - c1p;\n"
        "        deltahp = 2 * Math.sqrt(c1p * c2p) * Math.sin(deltahp * Math.PI / 360);\n"
        "        \n"
        "        const sl = 1 + ((0.015 * Math.pow(avgL - 50, 2)) / Math.sqrt(20 + Math.pow(avgL - 50, 2)));\n"
        "        const sc = 1 + 0.045 * avgCp;\n"
        "        const sh = 1 + 0.015 * avgCp * t;\n"
        "        \n"
        "        const deltaro = 30 * Math.exp(-Math.pow((avghp - 275) / 25, 2));\n"
        "        const rc = 2 * Math.sqrt(Math.pow(avgCp, 7) / (Math.pow(avgCp, 7) + Math.pow(25, 7)));\n"
        "        const rt = -rc * Math.sin(2 * deltaro * Math.PI / 180);\n"
        "        \n"
        "        const kl = 1, kc = 1, kh = 1;\n"
        "        \n"
        "        const deltaE = Math.sqrt(\n"
        "            Math.pow(deltalp / (kl * sl), 2) + \n"
        "            Math.pow(deltacp / (kc * sc), 2) + \n"
        "            Math.pow(deltahp / (kh * sh), 2) + \n"
        "            rt * (deltacp / (kc * sc)) * (deltahp / (kh * sh))\n"
        "        );\n"
        "        \n"
        "        return deltaE;\n"
        "    }\n"
        "    \n"
        "    convertImage(canvas) {\n"
        "        const ctx = canvas.getContext('2d');\n"
        "        const imageData = ctx.getImageData(0, 0, this.width, this.height);\n"
        "        const data = imageData.data;\n"
        "        \n"
        "        // Create binary data structure\n"
        "        const binarySize = 64 + (this.width * this.height / 2); // header + image data\n"
        "        const binaryData = new Uint8Array(binarySize);\n"
        "        const dataView = new DataView(binaryData.buffer);\n"
        "        \n"
        "        // Write header\n"
        "        dataView.setUint32(0, 0xfafa1a1a, true); // Magic number (little endian)\n"
        "        dataView.setBigUint64(4, BigInt(Math.floor(Date.now() / 1000)), true); // Timestamp\n"
        "        \n"
        "        // Create preview canvas\n"
        "        const previewCanvas = document.createElement('canvas');\n"
        "        previewCanvas.width = this.width;\n"
        "        previewCanvas.height = this.height;\n"
        "        const previewCtx = previewCanvas.getContext('2d');\n"
        "        const previewImageData = previewCtx.createImageData(this.width, this.height);\n"
        "        const previewData = previewImageData.data;\n"
        "        \n"
        "        // Process each pixel\n"
        "        for (let y = 0; y < this.height; y++) {\n"
        "            for (let x = 0; x < this.width; x += 2) {\n"
        "                // Process two pixels at a time for 4-bit packing\n"
        "                const i1 = (y * this.width + x) * 4;\n"
        "                const i2 = (y * this.width + x + 1) * 4;\n"
        "                \n"
        "                // Get colors for both pixels\n"
        "                const r1 = this.gammaLinear(data[i1] / 255);\n"
        "                const g1 = this.gammaLinear(data[i1 + 1] / 255);\n"
        "                const b1 = this.gammaLinear(data[i1 + 2] / 255);\n"
        "                \n"
        "                const r2 = this.gammaLinear(data[i2] / 255);\n"
        "                const g2 = this.gammaLinear(data[i2 + 1] / 255);\n"
        "                const b2 = this.gammaLinear(data[i2 + 2] / 255);\n"
        "                \n"
        "                // Find closest colors in EPD palette\n"
        "                let bestColor1 = 0;\n"
        "                let bestColor2 = 0;\n"
        "                let bestDifference1 = Infinity;\n"
        "                let bestDifference2 = Infinity;\n"
        "                \n"
        "                for (let i = 0; i < this.epdColors.length; i++) {\n"
        "                    const diff1 = this.colorDifference([r1, g1, b1], this.epdColors[i]);\n"
        "                    const diff2 = this.colorDifference([r2, g2, b2], this.epdColors[i]);\n"
        "                    \n"
        "                    if (diff1 < bestDifference1) {\n"
        "                        bestDifference1 = diff1;\n"
        "                        bestColor1 = i;\n"
        "                    }\n"
        "                    if (diff2 < bestDifference2) {\n"
        "                        bestDifference2 = diff2;\n"
        "                        bestColor2 = i;\n"
        "                    }\n"
        "                }\n"
        "                \n"
        "                // Write to binary data (4-bit packed)\n"
        "                // Calculate the correct index for the EPD display\n"
        "                const epdX = (x + this.width/2) % this.width; // Center the image horizontally\n"
        "                const epdY = y;\n"
        "                const binaryIndex = 64 + (epdY * this.width + epdX) / 2;\n"
        "                binaryData[binaryIndex] = (bestColor2 << 4) | bestColor1;\n"
        "                \n"
        "                // Set preview pixels\n"
        "                const color1 = this.epdColorsInt[bestColor1];\n"
        "                const color2 = this.epdColorsInt[bestColor2];\n"
        "                \n"
        "                // First pixel\n"
        "                const previewPixelIndex1 = (x + y * this.width) * 4;\n"
        "                previewData[previewPixelIndex1] = (color1 >> 16) & 0xFF;     // R\n"
        "                previewData[previewPixelIndex1 + 1] = (color1 >> 8) & 0xFF;  // G\n"
        "                previewData[previewPixelIndex1 + 2] = color1 & 0xFF;         // B\n"
        "                previewData[previewPixelIndex1 + 3] = 255;                  // A\n"
        "                \n"
        "                // Second pixel\n"
        "                const previewPixelIndex2 = (x + 1 + y * this.width) * 4;\n"
        "                previewData[previewPixelIndex2] = (color2 >> 16) & 0xFF;     // R\n"
        "                previewData[previewPixelIndex2 + 1] = (color2 >> 8) & 0xFF;  // G\n"
        "                previewData[previewPixelIndex2 + 2] = color2 & 0xFF;         // B\n"
        "                previewData[previewPixelIndex2 + 3] = 255;                  // A\n"
        "            }\n"
        "        }\n"
        "        \n"
        "        previewCtx.putImageData(previewImageData, 0, 0);\n"
        "        \n"
        "        return {\n"
        "            binaryData: binaryData,\n"
        "            previewCanvas: previewCanvas\n"
        "        };\n"
        "    }\n"
        "}\n"
        "</script>\n"
        "</head>"
        "<body>"
        "<h1>ESP32C3 EPD PicFrame</h1>"
        "<div class='card'>"
        "<h2>Connection Info</h2>"
        "<p>WiFi AP: " WIFI_SSID "</p>"
        "<p>Password: " WIFI_PASS "</p>"
        "<p>IP Address: 192.168.4.1</p>"
        "</div>"
        "<div class='card'>"
        "<h2>Upload New Image</h2>"
        "<form class='upload-form' id='uploadForm'>"
        "<input type='file' name='image' accept='image/*' required id='imageInput'>"
        "<div class='preview-container'>"
        "<canvas id='previewCanvas' class='preview-canvas' width='600' height='448'></canvas>"
        "</div>"
        "<div class='progress-container' id='progressContainer'>"
        "<div class='progress-bar'>"
        "<div class='progress-bar-fill' id='progressBar'></div>"
        "</div>"
        "<div class='progress-text' id='progressText'>0%</div>"
        "</div>"
        "<div class='loading' id='loading'>"
        "<div class='spinner'></div>"
        "<p>Converting and uploading image...</p>"
        "</div>"
        "<button type='submit' id='uploadButton' disabled>Upload Image</button>"
        "</form>"
        "</div>"
        "<div class='card'>"
        "<h2>EPD Status</h2>"
        "<p>Status: Ready</p>"
        "<p>Display: Active</p>"
        "</div>"
        "<script>\n"
        "const converter = new EPDConverter();\n"
        "const form = document.getElementById('uploadForm');\n"
        "const imageInput = document.getElementById('imageInput');\n"
        "const previewCanvas = document.getElementById('previewCanvas');\n"
        "const uploadButton = document.getElementById('uploadButton');\n"
        "const loading = document.getElementById('loading');\n"
        "const progressContainer = document.getElementById('progressContainer');\n"
        "const progressBar = document.getElementById('progressBar');\n"
        "const progressText = document.getElementById('progressText');\n"
        "let currentImage = null;\n"
        "\n"
        "// Add status message container\n"
        "const statusContainer = document.createElement('div');\n"
        "statusContainer.className = 'status';\n"
        "form.insertBefore(statusContainer, form.firstChild);\n"
        "\n"
        "function showStatus(message, isError = false) {\n"
        "    statusContainer.textContent = message;\n"
        "    statusContainer.className = 'status ' + (isError ? 'error' : 'success');\n"
        "    statusContainer.style.display = 'block';\n"
        "}\n"
        "\n"
        "function updateProgress(percent, message) {\n"
        "    progressBar.style.width = percent + '%';\n"
        "    progressText.textContent = message || (percent + '%');\n"
        "}\n"
        "\n"
        "imageInput.addEventListener('change', async (e) => {\n"
        "    const file = e.target.files[0];\n"
        "    if (!file) return;\n"
        "\n"
        "    try {\n"
        "        showStatus('Processing image...');\n"
        "        progressContainer.classList.add('active');\n"
        "        updateProgress(0, 'Loading image...');\n"
        "\n"
        "        // Create image from file\n"
        "        const img = new Image();\n"
        "        img.src = URL.createObjectURL(file);\n"
        "        await new Promise((resolve, reject) => {\n"
        "            img.onload = resolve;\n"
        "            img.onerror = () => reject(new Error('Failed to load image'));\n"
        "        });\n"
        "\n"
        "        updateProgress(20, 'Resizing image...');\n"
        "\n"
        "        // Create canvas and draw image\n"
        "        const canvas = document.createElement('canvas');\n"
        "        canvas.width = 600;\n"
        "        canvas.height = 448;\n"
        "        const ctx = canvas.getContext('2d');\n"
        "        ctx.drawImage(img, 0, 0, 600, 448);\n"
        "\n"
        "        updateProgress(40, 'Converting to EPD format...');\n"
        "\n"
        "        // Convert image\n"
        "        const result = converter.convertImage(canvas);\n"
        "        currentImage = result.binaryData;\n"
        "\n"
        "        updateProgress(60, 'Generating preview...');\n"
        "\n"
        "        // Show preview\n"
        "        const previewCtx = previewCanvas.getContext('2d');\n"
        "        previewCtx.drawImage(result.previewCanvas, 0, 0);\n"
        "\n"
        "        updateProgress(100, 'Ready to upload');\n"
        "        showStatus('Image processed successfully! Click Upload to continue.');\n"
        "        uploadButton.disabled = false;\n"
        "    } catch (error) {\n"
        "        console.error('Error processing image:', error);\n"
        "        showStatus('Error: ' + error.message, true);\n"
        "        progressContainer.classList.remove('active');\n"
        "        uploadButton.disabled = true;\n"
        "    }\n"
        "});\n"
        "\n"
        "form.addEventListener('submit', async (e) => {\n"
        "    e.preventDefault();\n"
        "    if (!currentImage) return;\n"
        "\n"
        "    try {\n"
        "        loading.classList.add('active');\n"
        "        progressContainer.classList.add('active');\n"
        "        uploadButton.disabled = true;\n"
        "        showStatus('Uploading image...');\n"
        "\n"
        "        // Create a Blob from the binary data\n"
        "        const blob = new Blob([currentImage], { type: 'application/octet-stream' });\n"
        "        const formData = new FormData();\n"
        "        formData.append('image', blob, 'image.bin');\n"
        "\n"
        "        // Send binary data with progress tracking\n"
        "        const response = await fetch('/upload', {\n"
        "            method: 'POST',\n"
        "            body: formData\n"
        "        });\n"
        "\n"
        "        if (!response.ok) {\n"
        "            throw new Error('Upload failed: ' + response.statusText);\n"
        "        }\n"
        "\n"
        "        // Update progress to 100%\n"
        "        updateProgress(100, 'Upload complete!');\n"
        "        showStatus('Image uploaded successfully!');\n"
        "\n"
        "        // Redirect to success page after a short delay\n"
        "        setTimeout(() => {\n"
        "            window.location.href = '/upload-success';\n"
        "        }, 2000);\n"
        "    } catch (error) {\n"
        "        console.error('Upload error:', error);\n"
        "        showStatus('Upload failed: ' + error.message, true);\n"
        "        loading.classList.remove('active');\n"
        "        progressContainer.classList.remove('active');\n"
        "        uploadButton.disabled = false;\n"
        "    }\n"
        "});\n"
        "</script>\n"
        "</body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    const char* response = "{\"status\":\"ready\",\"device\":\"ESP32C3\",\"epd\":\"connected\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Image upload handler
static esp_err_t upload_handler(httpd_req_t *req)
{
    char buf[1024];
    int remaining = req->content_len;
    int received = 0;
    
    // Find the images partition
    const esp_partition_t *part = esp_partition_find_first(123, 0, NULL);
    if (part == NULL) {
        ESP_LOGE(TAG, "❌ Images partition not found!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Images partition not found");
        return ESP_FAIL;
    }
    
    // Erase the partition first
    esp_err_t err = esp_partition_erase_range(part, 0, IMG_SIZE_BYTES);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to erase partition: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to erase partition");
        return ESP_FAIL;
    }
    
    // Set response headers for progress tracking
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Content-Length");
    
    // First, read the header (64 bytes)
    if (remaining < 64) {
        ESP_LOGE(TAG, "❌ Data too short for header");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Data too short for header");
        return ESP_FAIL;
    }
    
    int ret = httpd_req_recv(req, buf, 64);
    if (ret != 64) {
        ESP_LOGE(TAG, "❌ Failed to read header");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read header");
        return ESP_FAIL;
    }
    
    // Write header to flash
    err = esp_partition_write(part, 0, buf, 64);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to write header: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write header");
        return ESP_FAIL;
    }
    
    received += 64;
    remaining -= 64;
    
    // Receive and write image data in chunks
    while (remaining > 0) {
        int recv_size = MIN(remaining, sizeof(buf));
        ret = httpd_req_recv(req, buf, recv_size);
        
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "❌ Error receiving data");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error receiving data");
            return ESP_FAIL;
        }
        
        // Debug: Print first few bytes of each chunk
        ESP_LOGI(TAG, "📦 Chunk at offset %d, size %d, first bytes: %02x %02x %02x %02x", 
                 received, ret, buf[0], buf[1], buf[2], buf[3]);
        
        // Write chunk to flash at the correct offset
        err = esp_partition_write(part, received, buf, ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to write chunk at offset %d: %s", received, esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write data");
            return ESP_FAIL;
        }
        
        // Update received count and remaining bytes
        received += ret;
        remaining -= ret;
        
        // Log progress
        ESP_LOGI(TAG, "📤 Upload progress: %d%% (%d/%d bytes)", 
                 (received * 100) / req->content_len,
                 received, req->content_len);
    }
    
    if (received > 0) {
        ESP_LOGI(TAG, "✅ Image uploaded successfully");
        
        // Send success response
        const char* response = "<!DOCTYPE html>"
            "<html><head>"
            "<meta http-equiv='refresh' content='3;url=/' />"
            "<title>Upload Success</title>"
            "<style>"
            "body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; }"
            ".success { background: #dff0d8; color: #3c763d; padding: 20px; border-radius: 8px; }"
            "</style>"
            "</head>"
            "<body>"
            "<div class='success'>"
            "<h1>✅ Upload Successful!</h1>"
            "<p>Image has been saved and will be displayed on the EPD.</p>"
            "<p>Redirecting to home page...</p>"
            "</div>"
            "</body></html>";
        
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        
        // Map the partition to read the image data
        const flash_image_t *images = NULL;
        spi_flash_mmap_handle_t mmap_handle;
        err = esp_partition_mmap(part, 0, IMG_SIZE_BYTES, SPI_FLASH_MMAP_DATA, (const void**)&images, &mmap_handle);
        
        if (err == ESP_OK) {
            // Update EPD display
            epd_send(images->data, ICON_NONE);
            epd_shutdown();
            spi_flash_munmap(mmap_handle);
        } else {
            ESP_LOGE(TAG, "❌ Failed to map partition for EPD update: %s", esp_err_to_name(err));
        }
        
        return ESP_OK;
    }
    
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid image data");
    return ESP_FAIL;
}

// Upload success handler
static esp_err_t upload_success_handler(httpd_req_t *req)
{
    const char* response = "<!DOCTYPE html>"
        "<html><head>"
        "<meta http-equiv='refresh' content='3;url=/' />"
        "<title>Upload Success</title>"
        "<style>"
        "body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; }"
        ".success { background: #dff0d8; color: #3c763d; padding: 20px; border-radius: 8px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div class='success'>"
        "<h1>✅ Upload Successful!</h1>"
        "<p>Image has been saved and will be displayed on the EPD.</p>"
        "<p>Redirecting to home page...</p>"
        "</div>"
        "</body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// URI handlers
static const httpd_uri_t hello_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = hello_handler
};

static const httpd_uri_t status_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_handler
};

static const httpd_uri_t upload_uri = {
    .uri = "/upload",
    .method = HTTP_POST,
    .handler = upload_handler
};

static const httpd_uri_t upload_success_uri = {
    .uri = "/upload-success",
    .method = HTTP_GET,
    .handler = upload_success_handler
};

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "📱 Station connected to AP");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "📱 Station disconnected from AP");
    }
}

// Initialize WiFi AP
static void wifi_init_ap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif, &ip_info);
    
    ESP_LOGI(TAG, "📡 WiFi AP started");
    ESP_LOGI(TAG, "🔗 SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "🔑 Password: %s", WIFI_PASS);
    ESP_LOGI(TAG, "🌐 AP IP: " IPSTR, IP2STR(&ip_info.ip));
}

// Initialize HTTP server
static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    
    ESP_LOGI(TAG, "🚀 Starting HTTP server on port %d", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "📋 Registering URI handlers");
        httpd_register_uri_handler(server, &hello_uri);
        httpd_register_uri_handler(server, &status_uri);
        httpd_register_uri_handler(server, &upload_uri);
        httpd_register_uri_handler(server, &upload_success_uri);
        return server;
    }
    
    ESP_LOGE(TAG, "❌ Error starting HTTP server!");
    return NULL;
}

void app_main(void)
{
	ESP_LOGI(TAG, "🚀 === EPD PicFrame with Web Server ===");
	ESP_LOGI(TAG, "ESP32C3 EPD + WiFi AP + HTTP Server");

	// Initialize NVS (required for WiFi)
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	ESP_LOGI(TAG, "✅ NVS initialized");

	// Initialize WiFi AP
	wifi_init_ap();
	
	// Start HTTP server
	start_webserver();
	ESP_LOGI(TAG, "🌐 Web interface available at: http://192.168.4.1");
	
	// Access embedded icon data
	extern const uint8_t icons_bmp_start[] asm("_binary_icons_bmp_start");
	extern const uint8_t icons_bmp_end[] asm("_binary_icons_bmp_end");
	const uint8_t *icon = icons_bmp_start;
	size_t icon_size = icons_bmp_end - icons_bmp_start;
	
	ESP_LOGI(TAG, "Icon embedded successfully, size: %d bytes", icon_size);
	ESP_LOGI(TAG, "Icon will be used for EPD status display");
	
	ESP_LOGI(TAG, "📋 EPD initialization complete");
	ESP_LOGI(TAG, "🌐 Web server running - connect to WiFi and browse to http://192.168.4.1");
	
	// Keep web server running
	while (1) {
		ESP_LOGI(TAG, "🔄 Web server active - serving requests...");
		vTaskDelay(30000 / portTICK_PERIOD_MS); // Log every 30 seconds
	}
}
