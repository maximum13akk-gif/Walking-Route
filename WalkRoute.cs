// WalkRoute.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;

class Waypoint
{
    [JsonPropertyName("name")]
    public string Name { get; set; }
    [JsonPropertyName("lat")]
    public double Lat { get; set; }
    [JsonPropertyName("lng")]
    public double Lng { get; set; }

    public Waypoint() { }
    public Waypoint(string name, double lat, double lng)
    {
        Name = name;
        Lat = lat;
        Lng = lng;
    }
}

class RouteData
{
    [JsonPropertyName("waypoints")]
    public List<Waypoint> Waypoints { get; set; } = new List<Waypoint>();
    [JsonPropertyName("speed")]
    public double Speed { get; set; } = 5.0;
    [JsonPropertyName("weight")]
    public double Weight { get; set; } = 70.0;
}

class RoutePlanner
{
    private RouteData data = new RouteData();
    private readonly string dataFile = "walk_route.json";
    private readonly HttpClient client = new HttpClient();
    private readonly JsonSerializerOptions options = new JsonSerializerOptions { WriteIndented = true };

    public RoutePlanner() => Load();

    private void Load()
    {
        if (!File.Exists(dataFile)) return;
        string json = File.ReadAllText(dataFile);
        data = JsonSerializer.Deserialize<RouteData>(json) ?? new RouteData();
    }

    private void Save()
    {
        string json = JsonSerializer.Serialize(data, options);
        File.WriteAllText(dataFile, json);
    }

    private async Task<(double lat, double lng)?> Geocode(string query)
    {
        string url = $"https://nominatim.openstreetmap.org/search?q={Uri.EscapeDataString(query)}&format=json&limit=1";
        using var req = new HttpRequestMessage(HttpMethod.Get, url);
        req.Headers.Add("User-Agent", "WalkRoutePlanner/1.0");
        var resp = await client.SendAsync(req);
        if (!resp.IsSuccessStatusCode) return null;
        string json = await resp.Content.ReadAsStringAsync();
        var arr = JsonSerializer.Deserialize<List<Dictionary<string, string>>>(json);
        if (arr == null || arr.Count == 0) return null;
        double lat = double.Parse(arr[0]["lat"]);
        double lng = double.Parse(arr[0]["lon"]);
        return (lat, lng);
    }

    public async Task Add(string location)
    {
        if (location.Contains(','))
        {
            var parts = location.Split(',').Select(s => s.Trim()).ToArray();
            if (parts.Length == 2 && double.TryParse(parts[0], out double lat) && double.TryParse(parts[1], out double lng))
            {
                string name = $"({lat:F4},{lng:F4})";
                data.Waypoints.Add(new Waypoint(name, lat, lng));
                Save();
                Console.WriteLine($"✅ Added waypoint: {name}");
                return;
            }
        }
        var coords = await Geocode(location);
        if (!coords.HasValue)
        {
            Console.WriteLine($"❌ Could not geocode '{location}'");
            return;
        }
        data.Waypoints.Add(new Waypoint(location, coords.Value.lat, coords.Value.lng));
        Save();
        Console.WriteLine($"✅ Added waypoint: {location} ({coords.Value.lat:F4}, {coords.Value.lng:F4})");
    }

    public void List()
    {
        if (data.Waypoints.Count == 0)
        {
            Console.WriteLine("No waypoints.");
            return;
        }
        Console.WriteLine("\n📍 Waypoints:");
        for (int i = 0; i < data.Waypoints.Count; i++)
        {
            var w = data.Waypoints[i];
            Console.WriteLine($"{i+1}: {w.Name} ({w.Lat:F4}, {w.Lng:F4})");
        }
    }

    public void Remove(int index)
    {
        if (index < 1 || index > data.Waypoints.Count)
        {
            Console.WriteLine($"❌ Invalid index. Must be between 1 and {data.Waypoints.Count}");
            return;
        }
        var removed = data.Waypoints[index-1];
        data.Waypoints.RemoveAt(index-1);
        Save();
        Console.WriteLine($"✅ Removed waypoint: {removed.Name}");
    }

    public void Clear()
    {
        data.Waypoints.Clear();
        Save();
        Console.WriteLine("✅ All waypoints cleared.");
    }

    public void Speed(double speed)
    {
        data.Speed = speed;
        Save();
        Console.WriteLine($"✅ Walking speed set to {speed:F1} km/h");
    }

    public async Task Route()
    {
        if (data.Waypoints.Count < 2)
        {
            Console.WriteLine("Need at least 2 waypoints to calculate a route.");
            return;
        }
        string coords = string.Join(";", data.Waypoints.Select(w => $"{w.Lng},{w.Lat}"));
        string url = $"http://router.project-osrm.org/route/v1/walking/{coords}?overview=full&geometries=polyline&steps=true";
        var resp = await client.GetAsync(url);
        if (!resp.IsSuccessStatusCode)
        {
            Console.WriteLine("❌ Error fetching route.");
            return;
        }
        string json = await resp.Content.ReadAsStringAsync();
        using var doc = JsonDocument.Parse(json);
        var root = doc.RootElement;
        if (!root.TryGetProperty("routes", out var routesElem) || routesElem.GetArrayLength() == 0)
        {
            Console.WriteLine("❌ No route found.");
            return;
        }
        var route = routesElem[0];
        double distanceKm = route.GetProperty("distance").GetDouble() / 1000;
        double durationMin = route.GetProperty("duration").GetDouble() / 60;
        double elevation = distanceKm * 12;
        double calories = distanceKm * 55 * (data.Weight / 70.0);

        Console.WriteLine("\n🚶 Walking Route:");
        Console.WriteLine($"Total distance: {distanceKm:F2} km");
        Console.WriteLine($"Total duration: {durationMin:F1} min (at {data.Speed:F1} km/h)");
        Console.WriteLine($"Elevation gain: ~{elevation:F0} m");
        Console.WriteLine($"Calories burned: ~{calories:F0} kcal");

        if (route.TryGetProperty("legs", out var legsElem))
        {
            Console.WriteLine("\nTurn-by-turn:");
            int stepNum = 1;
            foreach (var leg in legsElem.EnumerateArray())
            {
                if (leg.TryGetProperty("steps", out var stepsElem))
                {
                    foreach (var step in stepsElem.EnumerateArray())
                    {
                        string instruction = step.GetProperty("maneuver").GetProperty("instruction").GetString();
                        double dist = step.GetProperty("distance").GetDouble() / 1000;
                        Console.WriteLine($"{stepNum}. {instruction} ({dist:F2} km)");
                        stepNum++;
                    }
                }
            }
        }
    }

    static async Task Main(string[] args)
    {
        if (args.Length < 1)
        {
            Console.WriteLine("Usage: WalkRoute <command> [options]");
            return;
        }
        var planner = new RoutePlanner();
        string cmd = args[0];
        switch (cmd)
        {
            case "add":
                if (args.Length < 2) { Console.WriteLine("add <location>"); return; }
                await planner.Add(args[1]);
                break;
            case "list":
                planner.List();
                break;
            case "route":
                await planner.Route();
                break;
            case "remove":
                if (args.Length < 2) { Console.WriteLine("remove <index>"); return; }
                planner.Remove(int.Parse(args[1]));
                break;
            case "clear":
                planner.Clear();
                break;
            case "speed":
                if (args.Length < 2) { Console.WriteLine("speed <kmh>"); return; }
                planner.Speed(double.Parse(args[1]));
                break;
            default:
                Console.WriteLine("Unknown command. Use add, list, route, remove, clear, speed.");
                break;
        }
    }
}
