/**
 * EPD Image Converter - JavaScript implementation
 * Converts images to e-paper display format with 7-color palette
 * Port of the original C implementation
 */

class EPDConverter {
    constructor() {
        this.EPD_W = 600;
        this.EPD_H = 448;
        this.EPD_UPSIDE_DOWN = true;
        
        // RGB colors as displayed on the EPD screen (linear RGB values)
        this.epdColors = [
            [0, 0, 0],           // Black
            [1, 1, 1],           // White  
            [0.059, 0.329, 0.119], // Green
            [0.061, 0.147, 0.336], // Blue
            [0.574, 0.066, 0.010], // Red
            [0.982, 0.756, 0.004], // Yellow
            [0.795, 0.255, 0.018], // Orange
        ];
        
        // Convert float RGB colors to integer colors for preview
        this.epdColorsInt = this.epdColors.map(color => 
            (Math.round(color[0] * 255) << 16) | 
            (Math.round(color[1] * 255) << 8) | 
            Math.round(color[2] * 255)
        );
    }
    
    /**
     * Convert sRGB to linear RGB
     * @param {number} srgb - sRGB value [0..1]
     * @returns {number} Linear RGB value [0..1]
     */
    gammaLinear(srgb) {
        return srgb > 0.04045 ? Math.pow((srgb + 0.055) / 1.055, 2.4) : srgb / 12.92;
    }
    
    /**
     * Convert linear RGB to CIE-LAB
     * @param {number[]} rgb - Linear RGB array [r, g, b] in range [0..1]
     * @returns {number[]} LAB array [L, a, b]
     */
    rgbToLab(rgb) {
        let [r, g, b] = rgb.map(c => Math.max(0, Math.min(100, c * 100)));
        
        // Observer = 2°, Illuminant = D65
        let x = r * 0.4124 + g * 0.3576 + b * 0.1805;
        let y = r * 0.2126 + g * 0.7152 + b * 0.0722;
        let z = r * 0.0193 + g * 0.1192 + b * 0.9505;
        
        x = x / 95.047;
        y = y / 100.0;
        z = z / 108.883;
        
        x = x > 0.008856 ? Math.pow(x, 1/3) : (7.787 * x) + (16/116);
        y = y > 0.008856 ? Math.pow(y, 1/3) : (7.787 * y) + (16/116);
        z = z > 0.008856 ? Math.pow(z, 1/3) : (7.787 * z) + (16/116);
        
        return [
            (116 * y) - 16,
            500 * (x - y),
            200 * (y - z)
        ];
    }
    
