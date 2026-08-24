// walk_route.go
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"net/http"
	"os"
	"strconv"
	"strings"
)

type Waypoint struct {
	Name string  `json:"name"`
	Lat  float64 `json:"lat"`
	Lng  float64 `json:"lng"`
}

type RouteData struct {
	Waypoints []Waypoint `json:"waypoints"`
	Speed     float64    `json:"speed"`
	Weight    float64    `json:"weight"`
}

type RoutePlanner struct {
	Data RouteData `json:"data"`
	File string
}

func NewRoutePlanner(file string) *RoutePlanner {
	rp := &RoutePlanner{File: file}
	rp.load()
	if rp.Data.Speed == 0 {
		rp.Data.Speed = 5.0
	}
	if rp.Data.Weight == 0 {
		rp.Data.Weight = 70.0
	}
	return rp
}

func (rp *RoutePlanner) load() {
	data, err := os.ReadFile(rp.File)
	if err != nil {
		return
	}
	json.Unmarshal(data, &rp.Data)
}

func (rp *RoutePlanner) save() {
	data, _ := json.MarshalIndent(rp.Data, "", "  ")
	os.WriteFile(rp.File, data, 0644)
}

func geocode(query string) (float64, float64, error) {
	url := fmt.Sprintf("https://nominatim.openstreetmap.org/search?q=%s&format=json&limit=1", query)
	req, _ := http.NewRequest("GET", url, nil)
	req.Header.Set("User-Agent", "WalkRoutePlanner/1.0")
	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return 0, 0, err
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(resp.Body)
	var data []map[string]string
	if err := json.Unmarshal(body, &data); err != nil {
		return 0, 0, err
	}
	if len(data) == 0 {
		return 0, 0, fmt.Errorf("not found")
	}
	lat, _ := strconv.ParseFloat(data[0]["lat"], 64)
	lng, _ := strconv.ParseFloat(data[0]["lon"], 64)
	return lat, lng, nil
}

func (rp *RoutePlanner) add(location string) {
	if strings.Contains(location, ",") {
		parts := strings.Split(location, ",")
		if len(parts) == 2 {
			lat, err1 := strconv.ParseFloat(strings.TrimSpace(parts[0]), 64)
			lng, err2 := strconv.ParseFloat(strings.TrimSpace(parts[1]), 64)
			if err1 == nil && err2 == nil {
				name := fmt.Sprintf("(%.4f,%.4f)", lat, lng)
				rp.Data.Waypoints = append(rp.Data.Waypoints, Waypoint{Name: name, Lat: lat, Lng: lng})
				rp.save()
				fmt.Printf("✅ Added waypoint: %s\n", name)
				return
			}
		}
	}
	lat, lng, err := geocode(location)
	if err != nil {
		fmt.Printf("❌ Could not geocode '%s': %v\n", location, err)
		return
	}
	rp.Data.Waypoints = append(rp.Data.Waypoints, Waypoint{Name: location, Lat: lat, Lng: lng})
	rp.save()
	fmt.Printf("✅ Added waypoint: %s (%.4f, %.4f)\n", location, lat, lng)
}

func (rp *RoutePlanner) list() {
	if len(rp.Data.Waypoints) == 0 {
		fmt.Println("No waypoints.")
		return
	}
	fmt.Println("\n📍 Waypoints:")
	for i, w := range rp.Data.Waypoints {
		fmt.Printf("%d: %s (%.4f, %.4f)\n", i+1, w.Name, w.Lat, w.Lng)
	}
}

func (rp *RoutePlanner) remove(index int) {
	if index < 1 || index > len(rp.Data.Waypoints) {
		fmt.Printf("❌ Invalid index. Must be between 1 and %d\n", len(rp.Data.Waypoints))
		return
	}
	removed := rp.Data.Waypoints[index-1]
	rp.Data.Waypoints = append(rp.Data.Waypoints[:index-1], rp.Data.Waypoints[index:]...)
	rp.save()
	fmt.Printf("✅ Removed waypoint: %s\n", removed.Name)
}

func (rp *RoutePlanner) clear() {
	rp.Data.Waypoints = []Waypoint{}
	rp.save()
	fmt.Println("✅ All waypoints cleared.")
}

func (rp *RoutePlanner) speed(speed float64) {
	rp.Data.Speed = speed
	rp.save()
	fmt.Printf("✅ Walking speed set to %.1f km/h\n", speed)
}

func (rp *RoutePlanner) route() {
	if len(rp.Data.Waypoints) < 2 {
		fmt.Println("Need at least 2 waypoints to calculate a route.")
		return
	}
	coords := ""
	for i, w := range rp.Data.Waypoints {
		if i > 0 {
			coords += ";"
		}
		coords += fmt.Sprintf("%f,%f", w.Lng, w.Lat)
	}
	url := fmt.Sprintf("http://router.project-osrm.org/route/v1/walking/%s?overview=full&geometries=polyline&steps=true", coords)
	resp, err := http.Get(url)
	if err != nil {
		fmt.Printf("❌ Error fetching route: %v\n", err)
		return
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(resp.Body)
	var data map[string]interface{}
	if err := json.Unmarshal(body, &data); err != nil {
		fmt.Println("❌ Invalid response from routing service.")
		return
	}
	routes, ok := data["routes"].([]interface{})
	if !ok || len(routes) == 0 {
		fmt.Println("❌ No route found.")
		return
	}
	route := routes[0].(map[string]interface{})
	distanceKm := route["distance"].(float64) / 1000
	durationMin := route["duration"].(float64) / 60
	// Estimate elevation gain
	elevation := distanceKm * 12
	calories := distanceKm * 55 * (rp.Data.Weight / 70.0)

	fmt.Printf("\n🚶 Walking Route:\n")
	fmt.Printf("Total distance: %.2f km\n", distanceKm)
	fmt.Printf("Total duration: %.1f min (at %.1f km/h)\n", durationMin, rp.Data.Speed)
	fmt.Printf("Elevation gain: ~%.0f m\n", elevation)
	fmt.Printf("Calories burned: ~%.0f kcal\n", calories)

	legs, ok := route["legs"].([]interface{})
	if ok && len(legs) > 0 {
		fmt.Println("\nTurn-by-turn:")
		stepNum := 1
		for _, leg := range legs {
			steps, _ := leg.(map[string]interface{})["steps"].([]interface{})
			for _, s := range steps {
				st := s.(map[string]interface{})
				maneuver := st["maneuver"].(map[string]interface{})
				instruction := maneuver["instruction"].(string)
				dist := st["distance"].(float64) / 1000
				fmt.Printf("%d. %s (%.2f km)\n", stepNum, instruction, dist)
				stepNum++
			}
		}
	}
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: walk_route <command> [options]")
		return
	}
	rp := NewRoutePlanner("walk_route.json")
	cmd := os.Args[1]
	switch cmd {
	case "add":
		if len(os.Args) < 3 {
			fmt.Println("add <location>")
			return
		}
		rp.add(os.Args[2])
	case "list":
		rp.list()
	case "route":
		rp.route()
	case "remove":
		if len(os.Args) < 3 {
			fmt.Println("remove <index>")
			return
		}
		idx, _ := strconv.Atoi(os.Args[2])
		rp.remove(idx)
	case "clear":
		rp.clear()
	case "speed":
		if len(os.Args) < 3 {
			fmt.Println("speed <kmh>")
			return
		}
		speed, _ := strconv.ParseFloat(os.Args[2], 64)
		rp.speed(speed)
	default:
		fmt.Println("Unknown command. Use add, list, route, remove, clear, speed.")
	}
}
