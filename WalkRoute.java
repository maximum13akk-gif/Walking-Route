// WalkRoute.java
import java.io.*;
import java.net.*;
import java.nio.file.*;
import java.util.*;
import com.google.gson.*;

class Waypoint {
    String name;
    double lat;
    double lng;

    Waypoint(String name, double lat, double lng) {
        this.name = name;
        this.lat = lat;
        this.lng = lng;
    }
}

class RouteData {
    List<Waypoint> waypoints = new ArrayList<>();
    double speed = 5.0;
    double weight = 70.0;
}

class RoutePlanner {
    private RouteData data = new RouteData();
    private final String dataFile = "walk_route.json";
    private final Gson gson = new GsonBuilder().setPrettyPrinting().create();

    public RoutePlanner() { load(); }

    private void load() {
        try {
            Path path = Paths.get(dataFile);
            if (Files.exists(path)) {
                String json = new String(Files.readAllBytes(path));
                data = gson.fromJson(json, RouteData.class);
            }
        } catch (Exception e) {}
    }

    private void save() {
        try {
            Files.write(Paths.get(dataFile), gson.toJson(data).getBytes());
        } catch (Exception e) {}
    }

    private double[] geocode(String query) throws Exception {
        String url = "https://nominatim.openstreetmap.org/search?q=" + URLEncoder.encode(query, "UTF-8") + "&format=json&limit=1";
        HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
        conn.setRequestMethod("GET");
        conn.setRequestProperty("User-Agent", "WalkRoutePlanner/1.0");
        BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream()));
        StringBuilder sb = new StringBuilder();
        String line;
        while ((line = reader.readLine()) != null) sb.append(line);
        reader.close();
        JsonArray arr = gson.fromJson(sb.toString(), JsonArray.class);
        if (arr.size() == 0) return null;
        JsonObject obj = arr.get(0).getAsJsonObject();
        double lat = obj.get("lat").getAsDouble();
        double lng = obj.get("lon").getAsDouble();
        return new double[]{lat, lng};
    }

    public void add(String location) throws Exception {
        if (location.contains(",")) {
            String[] parts = location.split(",");
            if (parts.length == 2) {
                try {
                    double lat = Double.parseDouble(parts[0].trim());
                    double lng = Double.parseDouble(parts[1].trim());
                    String name = String.format("(%.4f,%.4f)", lat, lng);
                    data.waypoints.add(new Waypoint(name, lat, lng));
                    save();
                    System.out.println("✅ Added waypoint: " + name);
                    return;
                } catch (NumberFormatException e) {}
            }
        }
        double[] coords = geocode(location);
        if (coords == null) {
            System.out.println("❌ Could not geocode '" + location + "'");
            return;
        }
        data.waypoints.add(new Waypoint(location, coords[0], coords[1]));
        save();
        System.out.printf("✅ Added waypoint: %s (%.4f, %.4f)%n", location, coords[0], coords[1]);
    }

    public void list() {
        if (data.waypoints.isEmpty()) {
            System.out.println("No waypoints.");
            return;
        }
        System.out.println("\n📍 Waypoints:");
        for (int i = 0; i < data.waypoints.size(); i++) {
            Waypoint w = data.waypoints.get(i);
            System.out.printf("%d: %s (%.4f, %.4f)%n", i+1, w.name, w.lat, w.lng);
        }
    }

    public void remove(int index) {
        if (index < 1 || index > data.waypoints.size()) {
            System.out.printf("❌ Invalid index. Must be between 1 and %d%n", data.waypoints.size());
            return;
        }
        Waypoint removed = data.waypoints.remove(index-1);
        save();
        System.out.println("✅ Removed waypoint: " + removed.name);
    }

    public void clear() {
        data.waypoints.clear();
        save();
        System.out.println("✅ All waypoints cleared.");
    }

    public void speed(double speed) {
        data.speed = speed;
        save();
        System.out.printf("✅ Walking speed set to %.1f km/h%n", speed);
    }

    public void route() throws Exception {
        if (data.waypoints.size() < 2) {
            System.out.println("Need at least 2 waypoints to calculate a route.");
            return;
        }
        StringBuilder coords = new StringBuilder();
        for (Waypoint w : data.waypoints) {
            if (coords.length() > 0) coords.append(";");
            coords.append(w.lng).append(",").append(w.lat);
        }
        String url = "http://router.project-osrm.org/route/v1/walking/" + coords + "?overview=full&geometries=polyline&steps=true";
        HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
        conn.setRequestMethod("GET");
        BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream()));
        StringBuilder sb = new StringBuilder();
        String line;
        while ((line = reader.readLine()) != null) sb.append(line);
        reader.close();
        JsonObject obj = gson.fromJson(sb.toString(), JsonObject.class);
        if (!obj.has("routes") || obj.get("routes").getAsJsonArray().size() == 0) {
            System.out.println("❌ No route found.");
            return;
        }
        JsonObject route = obj.get("routes").getAsJsonArray().get(0).getAsJsonObject();
        double distanceKm = route.get("distance").getAsDouble() / 1000;
        double durationMin = route.get("duration").getAsDouble() / 60;
        double elevation = distanceKm * 12;
        double calories = distanceKm * 55 * (data.weight / 70.0);

        System.out.println("\n🚶 Walking Route:");
        System.out.printf("Total distance: %.2f km%n", distanceKm);
        System.out.printf("Total duration: %.1f min (at %.1f km/h)%n", durationMin, data.speed);
        System.out.printf("Elevation gain: ~%.0f m%n", elevation);
        System.out.printf("Calories burned: ~%.0f kcal%n", calories);

        if (route.has("legs")) {
            System.out.println("\nTurn-by-turn:");
            int stepNum = 1;
            for (JsonElement legElem : route.get("legs").getAsJsonArray()) {
                JsonObject leg = legElem.getAsJsonObject();
                if (leg.has("steps")) {
                    for (JsonElement stepElem : leg.get("steps").getAsJsonArray()) {
                        JsonObject step = stepElem.getAsJsonObject();
                        JsonObject maneuver = step.get("maneuver").getAsJsonObject();
                        String instruction = maneuver.get("instruction").getAsString();
                        double dist = step.get("distance").getAsDouble() / 1000;
                        System.out.printf("%d. %s (%.2f km)%n", stepNum, instruction, dist);
                        stepNum++;
                    }
                }
            }
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length < 1) {
            System.out.println("Usage: WalkRoute <command> [options]");
            return;
        }
        RoutePlanner planner = new RoutePlanner();
        String cmd = args[0];
        switch (cmd) {
            case "add":
                if (args.length < 2) { System.out.println("add <location>"); return; }
                planner.add(args[1]);
                break;
            case "list":
                planner.list();
                break;
            case "route":
                planner.route();
                break;
            case "remove":
                if (args.length < 2) { System.out.println("remove <index>"); return; }
                planner.remove(Integer.parseInt(args[1]));
                break;
            case "clear":
                planner.clear();
                break;
            case "speed":
                if (args.length < 2) { System.out.println("speed <kmh>"); return; }
                planner.speed(Double.parseDouble(args[1]));
                break;
            default:
                System.out.println("Unknown command. Use add, list, route, remove, clear, speed.");
        }
    }
}
