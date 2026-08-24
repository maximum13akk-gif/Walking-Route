# walk_route.py
import sys
import os
import json
import requests
import argparse
import math
from datetime import datetime

DATA_FILE = "walk_route.json"
WALKING_SPEED = 5.0  # km/h
CALORIES_PER_KM = 55  # approx for a 70kg person

class Waypoint:
    def __init__(self, name, lat, lng):
        self.name = name
        self.lat = lat
        self.lng = lng

    def to_dict(self):
        return {"name": self.name, "lat": self.lat, "lng": self.lng}

    @classmethod
    def from_dict(cls, d):
        return cls(d["name"], d["lat"], d["lng"])

class RoutePlanner:
    def __init__(self):
        self.waypoints = []
        self.speed = WALKING_SPEED
        self.weight = 70  # kg
        self.load()

    def load(self):
        if os.path.exists(DATA_FILE):
            with open(DATA_FILE, "r") as f:
                data = json.load(f)
                self.waypoints = [Waypoint.from_dict(w) for w in data.get("waypoints", [])]
                self.speed = data.get("speed", WALKING_SPEED)
                self.weight = data.get("weight", 70)

    def save(self):
        with open(DATA_FILE, "w") as f:
            json.dump({
                "waypoints": [w.to_dict() for w in self.waypoints],
                "speed": self.speed,
                "weight": self.weight
            }, f, indent=2)

    def geocode(self, query):
        url = "https://nominatim.openstreetmap.org/search"
        params = {"q": query, "format": "json", "limit": 1}
        headers = {"User-Agent": "WalkRoutePlanner/1.0"}
        resp = requests.get(url, params=params, headers=headers, timeout=10)
        if resp.status_code != 200:
            return None
        data = resp.json()
        if not data:
            return None
        return float(data[0]["lat"]), float(data[0]["lon"])

    def add(self, location):
        if "," in location:
            try:
                parts = location.split(",")
                lat = float(parts[0].strip())
                lng = float(parts[1].strip())
                name = f"({lat:.4f},{lng:.4f})"
                self.waypoints.append(Waypoint(name, lat, lng))
                self.save()
                print(f"✅ Added waypoint: {name}")
                return
            except ValueError:
                pass
        coords = self.geocode(location)
        if not coords:
            print(f"❌ Could not geocode '{location}'")
            return
        lat, lng = coords
        self.waypoints.append(Waypoint(location, lat, lng))
        self.save()
        print(f"✅ Added waypoint: {location} ({lat:.4f}, {lng:.4f})")

    def list(self):
        if not self.waypoints:
            print("No waypoints.")
            return
        print("\n📍 Waypoints:")
        for i, w in enumerate(self.waypoints, 1):
            print(f"{i}: {w.name} ({w.lat:.4f}, {w.lng:.4f})")

    def remove(self, index):
        if index < 1 or index > len(self.waypoints):
            print(f"❌ Invalid index. Must be between 1 and {len(self.waypoints)}")
            return
        removed = self.waypoints.pop(index - 1)
        self.save()
        print(f"✅ Removed waypoint: {removed.name}")

    def clear(self):
        self.waypoints = []
        self.save()
        print("✅ All waypoints cleared.")

    def set_speed(self, speed):
        self.speed = speed
        self.save()
        print(f"✅ Walking speed set to {speed:.1f} km/h")

    def route(self):
        if len(self.waypoints) < 2:
            print("Need at least 2 waypoints to calculate a route.")
            return
        coords = ";".join(f"{w.lng},{w.lat}" for w in self.waypoints)
        url = f"http://router.project-osrm.org/route/v1/walking/{coords}"
        params = {"overview": "full", "geometries": "polyline", "steps": "true"}
        resp = requests.get(url, params=params, timeout=10)
        if resp.status_code != 200:
            print("❌ Error fetching route.")
            return
        data = resp.json()
        if "routes" not in data or not data["routes"]:
            print("❌ No route found.")
            return
        route = data["routes"][0]
        distance_km = route["distance"] / 1000
        duration_min = route["duration"] / 60
        # Estimate elevation gain (simplified)
        elevation_gain = self.estimate_elevation(route)
        # Calories burned
        calories = distance_km * CALORIES_PER_KM * (self.weight / 70)

        print("\n🚶 Walking Route:")
        print(f"Total distance: {distance_km:.2f} km")
        print(f"Total duration: {duration_min:.1f} min (at {self.speed:.1f} km/h)")
        print(f"Elevation gain: ~{elevation_gain:.0f} m")
        print(f"Calories burned: ~{calories:.0f} kcal")

        legs = route.get("legs", [])
        if legs:
            print("\nTurn-by-turn:")
            step_num = 1
            for leg in legs:
                for step in leg.get("steps", []):
                    instruction = step.get("maneuver", {}).get("instruction", "")
                    dist = step.get("distance", 0) / 1000
                    if instruction:
                        print(f"{step_num}. {instruction} ({dist:.2f} km)")
                        step_num += 1

    def estimate_elevation(self, route):
        # Simplified elevation gain from route
        # In a real implementation, we would use elevation data from DEM
        # For now, return a dummy value based on distance
        distance_km = route["distance"] / 1000
        return distance_km * 12  # rough approximation: 12m per km

def main():
    parser = argparse.ArgumentParser(description="Walking Route Planner")
    subparsers = parser.add_subparsers(dest="cmd", required=True)

    add_parser = subparsers.add_parser("add")
    add_parser.add_argument("location")

    subparsers.add_parser("list")
    subparsers.add_parser("route")

    remove_parser = subparsers.add_parser("remove")
    remove_parser.add_argument("index", type=int)

    subparsers.add_parser("clear")

    speed_parser = subparsers.add_parser("speed")
    speed_parser.add_argument("speed", type=float)

    args = parser.parse_args()
    planner = RoutePlanner()

    if args.cmd == "add":
        planner.add(args.location)
    elif args.cmd == "list":
        planner.list()
    elif args.cmd == "route":
        planner.route()
    elif args.cmd == "remove":
        planner.remove(args.index)
    elif args.cmd == "clear":
        planner.clear()
    elif args.cmd == "speed":
        planner.set_speed(args.speed)

if __name__ == "__main__":
    main()
