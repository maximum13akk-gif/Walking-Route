# walk_route.php
#!/usr/bin/env php
<?php

define('DATA_FILE', 'walk_route.json');
define('DEFAULT_SPEED', 5.0);
define('CALORIES_PER_KM', 55);

class Waypoint {
    public $name;
    public $lat;
    public $lng;

    function __construct($name, $lat, $lng) {
        $this->name = $name;
        $this->lat = $lat;
        $this->lng = $lng;
    }

    function toArray() {
        return ['name' => $this->name, 'lat' => $this->lat, 'lng' => $this->lng];
    }

    static function fromArray($data) {
        return new self($data['name'], $data['lat'], $data['lng']);
    }
}

class RoutePlanner {
    private $waypoints = [];
    private $speed = DEFAULT_SPEED;
    private $weight = 70;

    function __construct() {
        $this->load();
    }

    function load() {
        if (file_exists(DATA_FILE)) {
            $data = json_decode(file_get_contents(DATA_FILE), true);
            foreach ($data['waypoints'] as $item) {
                $this->waypoints[] = Waypoint::fromArray($item);
            }
            $this->speed = $data['speed'] ?? DEFAULT_SPEED;
            $this->weight = $data['weight'] ?? 70;
        }
    }

    function save() {
        $data = [
            'waypoints' => array_map(function($w) { return $w->toArray(); }, $this->waypoints),
            'speed' => $this->speed,
            'weight' => $this->weight
        ];
        file_put_contents(DATA_FILE, json_encode($data, JSON_PRETTY_PRINT));
    }

    function geocode($query) {
        $url = "https://nominatim.openstreetmap.org/search?q=" . urlencode($query) . "&format=json&limit=1";
        $opts = ['http' => ['method' => 'GET', 'header' => "User-Agent: WalkRoutePlanner/1.0\r\n"]];
        $ctx = stream_context_create($opts);
        $resp = file_get_contents($url, false, $ctx);
        if ($resp === false) return null;
        $data = json_decode($resp, true);
        if (empty($data)) return null;
        return [(float)$data[0]['lat'], (float)$data[0]['lon']];
    }

    function add($location) {
        if (strpos($location, ',') !== false) {
            $parts = array_map('trim', explode(',', $location));
            if (count($parts) == 2) {
                $lat = (float)$parts[0];
                $lng = (float)$parts[1];
                if ($lat != 0.0 && $lng != 0.0) {
                    $name = sprintf("(%.4f,%.4f)", $lat, $lng);
                    $this->waypoints[] = new Waypoint($name, $lat, $lng);
                    $this->save();
                    echo "✅ Added waypoint: $name\n";
                    return;
                }
            }
        }
        $coords = $this->geocode($location);
        if (!$coords) {
            echo "❌ Could not geocode '$location'\n";
            return;
        }
        list($lat, $lng) = $coords;
        $this->waypoints[] = new Waypoint($location, $lat, $lng);
        $this->save();
        printf("✅ Added waypoint: %s (%.4f, %.4f)\n", $location, $lat, $lng);
    }

    function list() {
        if (empty($this->waypoints)) {
            echo "No waypoints.\n";
            return;
        }
        echo "\n📍 Waypoints:\n";
        foreach ($this->waypoints as $i => $w) {
            printf("%d: %s (%.4f, %.4f)\n", $i+1, $w->name, $w->lat, $w->lng);
        }
    }

    function remove($index) {
        if ($index < 1 || $index > count($this->waypoints)) {
            echo "❌ Invalid index. Must be between 1 and " . count($this->waypoints) . "\n";
            return;
        }
        $removed = array_splice($this->waypoints, $index-1, 1)[0];
        $this->save();
        echo "✅ Removed waypoint: {$removed->name}\n";
    }

    function clear() {
        $this->waypoints = [];
        $this->save();
        echo "✅ All waypoints cleared.\n";
    }

    function speed($speed) {
        $this->speed = $speed;
        $this->save();
        printf("✅ Walking speed set to %.1f km/h\n", $speed);
    }

    function route() {
        if (count($this->waypoints) < 2) {
            echo "Need at least 2 waypoints to calculate a route.\n";
            return;
        }
        $coords = implode(';', array_map(function($w) { return "$w->lng,$w->lat"; }, $this->waypoints));
        $url = "http://router.project-osrm.org/route/v1/walking/$coords?overview=full&geometries=polyline&steps=true";
        $resp = file_get_contents($url);
        if ($resp === false) {
            echo "❌ Error fetching route.\n";
            return;
        }
        $data = json_decode($resp, true);
        if (!isset($data['routes']) || empty($data['routes'])) {
            echo "❌ No route found.\n";
            return;
        }
        $route = $data['routes'][0];
        $distance_km = $route['distance'] / 1000;
        $duration_min = $route['duration'] / 60;
        $elevation = $distance_km * 12;
        $calories = $distance_km * CALORIES_PER_KM * ($this->weight / 70);

        echo "\n🚶 Walking Route:\n";
        printf("Total distance: %.2f km\n", $distance_km);
        printf("Total duration: %.1f min (at %.1f km/h)\n", $duration_min, $this->speed);
        printf("Elevation gain: ~%.0f m\n", $elevation);
        printf("Calories burned: ~%.0f kcal\n", $calories);

        if (isset($route['legs'])) {
            echo "\nTurn-by-turn:\n";
            $step_num = 1;
            foreach ($route['legs'] as $leg) {
                foreach ($leg['steps'] as $step) {
                    $instruction = $step['maneuver']['instruction'];
                    $dist = $step['distance'] / 1000;
                    printf("%d. %s (%.2f km)\n", $step_num, $instruction, $dist);
                    $step_num++;
                }
            }
        }
    }
}

if ($argc < 2) {
    die("Usage: php walk_route.php <command> [options]\n");
}
$cmd = $argv[1];
$planner = new RoutePlanner();

switch ($cmd) {
    case 'add':
        if ($argc < 3) die("add <location>\n");
        $planner->add($argv[2]);
        break;
    case 'list':
        $planner->list();
        break;
    case 'route':
        $planner->route();
        break;
    case 'remove':
        if ($argc < 3) die("remove <index>\n");
        $planner->remove((int)$argv[2]);
        break;
    case 'clear':
        $planner->clear();
        break;
    case 'speed':
        if ($argc < 3) die("speed <kmh>\n");
        $planner->speed((float)$argv[2]);
        break;
    default:
        echo "Unknown command. Use add, list, route, remove, clear, speed.\n";
}
?>
