#pragma once

#include <string>
#include <optional>
#include <memory>
#include <functional>
#include <vector>

namespace PriceService {

struct CryptoPriceData {
    std::string symbol;        // e.g., "BTC"
    std::string name;          // Full name e.g., "Bitcoin"
    double usd_price;          // Current price in USD
    double price_change_24h;   // 24h price change percentage
    double market_cap;         // Market capitalization in USD
    std::string last_updated;  // Last update timestamp
    std::string image_url;     // Icon/logo image URL from CoinGecko
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

    // Get top N cryptocurrencies by market cap (excluding stablecoins)
    std::vector<CryptoPriceData> GetTopCryptosByMarketCap(int count = 5);

    // Convert BTC to USD
    double ConvertBTCToUSD(double btc_amount, double usd_price);

    // Set timeout for HTTP requests
    void SetTimeout(int seconds);

private:
    std::string MakeRequest(const std::string& endpoint);
    std::optional<CryptoPriceData> ParsePriceResponse(const std::string& json_response);
    std::vector<CryptoPriceData> ParseTopCryptosResponse(const std::string& json_response, int count);
};

} // namespace PriceService