    /**
     * Calculate DeltaE 2000 color difference
     * @param {number[]} rgb1 - First RGB color [r, g, b]
     * @param {number[]} rgb2 - Second RGB color [r, g, b]  
     * @returns {number} DeltaE difference
     */
    colorDifference(rgb1, rgb2) {
        const lab1 = this.rgbToLab(rgb1);
        const lab2 = this.rgbToLab(rgb2);
        
        // DeltaE 2000 implementation
        const avgL = (lab1[0] + lab2[0]) / 2;
        const c1 = Math.sqrt(lab1[1] * lab1[1] + lab1[2] * lab1[2]);
        const c2 = Math.sqrt(lab2[1] * lab2[1] + lab2[2] * lab2[2]);
        const avgC = (c1 + c2) / 2;
        const g = (1 - Math.sqrt(Math.pow(avgC, 7) / (Math.pow(avgC, 7) + Math.pow(25, 7)))) / 2;
        
        const a1p = lab1[1] * (1 + g);
        const a2p = lab2[1] * (1 + g);
        const c1p = Math.sqrt(a1p * a1p + lab1[2] * lab1[2]);
        const c2p = Math.sqrt(a2p * a2p + lab2[2] * lab2[2]);
        const avgCp = (c1p + c2p) / 2;
        
        let h1p = Math.atan2(lab1[2], a1p) * 180 / Math.PI;
        if (h1p < 0) h1p += 360;
        let h2p = Math.atan2(lab2[2], a2p) * 180 / Math.PI;
        if (h2p < 0) h2p += 360;
        
        const avghp = Math.abs(h1p - h2p) > 180 ? (h1p + h2p + 360) / 2 : (h1p + h2p) / 2;
        const t = 1 - 0.17 * Math.cos((avghp - 30) * Math.PI / 180) + 
                     0.24 * Math.cos(2 * avghp * Math.PI / 180) + 
                     0.32 * Math.cos((3 * avghp + 6) * Math.PI / 180) - 
                     0.2 * Math.cos((4 * avghp - 63) * Math.PI / 180);
        
        let deltahp = h2p - h1p;
        if (Math.abs(deltahp) > 180) {
            if (h2p <= h1p) {
                deltahp += 360;
            } else {
                deltahp -= 360;
            }
        }
        
        const deltalp = lab2[0] - lab1[0];
        const deltacp = c2p - c1p;
        deltahp = 2 * Math.sqrt(c1p * c2p) * Math.sin(deltahp * Math.PI / 360);
        
        const sl = 1 + ((0.015 * Math.pow(avgL - 50, 2)) / Math.sqrt(20 + Math.pow(avgL - 50, 2)));
        const sc = 1 + 0.045 * avgCp;
        const sh = 1 + 0.015 * avgCp * t;
        
        const deltaro = 30 * Math.exp(-Math.pow((avghp - 275) / 25, 2));
        const rc = 2 * Math.sqrt(Math.pow(avgCp, 7) / (Math.pow(avgCp, 7) + Math.pow(25, 7)));
        const rt = -rc * Math.sin(2 * deltaro * Math.PI / 180);
        
        const kl = 1, kc = 1, kh = 1;
        
        const deltaE = Math.sqrt(
            Math.pow(deltalp / (kl * sl), 2) + 
            Math.pow(deltacp / (kc * sc), 2) + 
            Math.pow(deltahp / (kh * sh), 2) + 
            rt * (deltacp / (kc * sc)) * (deltahp / (kh * sh))
        );
        
        return deltaE;
    }
    
    /**
     * Clamp value between min and max
     */
    clamp(val, min, max) {
        return Math.max(min, Math.min(max, val));
    }
    
    /**
     * Distribute Floyd-Steinberg dithering error
     * @param {Float32Array} pixels - Pixel array [r,g,b,r,g,b,...]
     * @param {number} x - X coordinate
     * @param {number} y - Y coordinate  
     * @param {number} colorIndex - Color component (0=r, 1=g, 2=b)
     * @param {number} error - Error to distribute
     */
    distributeError(pixels, x, y, colorIndex, error) {
        if (x < 0 || x >= this.EPD_W || y < 0 || y >= this.EPD_H) return;
        
        const index = (x + y * this.EPD_W) * 3 + colorIndex;
        const newVal = this.clamp(pixels[index] + error, 0, 1);
        pixels[index] = newVal;
    }
    
