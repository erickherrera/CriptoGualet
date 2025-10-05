#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace QR {

struct QRData {
    int width;
    int height;
    std::vector<uint8_t> data;

    QRData() : width(0), height(0) {}
    QRData(int w, int h, std::vector<uint8_t> d) : width(w), height(h), data(std::move(d)) {}
};

bool GenerateQRCode(const std::string& text, QRData& qrData);

bool SaveQRCodeAsPNG(const QRData& qrData, const std::string& filename, int scale = 8);

} // namespace QR