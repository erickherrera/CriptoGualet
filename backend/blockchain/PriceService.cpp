#include "PriceService.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <chrono>
#include <map>

using json = nlohmann::json;

namespace PriceService {

static const std::map<std::string, std::string> symbolToCoinId = {
    {"btc", "bitcoin"},
    {"eth", "ethereum"},
    {"usdt", "tether"},
    {"bnb", "binancecoin"},
    {"sol", "solana"},
    {"usdc", "usd-coin"},
    {"xrp", "ripple"},
    {"steth", "staked-ether"},
    {"doge", "dogecoin"},
    {"ada", "cardano"},
    {"trx", "tron"},
    {"avax", "avalanche-2"},
    {"ton", "the-open-network"},
    {"wbtc", "wrapped-bitcoin"},
    {"link", "chainlink"},
    {"shib", "shiba-inu"},
    {"dot", "polkadot"},
    {"matic", "matic-network"},
    {"bch", "bitcoin-cash"},
    {"dai", "dai"},
    {"ltc", "litecoin"},
    {"uni", "uniswap"},
    {"atom", "cosmos"},
    {"icp", "internet-computer"},
    {"leo", "unus-sed-leo"},
    {"etc", "ethereum-classic"},
    {"xlm", "stellar"},
    {"fil", "filecoin"},
    {"xmr", "monero"},
    {"apt", "aptos"},
    {"okb", "okb"},
    {"hbar", "hedera-hashgraph"},
    {"mnt", "mantle"},
    {"near", "near"},
    {"cro", "crypto-com-chain"},
    {"rndr", "render-token"},
    {"kas", "kaspa"},
    {"imx", "immutable-x"},
    {"arb", "arbitrum"},
    {"op", "optimism"},
    {"vet", "vechain"},
    {"stx", "stacks"},
    {"grt", "the-graph"},
    {"mkr", "maker"},
    {"inj", "injective-protocol"},
    {"algo", "algorand"},
    {"rune", "thorchain"},
    {"qnt", "quant-network"},
    {"aave", "aave"},
    {"flr", "flare-network"},
    {"snx", "havven"},
    {"egld", "elrond-erd-2"},
    {"ftm", "fantom"},
    {"xtz", "tezos"},
    {"sand", "the-sandbox"},
    {"theta", "theta-token"},
    {"mana", "decentraland"},
    {"eos", "eos"},
    {"xdc", "xdce-crowd-sale"},
    {"axs", "axie-infinity"},
    {"flow", "flow"},
    {"neo", "neo"},
    {"klay", "klay-token"},
    {"chz", "chiliz"},
    {"usdd", "usdd"},
    {"tusd", "true-usd"},
    {"pepe", "pepe"},
    {"cfx", "conflux-token"},
    {"zec", "zcash"},
    {"miota", "iota"},
    {"ldo", "lido-dao"},
    {"bsv", "bitcoin-cash-sv"},
    {"kava", "kava"},
    {"dash", "dash"},
    {"ht", "huobi-token"},
    {"1inch", "1inch"},
    {"cake", "pancakeswap-token"},
    {"gmx", "gmx"},
    {"rpl", "rocket-pool"},
    {"zil", "zilliqa"},
    {"enj", "enjincoin"},
    {"bat", "basic-attention-token"},
    {"comp", "compound-governance-token"},
    {"yfi", "yearn-finance"},
    {"sui", "sui"},
    {"blur", "blur"},
    {"crv", "curve-dao-token"},
    {"gala", "gala"},
    {"chsb", "swissborg"},
    {"frax", "frax-share"},
    {"lrc", "loopring"},
    {"zrx", "0x"},
    {"sushi", "sushi"},
    {"one", "harmony"},
    {"waves", "waves"},
    {"celo", "celo"},
    {"icx", "icon"},
    {"woo", "wootrade"},
    {"qtum", "qtum"},
    {"ar", "arweave"}
};

static std::string toLower(const std::string& str) {
    std::string result = str;
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

static std::string getCoinId(const std::string& symbol) {
    std::string lowerSymbol = toLower(symbol);
    auto it = symbolToCoinId.find(lowerSymbol);
    if (it != symbolToCoinId.end()) {
        return it->second;
    }
    return lowerSymbol;
}

static std::string coinIdToSymbol(const std::string& coinId) {
    std::string lowerId = toLower(coinId);
    for (const auto& pair : symbolToCoinId) {
        if (pair.second == lowerId) {
            std::string sym = pair.first;
            for (char& c : sym) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return sym;
        }
    }
    std::string result = lowerId;
    for (char& c : result) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

PriceFetcher::PriceFetcher(int timeout)
    : base_url("https://api.coingecko.com/api/v3"),
      timeout_seconds(timeout),
      last_status_code(0),
      cache_ttl_seconds(30),
      cached_price(),
      cached_top_cryptos(),
      last_top_cryptos_fetch(),
      cache_mutex() {
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
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto cached = getCachedPrice();
        if (cached) {
            return cached;
        }
    }

    std::string coin_id = getCoinId(symbol);

    std::string endpoint = "/simple/price?ids=" + coin_id +
                          "&vs_currencies=usd&include_24hr_change=true&include_last_updated_at=true&include_market_cap=true";

    std::string response = MakeRequest(endpoint);
    if (response.empty()) {
        return std::nullopt;
    }

    auto result = ParsePriceResponse(response, coin_id);
    if (result) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        updatePriceCache(*result);
    }
    return result;
}

std::optional<CryptoPriceData> PriceFetcher::ParsePriceResponse(const std::string& json_response, const std::string& coin_id) {
    try {
        json parsed = json::parse(json_response);

        if (!parsed.contains(coin_id)) {
            std::cout << "[PriceService] Coin not found in response: " << coin_id << std::endl;
            return std::nullopt;
        }

        auto coin_data = parsed[coin_id];

        CryptoPriceData data;
        data.symbol = coinIdToSymbol(coin_id);
        data.name = "";
        data.usd_price = coin_data.value("usd", 0.0);
        data.price_change_24h = coin_data.value("usd_24h_change", 0.0);
        data.market_cap = coin_data.value("usd_market_cap", 0.0);
        data.image_url = "";

        if (coin_data.contains("last_updated_at")) {
            uint64_t timestamp = coin_data["last_updated_at"];
            data.last_updated = std::to_string(timestamp);
        } else {
            data.last_updated = "";
        }

        return data;
    } catch (const std::exception& e) {
        std::cout << "[PriceService] Parse exception: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<CryptoPriceData> PriceFetcher::GetTopCryptosByMarketCap(int count) {
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto cached = getCachedTopCryptos();
        if (cached) {
            return *cached;
        }
    }

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
                if (!results.empty()) {
                    updateTopCryptosCache(results);
                }
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

void PriceFetcher::SetCacheTTL(int seconds) {
    cache_ttl_seconds = seconds;
}

void PriceFetcher::ClearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex);
    cached_price = CachedPriceData();
    cached_top_cryptos.clear();
    last_top_cryptos_fetch = std::chrono::steady_clock::time_point();
}

