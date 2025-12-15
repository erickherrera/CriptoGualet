#include "include/QRGenerator.h"
#include <stdexcept>
#include <iostream>

#ifdef QRENCODE_AVAILABLE
#include <qrencode.h>
#endif

namespace QR {

bool GenerateQRCode(const std::string& text, QRData& qrData) {
#ifdef QRENCODE_AVAILABLE
    QRcode* qrcode = QRcode_encodeString(text.c_str(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);

    if (!qrcode) {
        return false;
    }

    qrData.width = qrcode->width;
    qrData.height = qrcode->width;
    qrData.data.clear();
    qrData.data.reserve(qrcode->width * qrcode->width);

    // Convert QR code data to our format
    // libqrencode uses bits 0 = white, 1 = black
    // We'll use 0 = black, 255 = white for easier Qt integration
    for (int i = 0; i < qrcode->width * qrcode->width; ++i) {
        qrData.data.push_back((qrcode->data[i] & 1) ? 0 : 255);
    }

    QRcode_free(qrcode);
    return true;
#else
    // Fallback: Create a simple "QR code not available" pattern
    qrData.width = 25;
    qrData.height = 25;
    qrData.data.clear();
    qrData.data.resize(25 * 25, 255); // All white

    // Draw a simple border and "X" pattern to indicate QR not available
    for (int i = 0; i < 25; ++i) {
        qrData.data[i] = 0; // Top border
        qrData.data[24 * 25 + i] = 0; // Bottom border
        qrData.data[i * 25] = 0; // Left border
        qrData.data[i * 25 + 24] = 0; // Right border
        qrData.data[i * 25 + i] = 0; // Diagonal 1
        qrData.data[i * 25 + (24 - i)] = 0; // Diagonal 2
    }

    return false; // Return false to indicate QR generation not available
#endif
}

bool SaveQRCodeAsPNG(const QRData& qrData, const std::string& filename, int scale) {
    // This function is not implemented as we don't have libpng
    // Qt will handle image display directly through QImage
    return false;
}

} // namespace QR