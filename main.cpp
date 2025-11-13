#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <curl/curl.h>
#include <cmath>
#include <iomanip>
#include <sstream>

// Simple HFT Market Maker for US Stocks (Alpha Vantage)
// Strategy: Quote bid/ask around midpoint, capture spread

class Portfolio {
    public:
        std::string name = "Iman";
        std::atomic<double> cash{1'000'000};
        std::atomic<int> shares{0};
};

class MarketMaker {
private:
    std::string symbol;  // Apple stock
    std::string api_key; // Use "demo" for testing, get free key from alphavantage.co
    std::atomic<double> last_price{0.0};
    std::atomic<double> bid_price{0.0};
    std::atomic<double> ask_price{0.0};
    std::atomic<bool> running{true};
    
    // Strategy parameters
    double spread_bps = 5.0;   // 5 basis points spread (0.05%)
    int share_size = 100;      // Number of shares per order
    
    // Alpha Vantage API
    const std::string BASE_URL = "https://www.alphavantage.co/query";
    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }
    
    std::string httpGet(const std::string& url) {
        CURL* curl = curl_easy_init();
        std::string response;
        
        if(curl) {
            std::cout << "Is curl here?" << std::endl;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            
            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            
            if(res != CURLE_OK) {
                std::cerr << "HTTP Error: " << curl_easy_strerror(res) << std::endl;
                return "";
            }
        }
        return response;
    }
    
    bool updateMarketPrice() {
        // Get real-time quote
        std::string url = BASE_URL + "?function=GLOBAL_QUOTE&symbol=" + symbol + "&apikey=" + api_key;
        std::string response = httpGet(url);
        
        if(response.empty()) {
            return false;
        }
        
        // Quick, dependency-free extraction of quoted JSON values:
        auto extractQuotedValue = [&](const std::string& key) -> std::string {
            std::string quotedKey = "\"" + key + "\"";
            size_t keyPos = response.find(quotedKey);
            if(keyPos == std::string::npos) return "";
            size_t colonPos = response.find(':', keyPos + quotedKey.size());
            if(colonPos == std::string::npos) return "";
            // find first quote after colon
            size_t firstQuote = response.find('"', colonPos);
            if(firstQuote == std::string::npos) return "";
            size_t secondQuote = response.find('"', firstQuote + 1);
            if(secondQuote == std::string::npos) return "";
            return response.substr(firstQuote + 1, secondQuote - firstQuote - 1);
        };
        
        // Alpha Vantage returns "Global Quote": { "05. price": "123.45", ... }
        std::string priceStr = extractQuotedValue("05. price");
        if(priceStr.empty()) {
            // response may contain an error or different format
            // attempt to detect API-level errors to log
            if(response.find("Error Message") != std::string::npos || response.find("Note") != std::string::npos) {
                std::cerr << "API Error/Note: " << response << std::endl;
            } else {
                std::cerr << "Unexpected response (missing 05. price): " << response << std::endl;
            }
            return false;
        }
        
        try {
            double price = std::stod(priceStr);
            last_price = price;
        } catch(...) {
            std::cerr << "Failed to parse price: " << priceStr << std::endl;
            return false;
        }
        
        std::string lowStr = extractQuotedValue("04. low");
        if(!lowStr.empty()) {
            try { bid_price = std::stod(lowStr); } catch(...) {}
        }
        std::string highStr = extractQuotedValue("03. high");
        if(!highStr.empty()) {
            try { ask_price = std::stod(highStr); } catch(...) {}
        }
        
        return true;
    }
    
    void displayOrderBook() {
        double mid = last_price.load();
        if(mid <= 0) return;
        
        double spread_factor = spread_bps / 10000.0;
        double our_bid = mid * (1.0 - spread_factor);
        double our_ask = mid * (1.0 + spread_factor);
        
        std::cout << "\n=== SIMULATED ORDER BOOK ===" << std::endl;
        std::cout << "Market ASK:  $" << std::fixed << std::setprecision(2) << ask_price.load() << std::endl;
        std::cout << "Our ASK:     $" << our_ask << " [" << share_size << " shares]  <-- SELL" << std::endl;
        std::cout << "------------ MID: $" << mid << " ------------" << std::endl;
        std::cout << "Our BID:     $" << our_bid << " [" << share_size << " shares]  <-- BUY" << std::endl;
        std::cout << "Market BID:  $" << std::fixed << std::setprecision(2) << bid_price.load() << std::endl;
    }
    
    void displayStats(int cycle, long latency_us) {
        double mid = last_price.load();
        if(mid <= 0) return;
        
        double spread_factor = spread_bps / 10000.0;
        double our_bid = mid * (1.0 - spread_factor);
        double our_ask = mid * (1.0 + spread_factor);
        double spread_dollars = our_ask - our_bid;
        double profit_per_rt = spread_dollars * share_size;
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "[Cycle #" << cycle << " @ " 
                  << std::put_time(std::localtime(&time), "%H:%M:%S") << "]" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Symbol:      " << symbol << std::endl;
        std::cout << "Mid Price:   $" << std::fixed << std::setprecision(2) << mid << std::endl;
        std::cout << "Our Bid:     $" << our_bid << " (" << share_size << " shares)" << std::endl;
        std::cout << "Our Ask:     $" << our_ask << " (" << share_size << " shares)" << std::endl;
        std::cout << "Spread:      $" << std::setprecision(4) << spread_dollars 
                  << " (" << spread_bps << " bps)" << std::endl;
        std::cout << "Profit/RT:   $" << std::setprecision(2) << profit_per_rt 
                  << " per round trip" << std::endl;
        std::cout << "Latency:     " << latency_us << " μs" << std::endl;
        std::cout << "========================================" << std::endl;
    }
    
