#pragma once

#include <string>
#include <optional>
#include <memory>
#include <functional>
#include <vector>
#include <chrono>
#include <mutex>

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

struct CachedPriceData {
    CryptoPriceData data;
    std::chrono::steady_clock::time_point timestamp;
};

class PriceFetcher {
private:
    std::string base_url;
    int timeout_seconds;
    long last_status_code;
    int cache_ttl_seconds;
    CachedPriceData cached_price;
    std::vector<CryptoPriceData> cached_top_cryptos;
    std::chrono::steady_clock::time_point last_top_cryptos_fetch;
    mutable std::mutex cache_mutex;

    bool isCacheValid(const std::chrono::steady_clock::time_point& timestamp) const;
    void updatePriceCache(const CryptoPriceData& data);
    void updateTopCryptosCache(const std::vector<CryptoPriceData>& data);
    std::optional<CryptoPriceData> getCachedPrice() const;
    std::optional<std::vector<CryptoPriceData>> getCachedTopCryptos() const;

public:
    explicit PriceFetcher(int timeout = 10);

    // Get current BTC price in USD
    std::optional<double> GetBTCPrice();

    // Get detailed price data
    std::optional<CryptoPriceData> GetCryptoPrice(const std::string& symbol = "bitcoin");

    // Get top N cryptocurrencies by market cap (excluding stablecoins)
    std::vector<CryptoPriceData> GetTopCryptosByMarketCap(int count = 5);

    // Force refresh cache
    void ClearCache();

    // Convert BTC to USD
    double ConvertBTCToUSD(double btc_amount, double usd_price);

    // Set timeout for HTTP requests
    void SetTimeout(int seconds);

    // Set cache TTL in seconds
    void SetCacheTTL(int seconds);

private:
    std::string MakeRequest(const std::string& endpoint);
    std::optional<CryptoPriceData> ParsePriceResponse(const std::string& json_response, const std::string& coin_id);
    std::vector<CryptoPriceData> ParseTopCryptosResponse(const std::string& json_response, int count);
};

} // namespace PriceService
