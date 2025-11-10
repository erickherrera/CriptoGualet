#include "include/PriceService.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <iostream>

using json = nlohmann::json;

namespace PriceService {

PriceFetcher::PriceFetcher(int timeout)
    : base_url("https://api.coingecko.com/api/v3"),
      timeout_seconds(timeout) {
}

std::string PriceFetcher::MakeRequest(const std::string& endpoint) {
    try {
        std::string url = base_url + endpoint;
        std::cout << "[PriceService] Making request to: " << url << std::endl;

        cpr::Response response = cpr::Get(
            cpr::Url{url},
            cpr::Timeout{timeout_seconds * 1000}
        );

        std::cout << "[PriceService] Response status: " << response.status_code << std::endl;

        if (response.status_code == 200) {
            std::cout << "[PriceService] Success! Response length: " << response.text.length() << " bytes" << std::endl;
            return response.text;
        }

        std::cout << "[PriceService] Request failed with status: " << response.status_code << std::endl;
        return "";
    } catch (const std::exception& e) {
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
    // Fetch extra to account for stablecoins we'll filter out
    std::string endpoint = "/coins/markets?vs_currency=usd&order=market_cap_desc&per_page=" +
                          std::to_string(count * 2) + // Fetch extra to account for stablecoins
                          "&page=1&sparkline=false&price_change_percentage=24h";

    std::string response = MakeRequest(endpoint);
    if (response.empty()) {
        return {};
    }

    return ParseTopCryptosResponse(response, count);
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

        // List of common stablecoins to exclude
        std::vector<std::string> stablecoins = {
            "tether", "usd-coin", "binance-usd", "dai", "true-usd",
            "paxos-standard", "usdd", "frax", "tusd", "gemini-dollar"
        };

        for (const auto& coin : parsed) {
            // Skip stablecoins
            std::string coin_id = coin.value("id", "");
            bool is_stablecoin = false;
            for (const auto& stable : stablecoins) {
                if (coin_id == stable) {
                    is_stablecoin = true;
                    break;
                }
            }
            if (is_stablecoin) {
                std::cout << "[PriceService] Skipping stablecoin: " << coin_id << std::endl;
                continue;
            }

            CryptoPriceData data;
            data.symbol = coin.value("symbol", "");
            data.name = coin.value("name", "");
            data.usd_price = coin.value("current_price", 0.0);
            data.price_change_24h = coin.value("price_change_percentage_24h", 0.0);
            data.market_cap = coin.value("market_cap", 0.0);
            data.last_updated = coin.value("last_updated", "");

            // Convert symbol to uppercase
            for (char& c : data.symbol) {
                c = std::toupper(c);
            }

            std::cout << "[PriceService] Added: " << data.name << " (" << data.symbol << ") - $" << data.usd_price << std::endl;

            results.push_back(data);

            // Stop when we have enough non-stablecoin results
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