public:
    std::string getSymbol() { return symbol; }
    void setSymbol(const std::string& sym) { symbol = sym; }
    void setApiKey(const std::string& key) { api_key = key; }
    void setSpread(double bps) { spread_bps = bps; }
    void setShareSize(int size) { share_size = size; }
    
    void run(Portfolio *portfolio) {
        std::cout << "Portfolio: " << portfolio->name << std::endl;
        std::cout << "\n╔════════════════════════════════════════╗" << std::endl;
        std::cout << "║   HFT MARKET MAKER - US STOCKS       ║" << std::endl;
        std::cout << "╚════════════════════════════════════════╝" << std::endl;
        std::cout << "\nConfiguration:" << std::endl;
        std::cout << "  Symbol:     " << symbol << std::endl;
        std::cout << "  Spread:     " << spread_bps << " bps" << std::endl;
        std::cout << "  Order Size: " << share_size << " shares" << std::endl;
        std::cout << "  API Key:    " << (api_key == "demo" ? "DEMO (limited)" : "Custom") << std::endl;
        
        if(api_key == "demo") {
            std::cout << "\n⚠️  Using DEMO key (limited to 25 requests/day)" << std::endl;
            std::cout << "   Get FREE key at: https://www.alphavantage.co/support/#api-key" << std::endl;
        }
        
        std::cout << "\nPress Ctrl+C to stop\n" << std::endl;
        
        int cycle = 0;
        while(running) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // 1. Update market price
            bool success = updateMarketPrice();
            
            if(!success) {
                std::cout << "\n⏳ Waiting for market data..." << std::endl;
                if(api_key == "demo" && cycle > 5) {
                    std::cout << "⚠️  DEMO key limit may be reached. Get free key at alphavantage.co" << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            
            cycle++;
            
            // 2. Calculate latency
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            // 3. Display stats
            displayStats(cycle, duration.count());
            
            // 4. Show order book every cycle
            displayOrderBook();
            
            // 5. Show performance metrics
            std::cout << "\n📊 Performance:" << std::endl;
            std::cout << "   Cycle time:  " << duration.count() / 1000.0 << " ms" << std::endl;
            
            if(duration.count() < 100000) {  // < 100ms
                std::cout << "   Status:      ✅ FAST" << std::endl;
            } else if(duration.count() < 500000) {  // < 500ms
                std::cout << "   Status:      ⚠️  MODERATE" << std::endl;
            } else {
                std::cout << "   Status:      ❌ SLOW (optimize needed)" << std::endl;
            }
            
            // In production: place/cancel orders here
            std::cout << "\n💡 Next: Implement order placement with broker API" << std::endl;
            
            // Alpha Vantage free tier: 5 calls/minute, wait 12+ seconds
            int wait_time = (api_key == "demo") ? 15 : 12;
            std::cout << "\nWaiting " << wait_time << " seconds (API rate limit)..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(wait_time));
        }
    }
    
    void stop() {
        running = false;
    }
};

int main(int argc, char* argv[]) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    MarketMaker mm;

    // // Parse command line arguments
    if(argc > 1) mm.setSymbol(argv[1]);
    if(argc > 2) mm.setApiKey(argv[2]);
    
    std::cout << "\n📈 HFT Market Maker starting..." << std::endl;

    Portfolio portfolio;
    std::cout << "Beginning balance: $"
                << std::fixed
                << std::setprecision(2)
                << portfolio.cash.load()
                << std::endl;

    // Run the market maker in a separate thread
    std::thread runner([&mm, &portfolio]() { mm.run(&portfolio); });

    
    // // Wait for Enter key to stop
    std::cout << "\nPress Enter to stop...\n" << std::endl;
    std::cin.get();
    
    mm.stop();
    runner.join();
    
    curl_global_cleanup();
    
    // std::cout << "\n✅ Market maker stopped." << std::endl;
    std::cout << "Ending balance: $" << portfolio.cash << std::endl << std::endl;
    return 0;
}