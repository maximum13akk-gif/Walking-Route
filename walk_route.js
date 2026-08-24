// walk_route.js
#!/usr/bin/env node
const fs = require('fs');
const axios = require('axios');
const { program } = require('commander');

const DATA_FILE = 'walk_route.json';
const DEFAULT_SPEED = 5.0;
const CALORIES_PER_KM = 55;

class Waypoint {
    constructor(name, lat, lng) {
        this.name = name;
        this.lat = lat;
        this.lng = lng;
    }
}

class RoutePlanner {
    constructor() {
        this.waypoints = [];
        this.speed = DEFAULT_SPEED;
        this.weight = 70;
        this.load();
    }

    load() {
        if (fs.existsSync(DATA_FILE)) {
            try {
                const data = JSON.parse(fs.readFileSync(DATA_FILE));
                this.waypoints = data.waypoints.map(w => new Waypoint(w.name, w.lat, w.lng));
                this.speed = data.speed || DEFAULT_SPEED;
                this.weight = data.weight || 70;
            } catch (e) {}
        }
    }

    save() {
        fs.writeFileSync(DATA_FILE, JSON.stringify({
            waypoints: this.waypoints,
            speed: this.speed,
            weight: this.weight
        }, null, 2));
    }

    async geocode(query) {
        const url = 'https://nominatim.openstreetmap.org/search';
        const params = { q: query, format: 'json', limit: 1 };
        const resp = await axios.get(url, { params, headers: { 'User-Agent': 'WalkRoutePlanner/1.0' }, timeout: 10000 });
        if (resp.data.length === 0) return null;
        return { lat: parseFloat(resp.data[0].lat), lng: parseFloat(resp.data[0].lon) };
    }

    async add(location) {
        if (location.includes(',')) {
            const parts = location.split(',').map(s => s.trim());
            if (parts.length === 2) {
                const lat = parseFloat(parts[0]);
                const lng = parseFloat(parts[1]);
                if (!isNaN(lat) && !isNaN(lng)) {
                    const name = `(${lat.toFixed(4)},${lng.toFixed(4)})`;
                    this.waypoints.push(new Waypoint(name, lat, lng));
                    this.save();
                    console.log(`✅ Added waypoint: ${name}`);
                    return;
                }
            }
        }
        const coords = await this.geocode(location);
        if (!coords) {
            console.log(`❌ Could not geocode '${location}'`);
            return;
        }
        this.waypoints.push(new Waypoint(location, coords.lat, coords.lng));
        this.save();
        console.log(`✅ Added waypoint: ${location} (${coords.lat.toFixed(4)}, ${coords.lng.toFixed(4)})`);
    }

    list() {
        if (this.waypoints.length === 0) {
            console.log('No waypoints.');
            return;
        }
        console.log('\n📍 Waypoints:');
        this.waypoints.forEach((w, i) => {
            console.log(`${i+1}: ${w.name} (${w.lat.toFixed(4)}, ${w.lng.toFixed(4)})`);
        });
    }

    remove(index) {
        if (index < 1 || index > this.waypoints.length) {
            console.log(`❌ Invalid index. Must be between 1 and ${this.waypoints.length}`);
            return;
        }
        const removed = this.waypoints.splice(index-1, 1)[0];
        this.save();
        console.log(`✅ Removed waypoint: ${removed.name}`);
    }

    clear() {
        this.waypoints = [];
        this.save();
        console.log('✅ All waypoints cleared.');
    }

    speed(speed) {
        this.speed = speed;
        this.save();
        console.log(`✅ Walking speed set to ${speed.toFixed(1)} km/h`);
    }

    async route() {
        if (this.waypoints.length < 2) {
            console.log('Need at least 2 waypoints to calculate a route.');
            return;
        }
        const coords = this.waypoints.map(w => `${w.lng},${w.lat}`).join(';');
        const url = `http://router.project-osrm.org/route/v1/walking/${coords}?overview=full&geometries=polyline&steps=true`;
        try {
            const resp = await axios.get(url, { timeout: 10000 });
            const data = resp.data;
            if (!data.routes || data.routes.length === 0) {
                console.log('❌ No route found.');
                return;
            }
            const route = data.routes[0];
            const distanceKm = route.distance / 1000;
            const durationMin = route.duration / 60;
            const elevation = distanceKm * 12;
            const calories = distanceKm * CALORIES_PER_KM * (this.weight / 70);

            console.log('\n🚶 Walking Route:');
            console.log(`Total distance: ${distanceKm.toFixed(2)} km`);
            console.log(`Total duration: ${durationMin.toFixed(1)} min (at ${this.speed.toFixed(1)} km/h)`);
            console.log(`Elevation gain: ~${elevation.toFixed(0)} m`);
            console.log(`Calories burned: ~${calories.toFixed(0)} kcal`);

            if (route.legs) {
                console.log('\nTurn-by-turn:');
                let stepNum = 1;
                for (const leg of route.legs) {
                    for (const step of leg.steps || []) {
                        const instruction = step.maneuver?.instruction || '';
                        const dist = step.distance / 1000;
                        if (instruction) {
                            console.log(`${stepNum}. ${instruction} (${dist.toFixed(2)} km)`);
                            stepNum++;
                        }
                    }
                }
            }
        } catch (err) {
            console.log(`❌ Error fetching route: ${err.message}`);
        }
    }
}

program
    .command('add <location>')
    .action(async (location) => {
        const planner = new RoutePlanner();
        await planner.add(location);
    });

program
    .command('list')
    .action(() => {
        const planner = new RoutePlanner();
        planner.list();
    });

program
    .command('route')
    .action(async () => {
        const planner = new RoutePlanner();
        await planner.route();
    });

program
    .command('remove <index>')
    .action((index) => {
        const planner = new RoutePlanner();
        planner.remove(parseInt(index));
    });

program
    .command('clear')
    .action(() => {
        const planner = new RoutePlanner();
        planner.clear();
    });

program
    .command('speed <kmh>')
    .action((speed) => {
        const planner = new RoutePlanner();
        planner.speed(parseFloat(speed));
    });

program.parse(process.argv);
