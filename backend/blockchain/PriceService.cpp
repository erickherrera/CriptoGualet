#include "include/PriceService.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace PriceService {

PriceFetcher::PriceFetcher(int timeout)
    : base_url("https://api.coingecko.com/api/v3"),
      timeout_seconds(timeout) {
}

std::string PriceFetcher::MakeRequest(const std::string& endpoint) {
    try {
        std::string url = base_url + endpoint;

        cpr::Response response = cpr::Get(
            cpr::Url{url},
            cpr::Timeout{timeout_seconds * 1000}
        );

        if (response.status_code == 200) {
            return response.text;
        }

        return "";
    } catch (const std::exception& e) {
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
                          "&vs_currencies=usd&include_24hr_change=true&include_last_updated_at=true";

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
        data.usd_price = btc_data.value("usd", 0.0);
        data.price_change_24h = btc_data.value("usd_24h_change", 0.0);

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

double PriceFetcher::ConvertBTCToUSD(double btc_amount, double usd_price) {
    return btc_amount * usd_price;
}

void PriceFetcher::SetTimeout(int seconds) {
    timeout_seconds = seconds;
}

} // namespace PriceService
