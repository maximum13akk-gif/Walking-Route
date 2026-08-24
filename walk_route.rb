# walk_route.rb
#!/usr/bin/env ruby
require 'json'
require 'net/http'
require 'uri'
require 'optparse'

DATA_FILE = 'walk_route.json'
DEFAULT_SPEED = 5.0
CALORIES_PER_KM = 55

class Waypoint
  attr_accessor :name, :lat, :lng

  def initialize(name, lat, lng)
    @name = name
    @lat = lat
    @lng = lng
  end

  def to_hash
    { name: @name, lat: @lat, lng: @lng }
  end

  def self.from_hash(h)
    new(h['name'], h['lat'], h['lng'])
  end
end

class RoutePlanner
  attr_reader :waypoints, :speed, :weight

  def initialize
    @waypoints = []
    @speed = DEFAULT_SPEED
    @weight = 70
    load
  end

  def load
    if File.exist?(DATA_FILE)
      data = JSON.parse(File.read(DATA_FILE))
      @waypoints = data['waypoints'].map { |h| Waypoint.from_hash(h) }
      @speed = data['speed'] || DEFAULT_SPEED
      @weight = data['weight'] || 70
    end
  end

  def save
    File.write(DATA_FILE, JSON.pretty_generate({
      waypoints: @waypoints.map(&:to_hash),
      speed: @speed,
      weight: @weight
    }))
  end

  def geocode(query)
    url = URI("https://nominatim.openstreetmap.org/search")
    params = { q: query, format: 'json', limit: 1 }
    url.query = URI.encode_www_form(params)
    req = Net::HTTP::Get.new(url)
    req['User-Agent'] = 'WalkRoutePlanner/1.0'
    res = Net::HTTP.start(url.host, url.port, use_ssl: true) { |http| http.request(req) }
    return nil unless res.is_a?(Net::HTTPSuccess)
    data = JSON.parse(res.body)
    return nil if data.empty?
    [data[0]['lat'].to_f, data[0]['lon'].to_f]
  end

  def add(location)
    if location.include?(',')
      parts = location.split(',').map(&:strip)
      if parts.size == 2
        lat = parts[0].to_f
        lng = parts[1].to_f
        if lat != 0.0 && lng != 0.0
          name = "(%.4f,%.4f)" % [lat, lng]
          @waypoints << Waypoint.new(name, lat, lng)
          save
          puts "✅ Added waypoint: #{name}"
          return
        end
      end
    end
    coords = geocode(location)
    unless coords
      puts "❌ Could not geocode '#{location}'"
      return
    end
    lat, lng = coords
    @waypoints << Waypoint.new(location, lat, lng)
    save
    puts "✅ Added waypoint: #{location} (%.4f, %.4f)" % [lat, lng]
  end

  def list
    if @waypoints.empty?
      puts "No waypoints."
      return
    end
    puts "\n📍 Waypoints:"
    @waypoints.each_with_index do |w, i|
      puts "#{i+1}: #{w.name} (%.4f, %.4f)" % [w.lat, w.lng]
    end
  end

  def remove(index)
    if index < 1 || index > @waypoints.size
      puts "❌ Invalid index. Must be between 1 and #{@waypoints.size}"
      return
    end
    removed = @waypoints.delete_at(index-1)
    save
    puts "✅ Removed waypoint: #{removed.name}"
  end

  def clear
    @waypoints = []
    save
    puts "✅ All waypoints cleared."
  end

  def speed(speed)
    @speed = speed
    save
    puts "✅ Walking speed set to #{'%.1f' % speed} km/h"
  end

  def route
    if @waypoints.size < 2
      puts "Need at least 2 waypoints to calculate a route."
      return
    end
    coords = @waypoints.map { |w| "#{w.lng},#{w.lat}" }.join(';')
    url = URI("http://router.project-osrm.org/route/v1/walking/#{coords}?overview=full&geometries=polyline&steps=true")
    res = Net::HTTP.get(url)
    data = JSON.parse(res)
    unless data['routes'] && !data['routes'].empty?
      puts "❌ No route found."
      return
    end
    route = data['routes'][0]
    distance_km = route['distance'] / 1000.0
    duration_min = route['duration'] / 60.0
    elevation = distance_km * 12
    calories = distance_km * CALORIES_PER_KM * (@weight / 70.0)

    puts "\n🚶 Walking Route:"
    puts "Total distance: %.2f km" % distance_km
    puts "Total duration: %.1f min (at %.1f km/h)" % [duration_min, @speed]
    puts "Elevation gain: ~%.0f m" % elevation
    puts "Calories burned: ~%.0f kcal" % calories

    if route['legs']
      puts "\nTurn-by-turn:"
      step_num = 1
      route['legs'].each do |leg|
        leg['steps'].each do |step|
          instruction = step['maneuver']['instruction']
          dist = step['distance'] / 1000.0
          puts "#{step_num}. #{instruction} (%.2f km)" % dist
          step_num += 1
        end
      end
    end
  end
end

options = {}
OptionParser.new do |opts|
  opts.banner = "Usage: walk_route.rb <command> [options]"
  opts.on("add LOCATION", "Add waypoint") { |v| options[:add] = v }
  opts.on("list", "List waypoints") { options[:list] = true }
  opts.on("route", "Calculate route") { options[:route] = true }
  opts.on("remove INDEX", Integer, "Remove waypoint") { |v| options[:remove] = v }
  opts.on("clear", "Clear all") { options[:clear] = true }
  opts.on("speed KMH", Float, "Set walking speed") { |v| options[:speed] = v }
end.parse!

planner = RoutePlanner.new
if options[:add]
  planner.add(options[:add])
elsif options[:list]
  planner.list
elsif options[:route]
  planner.route
elsif options[:remove]
  planner.remove(options[:remove])
elsif options[:clear]
  planner.clear
elsif options[:speed]
  planner.speed(options[:speed])
else
  puts "Usage: walk_route.rb [add|list|route|remove|clear|speed]"
end
