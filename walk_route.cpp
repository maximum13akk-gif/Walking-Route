// walk_route.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

const string DATA_FILE = "walk_route.json";
const double DEFAULT_SPEED = 5.0;
const double CALORIES_PER_KM = 55;

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string fetchUrl(const string& url) {
    CURL *curl = curl_easy_init();
    string response;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) return "";
    }
    return response;
}

struct Waypoint {
    string name;
    double lat;
    double lng;
};

struct RouteData {
    vector<Waypoint> waypoints;
    double speed = DEFAULT_SPEED;
    double weight = 70.0;
};

class RoutePlanner {
private:
    RouteData data;
    string dataFile = DATA_FILE;

    void load() {
        ifstream f(dataFile);
        if (!f.is_open()) return;
        json j;
        f >> j;
        if (j.contains("waypoints")) {
            for (auto& item : j["waypoints"]) {
                Waypoint w;
                w.name = item["name"];
                w.lat = item["lat"];
                w.lng = item["lng"];
                data.waypoints.push_back(w);
            }
        }
        if (j.contains("speed")) data.speed = j["speed"];
        if (j.contains("weight")) data.weight = j["weight"];
    }

    void save() {
        json j;
        j["waypoints"] = json::array();
        for (auto& w : data.waypoints) {
            j["waypoints"].push_back({{"name", w.name}, {"lat", w.lat}, {"lng", w.lng}});
        }
        j["speed"] = data.speed;
        j["weight"] = data.weight;
        ofstream f(dataFile);
        f << setw(2) << j << endl;
    }

    pair<double, double> geocode(const string& query) {
        string url = "https://nominatim.openstreetmap.org/search?q=" + curl_easy_escape(nullptr, query.c_str(), query.size()) + "&format=json&limit=1";
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "User-Agent: WalkRoutePlanner/1.0");
        string resp = fetchUrl(url);
        if (resp.empty()) return {0,0};
        auto data = json::parse(resp);
        if (data.empty()) return {0,0};
        double lat = stod(data[0]["lat"].get<string>());
        double lng = stod(data[0]["lon"].get<string>());
        return {lat, lng};
    }

public:
    RoutePlanner() { load(); }

    void add(const string& location) {
        if (location.find(',') != string::npos) {
            size_t pos = location.find(',');
            string latStr = location.substr(0, pos);
            string lngStr = location.substr(pos+1);
            double lat = stod(latStr);
            double lng = stod(lngStr);
            if (lat != 0.0 || lng != 0.0) {
                char name[64];
                snprintf(name, sizeof(name), "(%.4f,%.4f)", lat, lng);
                data.waypoints.push_back({name, lat, lng});
                save();
                cout << "✅ Added waypoint: " << name << "\n";
                return;
            }
        }
        auto coords = geocode(location);
        if (coords.first == 0 && coords.second == 0) {
            cout << "❌ Could not geocode '" << location << "'\n";
            return;
        }
        data.waypoints.push_back({location, coords.first, coords.second});
        save();
        char buf[64];
        snprintf(buf, sizeof(buf), "(%.4f,%.4f)", coords.first, coords.second);
        cout << "✅ Added waypoint: " << location << " " << buf << "\n";
    }

    void list() {
        if (data.waypoints.empty()) {
            cout << "No waypoints.\n";
            return;
        }
        cout << "\n📍 Waypoints:\n";
        for (size_t i=0; i<data.waypoints.size(); i++) {
            auto& w = data.waypoints[i];
            cout << i+1 << ": " << w.name << " (" << w.lat << ", " << w.lng << ")\n";
        }
    }

    void remove(int index) {
        if (index < 1 || index > (int)data.waypoints.size()) {
            cout << "❌ Invalid index. Must be between 1 and " << data.waypoints.size() << "\n";
            return;
        }
        auto removed = data.waypoints[index-1];
        data.waypoints.erase(data.waypoints.begin() + index-1);
        save();
        cout << "✅ Removed waypoint: " << removed.name << "\n";
    }

    void clear() {
        data.waypoints.clear();
        save();
        cout << "✅ All waypoints cleared.\n";
    }

    void speed(double spd) {
        data.speed = spd;
        save();
        cout << "✅ Walking speed set to " << data.speed << " km/h\n";
    }

    void route() {
        if (data.waypoints.size() < 2) {
            cout << "Need at least 2 waypoints to calculate a route.\n";
            return;
        }
        string coords;
        for (auto& w : data.waypoints) {
            if (!coords.empty()) coords += ";";
            coords += to_string(w.lng) + "," + to_string(w.lat);
        }
        string url = "http://router.project-osrm.org/route/v1/walking/" + coords + "?overview=full&geometries=polyline&steps=true";
        string resp = fetchUrl(url);
        if (resp.empty()) {
            cout << "❌ Error fetching route.\n";
            return;
        }
        auto dataJson = json::parse(resp);
        if (!dataJson.contains("routes") || dataJson["routes"].empty()) {
            cout << "❌ No route found.\n";
            return;
        }
        auto route = dataJson["routes"][0];
        double distanceKm = route["distance"] / 1000;
        double durationMin = route["duration"] / 60;
        double elevation = distanceKm * 12;
        double calories = distanceKm * CALORIES_PER_KM * (this->data.weight / 70.0);

        cout << "\n🚶 Walking Route:\n";
        cout << "Total distance: " << fixed << setprecision(2) << distanceKm << " km\n";
        cout << "Total duration: " << fixed << setprecision(1) << durationMin << " min (at " << data.speed << " km/h)\n";
        cout << "Elevation gain: ~" << fixed << setprecision(0) << elevation << " m\n";
        cout << "Calories burned: ~" << fixed << setprecision(0) << calories << " kcal\n";

        if (route.contains("legs")) {
            cout << "\nTurn-by-turn:\n";
            int stepNum = 1;
            for (auto& leg : route["legs"]) {
                for (auto& step : leg["steps"]) {
                    string instruction = step["maneuver"]["instruction"];
                    double dist = step["distance"] / 1000;
                    cout << stepNum << ". " << instruction << " (" << fixed << setprecision(2) << dist << " km)\n";
                    stepNum++;
                }
            }
        }
    }
};

int main(int argc, char* argv[]) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    if (argc < 2) {
        cerr << "Usage: walk_route <command> [options]\n";
        curl_global_cleanup();
        return 1;
    }
    RoutePlanner planner;
    string cmd = argv[1];

    if (cmd == "add") {
        if (argc < 3) { cerr << "add <location>\n"; curl_global_cleanup(); return 1; }
        planner.add(argv[2]);
    } else if (cmd == "list") {
        planner.list();
    } else if (cmd == "route") {
        planner.route();
    } else if (cmd == "remove") {
        if (argc < 3) { cerr << "remove <index>\n"; curl_global_cleanup(); return 1; }
        planner.remove(stoi(argv[2]));
    } else if (cmd == "clear") {
        planner.clear();
    } else if (cmd == "speed") {
        if (argc < 3) { cerr << "speed <kmh>\n"; curl_global_cleanup(); return 1; }
        planner.speed(stod(argv[2]));
    } else {
        cerr << "Unknown command. Use add, list, route, remove, clear, speed.\n";
        curl_global_cleanup();
        return 1;
    }
    curl_global_cleanup();
    return 0;
}
