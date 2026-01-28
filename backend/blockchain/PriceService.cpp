#include "PriceService.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <chrono>

using json = nlohmann::json;

namespace PriceService {

PriceFetcher::PriceFetcher(int timeout)
    : base_url("https://api.coingecko.com/api/v3"),
      timeout_seconds(timeout),
      last_status_code(0) {
}

std::string PriceFetcher::MakeRequest(const std::string& endpoint) {
    try {
        std::string url = base_url + endpoint;
        std::cout << "[PriceService] Making request to: " << url << std::endl;

        cpr::Response response = cpr::Get(
            cpr::Url{url},
            cpr::Header{{"User-Agent", "CriptoGualet/1.0"}, {"Accept", "application/json"}},
            cpr::Timeout{timeout_seconds * 1000}
        );

        last_status_code = response.status_code;
        std::cout << "[PriceService] Response status: " << response.status_code << std::endl;

        if (response.status_code == 200) {
            std::cout << "[PriceService] Success! Response length: " << response.text.length() << " bytes" << std::endl;
            return response.text;
        }

        std::cout << "[PriceService] Request failed with status: " << response.status_code;
        if (!response.text.empty()) {
            std::cout << " | Body: " << response.text.substr(0, 200);
        }
        std::cout << std::endl;
        return "";

    } catch (const std::exception& e) {
        last_status_code = 0;
        std::cout << "[PriceService] Exception: " << e.what() << std::endl;
        return "";
    }
}

std::optional<double> PriceFetcher::GetBTCPrice() {
    auto priceData = GetCryptoPrice("bitcoin");
    if (priceData) {
        return priceData->usd_price;
    }
    return std::nullopt;
}

std::optional<CryptoPriceData> PriceFetcher::GetCryptoPrice(const std::string& symbol) {
    // CoinGecko endpoint: /simple/price?ids=bitcoin&vs_currencies=usd&include_24hr_change=true
    std::string endpoint = "/simple/price?ids=" + symbol +
                          "&vs_currencies=usd&include_24hr_change=true&include_last_updated_at=true&include_market_cap=true";

    std::string response = MakeRequest(endpoint);
    if (response.empty()) {
        return std::nullopt;
    }

    return ParsePriceResponse(response);
}

std::optional<CryptoPriceData> PriceFetcher::ParsePriceResponse(const std::string& json_response) {
    try {
        json parsed = json::parse(json_response);

        // Check if bitcoin data exists
        if (!parsed.contains("bitcoin")) {
            return std::nullopt;
        }

        auto btc_data = parsed["bitcoin"];

        CryptoPriceData data;
        data.symbol = "BTC";
        data.name = "Bitcoin";
        data.usd_price = btc_data.value("usd", 0.0);
        data.price_change_24h = btc_data.value("usd_24h_change", 0.0);
        data.market_cap = btc_data.value("usd_market_cap", 0.0);

        // Convert timestamp to string if available
        if (btc_data.contains("last_updated_at")) {
            uint64_t timestamp = btc_data["last_updated_at"];
            data.last_updated = std::to_string(timestamp);
        } else {
            data.last_updated = "";
        }

        return data;
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

std::vector<CryptoPriceData> PriceFetcher::GetTopCryptosByMarketCap(int count) {
    // CoinGecko endpoint for market data with pagination
    std::string endpoint = "/coins/markets?vs_currency=usd&order=market_cap_desc&per_page=" +
                          std::to_string(count) +
                          "&page=1&sparkline=false&price_change_percentage=24h";

    const int max_attempts = 3;
    const int retry_delay_seconds = 3;

    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        std::string response = MakeRequest(endpoint);
        if (!response.empty()) {
            auto results = ParseTopCryptosResponse(response, count);
            if (!results.empty() || attempt == max_attempts) {
                return results;
            }
        }

        bool retryable = last_status_code == 0 || last_status_code == 403 ||
                         last_status_code == 429 ||
                         (last_status_code >= 500 && last_status_code <= 599);

        if (!retryable || attempt == max_attempts) {
            break;
        }

        std::cout << "[PriceService] Retry " << (attempt + 1) << "/" << max_attempts
                  << " after " << retry_delay_seconds << "s" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(retry_delay_seconds));
    }

    return {};
}

std::vector<CryptoPriceData> PriceFetcher::ParseTopCryptosResponse(const std::string& json_response, int count) {
    std::vector<CryptoPriceData> results;

    try {
        json parsed = json::parse(json_response);

        std::cout << "[PriceService] Parsed JSON type: " << (parsed.is_array() ? "array" : "not array") << std::endl;

        if (!parsed.is_array()) {
            std::cout << "[PriceService] Error: Response is not an array" << std::endl;
            return results;
        }

        std::cout << "[PriceService] Array size: " << parsed.size() << std::endl;

        for (const auto& coin : parsed) {
            CryptoPriceData data;
            data.symbol = coin.value("symbol", "");
            data.name = coin.value("name", "");
            data.usd_price = coin.value("current_price", 0.0);
            data.price_change_24h = coin.value("price_change_percentage_24h", 0.0);
            data.market_cap = coin.value("market_cap", 0.0);
            data.last_updated = coin.value("last_updated", "");
            data.image_url = coin.value("image", "");

            // Convert symbol to uppercase
            for (char& c : data.symbol) {
                c = std::toupper(c);
            }

            std::cout << "[PriceService] Added: " << data.name << " (" << data.symbol << ") - $" << data.usd_price << std::endl;

            results.push_back(data);

            // Stop when we have enough results
            if (static_cast<int>(results.size()) >= count) {
                break;
            }
        }

        std::cout << "[PriceService] Total results: " << results.size() << std::endl;
        return results;
    } catch (const std::exception& e) {
        std::cout << "[PriceService] Parse exception: " << e.what() << std::endl;
        return results;
    }
}

double PriceFetcher::ConvertBTCToUSD(double btc_amount, double usd_price) {
    return btc_amount * usd_price;
}

void PriceFetcher::SetTimeout(int seconds) {
    timeout_seconds = seconds;
}

} // namespace PriceService