bool PriceFetcher::isCacheValid(const std::chrono::steady_clock::time_point& timestamp) const {
    if (timestamp == std::chrono::steady_clock::time_point()) {
        return false;
    }
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp).count();
    return elapsed < cache_ttl_seconds;
}

void PriceFetcher::updatePriceCache(const CryptoPriceData& data) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    cached_price.data = data;
    cached_price.timestamp = std::chrono::steady_clock::now();
}

void PriceFetcher::updateTopCryptosCache(const std::vector<CryptoPriceData>& data) {
    std::lock_guard<std::mutex> lock(cache_mutex);
    cached_top_cryptos = data;
    last_top_cryptos_fetch = std::chrono::steady_clock::now();
}

std::optional<CryptoPriceData> PriceFetcher::getCachedPrice() const {
    std::lock_guard<std::mutex> lock(cache_mutex);
    if (isCacheValid(cached_price.timestamp)) {
        std::cout << "[PriceService] Returning cached price data" << std::endl;
        return cached_price.data;
    }
    return std::nullopt;
}

std::optional<std::vector<CryptoPriceData>> PriceFetcher::getCachedTopCryptos() const {
    std::lock_guard<std::mutex> lock(cache_mutex);
    if (isCacheValid(last_top_cryptos_fetch)) {
        std::cout << "[PriceService] Returning cached top cryptos data" << std::endl;
        return cached_top_cryptos;
    }
    return std::nullopt;
}

} // namespace PriceService
