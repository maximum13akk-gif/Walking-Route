🚶 Walking Route Planner — Multi‑Language Pedestrian Route Optimizer
8 languages, one powerful walking route planner – plan scenic walking routes with multiple waypoints, get distances, durations, elevation gain, and turn‑by‑turn directions – right from your terminal.

✨ Features
📍 Add waypoints – by address (geocoded) or direct coordinates (lat,lng)

🧭 Automatic geocoding – convert addresses to coordinates using Nominatim

🚶 Pedestrian routing – optimized for walking paths using OSRM

📏 Distance & duration – get accurate walking times and distances

⛰️ Elevation profile – estimate total elevation gain/loss (where available)

🔥 Calorie estimation – based on distance, weight, and walking speed

📋 List waypoints – see all saved locations with indices

🔄 Remove waypoints – delete by index

🗑️ Clear all – reset the route

📁 Save/Load routes – persistent storage in walk_route.json

🧰 Supported Languages & Files
Language	File	Dependencies
Python	walk_route.py	requests
Go	walk_route.go	none (stdlib)
JavaScript (Node)	walk_route.js	axios, commander
Ruby	walk_route.rb	httparty, json
PHP	walk_route.php	curl, json (ext)
Java	WalkRoute.java	java.net.http, Gson
C#	WalkRoute.cs	System.Net.Http, System.Text.Json
C++	walk_route.cpp	libcurl, nlohmann/json
🚀 Quick Start
All implementations follow the same CLI pattern:

bash
# Add a waypoint by address
<command> add "Eiffel Tower, Paris"

# Add by coordinates
<command> add "48.8584,2.2945"

# List waypoints
<command> list

# Calculate and display walking route
<command> route

# Remove waypoint by index
<command> remove 1

# Clear all waypoints
<command> clear

# Set walking speed (km/h)
<command> speed 5
Commands:

add <location> – add a waypoint (address or lat,lng)

list – show all waypoints with indices

route – compute and display the walking route

remove <index> – remove waypoint at index (1‑based)

clear – remove all waypoints

speed <kmh> – set walking speed (default: 5 km/h)

📸 Example Output
text
🚶 Walking Route Planner
Waypoints:
1: Eiffel Tower, Paris (48.8584, 2.2945)
2: Louvre Museum (48.8606, 2.3376)
3: Notre-Dame (48.8529, 2.3499)

🚶 Walking Route:
Total distance: 3.8 km
Total duration: 46 min (at 5.0 km/h)
Elevation gain: 45 m
Calories burned: 210 kcal

Turn-by-turn:
1. Start at Eiffel Tower
2. Walk east on Avenue Gustave Eiffel (0.2 km)
3. Turn right onto Quai Branly (0.5 km)
4. Cross Pont de l'Alma (0.3 km)
...
📁 Repository Structure
text
.
├── README.md
├── python/
│   └── walk_route.py
├── go/
│   └── walk_route.go
├── javascript/
│   └── walk_route.js
├── ruby/
│   └── walk_route.rb
├── php/
│   └── walk_route.php
├── java/
│   └── WalkRoute.java
├── csharp/
│   └── WalkRoute.cs
└── cpp/
    └── walk_route.cpp
