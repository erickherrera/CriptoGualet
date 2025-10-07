#pragma once

#include <string>
#include <optional>
#include <memory>
#include <functional>

namespace PriceService {

struct CryptoPriceData {
    std::string symbol;        // e.g., "BTC"
    double usd_price;          // Current price in USD
    double price_change_24h;   // 24h price change percentage
    std::string last_updated;  // Last update timestamp
};

class PriceFetcher {
private:
    std::string base_url;
    int timeout_seconds;

public:
    explicit PriceFetcher(int timeout = 10);

    // Get current BTC price in USD
    std::optional<double> GetBTCPrice();

    // Get detailed price data
    std::optional<CryptoPriceData> GetCryptoPrice(const std::string& symbol = "bitcoin");

    // Convert BTC to USD
    double ConvertBTCToUSD(double btc_amount, double usd_price);

    // Set timeout for HTTP requests
    void SetTimeout(int seconds);

private:
    std::string MakeRequest(const std::string& endpoint);
    std::optional<CryptoPriceData> ParsePriceResponse(const std::string& json_response);
};

} // namespace PriceService