    /**
     * Convert canvas to EPD format
     * @param {HTMLCanvasElement} canvas - Input canvas (600x448)
     * @returns {Object} Result containing binary data and preview canvas
     */
    convertImage(canvas) {
        if (canvas.width !== this.EPD_W || canvas.height !== this.EPD_H) {
            throw new Error(`Canvas must be ${this.EPD_W}x${this.EPD_H} pixels`);
        }
        
        const ctx = canvas.getContext('2d');
        const imageData = ctx.getImageData(0, 0, this.EPD_W, this.EPD_H);
        const data = imageData.data;
        
        // Convert to linear RGB float array
        const pixels = new Float32Array(this.EPD_W * this.EPD_H * 3);
        for (let i = 0; i < data.length; i += 4) {
            const pixelIndex = (i / 4) * 3;
            pixels[pixelIndex] = this.gammaLinear(data[i] / 255);     // R
            pixels[pixelIndex + 1] = this.gammaLinear(data[i + 1] / 255); // G
            pixels[pixelIndex + 2] = this.gammaLinear(data[i + 2] / 255); // B
        }
        
        // Create binary data structure
        const binarySize = 64 + (this.EPD_W * this.EPD_H / 2); // header + image data
        const binaryData = new Uint8Array(binarySize);
        const dataView = new DataView(binaryData.buffer);
        
        // Write header
        dataView.setUint32(0, 0xfafa1a1a, true); // Magic number (little endian)
        dataView.setBigUint64(4, BigInt(Math.floor(Date.now() / 1000)), true); // Timestamp
        
        // Create preview canvas
        const previewCanvas = document.createElement('canvas');
        previewCanvas.width = this.EPD_W;
        previewCanvas.height = this.EPD_H;
        const previewCtx = previewCanvas.getContext('2d');
        const previewImageData = previewCtx.createImageData(this.EPD_W, this.EPD_H);
        const previewData = previewImageData.data;
        
        // Process each pixel with Floyd-Steinberg dithering
        for (let y = 0; y < this.EPD_H; y++) {
            let oddByte = 0;
            
            for (let x = 0; x < this.EPD_W; x++) {
                const pixelIndex = (x + y * this.EPD_W) * 3;
                const rgb = [pixels[pixelIndex], pixels[pixelIndex + 1], pixels[pixelIndex + 2]];
                
                // Find closest color in EPD palette
                let bestColor = 0;
                let bestDifference = Infinity;
                
                for (let i = 0; i < this.epdColors.length; i++) {
                    const difference = this.colorDifference(rgb, this.epdColors[i]);
                    if (difference < bestDifference) {
                        bestDifference = difference;
                        bestColor = i;
                    }
                }
                
                // Distribute quantization error using Floyd-Steinberg
                for (let c = 0; c < 3; c++) {
                    const error = rgb[c] - this.epdColors[bestColor][c];
                    this.distributeError(pixels, x + 1, y, c, (error / 16) * 7);
                    this.distributeError(pixels, x - 1, y + 1, c, (error / 16) * 3);
                    this.distributeError(pixels, x, y + 1, c, (error / 16) * 5);
                    this.distributeError(pixels, x + 1, y + 1, c, (error / 16) * 1);
                }
                
                // Write to binary data (4-bit packed)
                const binaryIndex = 64 + Math.floor((y * this.EPD_W + x) / 2);
                if (this.EPD_UPSIDE_DOWN) {
                    const flippedX = this.EPD_W - 1 - x;
                    const flippedY = this.EPD_H - 1 - y;
                    const flippedBinaryIndex = 64 + Math.floor((flippedY * this.EPD_W + flippedX) / 2);
                    
                    if (x & 1) {
                        binaryData[flippedBinaryIndex] = oddByte | (bestColor << 4);
                    } else {
                        oddByte = bestColor;
                    }
                } else {
                    if (x & 1) {
                        binaryData[binaryIndex] = (oddByte << 4) | bestColor;
                    } else {
                        oddByte = bestColor;
                    }
                }
                
                // Set preview pixel
                const previewPixelIndex = (x + y * this.EPD_W) * 4;
                const color = this.epdColorsInt[bestColor];
                previewData[previewPixelIndex] = (color >> 16) & 0xFF;     // R
                previewData[previewPixelIndex + 1] = (color >> 8) & 0xFF;  // G
                previewData[previewPixelIndex + 2] = color & 0xFF;         // B
                previewData[previewPixelIndex + 3] = 255;                  // A
            }
        }
        
        previewCtx.putImageData(previewImageData, 0, 0);
        
        return {
            binaryData: binaryData,
            previewCanvas: previewCanvas,
            previewDataUrl: previewCanvas.toDataURL('image/png')
        };
    }
    
    /**
     * Convert result to base64 for compatibility with existing backend
     * @param {Uint8Array} binaryData - EPD binary data
     * @returns {string} Base64 encoded string
     */
    toBase64(binaryData) {
        let binary = '';
        for (let i = 0; i < binaryData.length; i++) {
            binary += String.fromCharCode(binaryData[i]);
        }
        return btoa(binary);
    }
}

// Export for use in other scripts
window.EPDConverter = EPDConverter; 