#pragma once
#include "../brown/transport.hpp"
#include "svg.hpp"
#include <limits>
#include <queue>

namespace museum::black {
namespace json = museum::brown::json;
class Router {
    struct Edge {
        std::size_t to;
        double time;
        char kind;
        std::string label;
    };
    std::map<std::string, std::size_t> stop_ids_;
    std::vector<std::vector<Edge>> graph_;

  public:
    Router(const museum::brown::Transport &catalog, double wait, double velocity) {
        if (!std::isfinite(wait) || !std::isfinite(velocity) || wait < 0 || velocity <= 0)
            throw std::invalid_argument("routing settings");
        for (const auto &[name, unused] : catalog.Stops()) {
            (void)unused;
            stop_ids_[name] = graph_.size();
            graph_.emplace_back();
        }
        auto direction = [&](const std::string &bus, const std::vector<std::string> &stops) {
            const auto base = graph_.size();
            graph_.resize(base + stops.size());
            for (std::size_t i = 0; i < stops.size(); ++i) {
                const auto ground = stop_ids_.at(stops[i]);
                graph_[ground].push_back({base + i, wait, 'W', stops[i]});
                graph_[base + i].push_back({ground, 0, 'X', {}});
                if (i + 1 < stops.size()) {
                    const auto meters = catalog.RoadDistance(stops[i], stops[i + 1]);
                    if (meters < 0)
                        throw std::invalid_argument("negative road distance");
                    graph_[base + i].push_back({base + i + 1, meters * 0.06 / velocity, 'B', bus});
                }
            }
        };
        for (const auto &[name, bus] : catalog.Buses()) {
            direction(name, bus.stops);
            if (!bus.is_roundtrip) {
                auto reverse = bus.stops;
                std::reverse(reverse.begin(), reverse.end());
                direction(name, reverse);
            }
        }
    }
    json::Node Route(const std::string &from, const std::string &to) const {
        if (!stop_ids_.count(from) || !stop_ids_.count(to))
            return json::Object{{"error_message", "not found"}};
        const auto source = stop_ids_.at(from), target = stop_ids_.at(to), absent = graph_.size();
        std::vector<double> distance(graph_.size(), std::numeric_limits<double>::infinity());
        std::vector<std::pair<std::size_t, std::size_t>> previous(graph_.size(), {absent, 0});
        using Entry = std::pair<double, std::size_t>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
        queue.push({0, source});
        distance[source] = 0;
        while (!queue.empty()) {
            auto [time, v] = queue.top();
            queue.pop();
            if (time != distance[v])
                continue;
            if (v == target)
                break;
            for (std::size_t i = 0; i < graph_[v].size(); ++i) {
                const auto &e = graph_[v][i];
                const auto next = time + e.time;
                if (next < distance[e.to]) {
                    distance[e.to] = next;
                    previous[e.to] = {v, i};
                    queue.push({next, e.to});
                }
            }
        }
        if (!std::isfinite(distance[target]))
            return json::Object{{"error_message", "not found"}};
        std::vector<const Edge *> path;
        for (auto v = target; v != source;) {
            auto [parent, index] = previous[v];
            if (parent == absent)
                throw std::logic_error("broken predecessor");
            path.push_back(&graph_[parent][index]);
            v = parent;
        }
        std::reverse(path.begin(), path.end());
        json::Array items;
        for (const auto *e : path) {
            if (e->kind == 'W')
                items.emplace_back(
                    json::Object{{"type", "Wait"}, {"stop_name", e->label}, {"time", e->time}});
            else if (e->kind == 'B') {
                if (!items.empty() && items.back().At("type").AsString() == "Bus" &&
                    items.back().At("bus").AsString() == e->label) {
                    auto &last = items.back().AsObject();
                    last["span_count"] = last["span_count"].AsNumber() + 1;
                    last["time"] = last["time"].AsNumber() + e->time;
                } else
                    items.emplace_back(json::Object{
                        {"type", "Bus"}, {"bus", e->label}, {"span_count", 1}, {"time", e->time}});
            }
        }
        return json::Object{{"total_time", distance[target]}, {"items", std::move(items)}};
    }
};

inline Svg::Color ReadColor(const json::Node &n) {
    if (n.IsString())
        return n.AsString();
    const auto &a = n.AsArray();
    if (a.size() != 3 && a.size() != 4)
        throw std::invalid_argument("color size");
    int rgb[3];
    for (int i = 0; i < 3; ++i) {
        auto x = a[i].AsNumber();
        if (x < 0 || x > 255 || x != std::floor(x))
            throw std::invalid_argument("color component");
        rgb[i] = static_cast<int>(x);
    }
    if (a.size() == 3)
        return Svg::Rgb{rgb[0], rgb[1], rgb[2]};
    auto alpha = a[3].AsNumber();
    if (alpha < 0 || alpha > 1)
        throw std::invalid_argument("color alpha");
    return Svg::Rgba{rgb[0], rgb[1], rgb[2], alpha};
}
inline std::map<std::string, Svg::Point> ProjectStops(const museum::brown::Transport &catalog,
                                                      double width, double height, double padding,
                                                      char mode) {
    if (width <= 0 || height <= 0 || padding < 0 || padding * 2 >= std::min(width, height))
        throw std::invalid_argument("map dimensions");
    std::map<std::string, Svg::Point> result;
    if (catalog.Stops().empty())
        return result;
    std::set<std::pair<std::string, std::string>> neighbors;
    for (const auto &[name, bus] : catalog.Buses()) {
        (void)name;
        for (std::size_t i = 1; i < bus.stops.size(); ++i) {
            neighbors.insert({bus.stops[i - 1], bus.stops[i]});
            neighbors.insert({bus.stops[i], bus.stops[i - 1]});
        }
    }
    std::vector<std::pair<double, std::string>> x, y;
    for (const auto &[name, s] : catalog.Stops()) {
        x.push_back({s.longitude, name});
        y.push_back({s.latitude, name});
    }
    std::sort(x.begin(), x.end());
    std::sort(y.begin(), y.end());
    if (mode == 'G') {
        auto dx = x.back().first - x.front().first, dy = y.back().first - y.front().first;
        double zoom = 0;
        if (dx > 0 && dy > 0)
            zoom = std::min((width - 2 * padding) / dx, (height - 2 * padding) / dy);
        else if (dx > 0)
            zoom = (width - 2 * padding) / dx;
        else if (dy > 0)
            zoom = (height - 2 * padding) / dy;
        for (const auto &[name, s] : catalog.Stops())
            result[name] = {(s.longitude - x.front().first) * zoom + padding,
                            (y.back().first - s.latitude) * zoom + padding};
    } else {
        if (mode != 'J' && mode != 'K')
            throw std::invalid_argument("projection mode");
        auto ranks = [&](const auto &axis) {
            std::map<std::string, int> rank;
            std::vector<std::string> group;
            int index = 0;
            for (std::size_t i = 0; i < axis.size(); ++i) {
                bool advance = mode == 'J' && i && axis[i].first != axis[i - 1].first;
                if (mode == 'K')
                    for (const auto &p : group)
                        if (neighbors.count({p, axis[i].second})) {
                            advance = true;
                            break;
                        }
                if (advance) {
                    ++index;
                    group.clear();
                }
                group.push_back(axis[i].second);
                rank[axis[i].second] = index;
            }
            return std::make_pair(rank, index);
        };
        auto [rx, nx] = ranks(x);
        auto [ry, ny] = ranks(y);
        for (const auto &[name, s] : catalog.Stops()) {
            (void)s;
            result[name] = {padding + (nx ? (width - 2 * padding) * rx.at(name) / nx : 0),
                            height - padding -
                                (ny ? (height - 2 * padding) * ry.at(name) / ny : 0)};
        }
    }
    return result;
}
inline std::string RenderMap(const museum::brown::Transport &catalog, const json::Node &settings,
                             char mode = 'G') {
    const auto number = [&](const std::string &name) { return settings.At(name).AsNumber(); };
    const auto point = [&](const std::string &name) {
        const auto &a = settings.At(name).AsArray();
        if (a.size() != 2)
            throw std::invalid_argument("offset size");
        return Svg::Point{a[0].AsNumber(), a[1].AsNumber()};
    };
    const auto font = [&](const std::string &name) {
        auto n = number(name);
        if (n < 1 || n > 100000 || n != std::floor(n))
            throw std::invalid_argument("font size");
        return static_cast<std::uint32_t>(n);
    };
    auto positions = ProjectStops(catalog, number("width"), number("height"), number("padding"),
                                  mode == 'H' || mode == 'I' ? 'G' : mode);
    std::vector<Svg::Color> palette;
    for (const auto &c : settings.At("color_palette").AsArray())
        palette.push_back(ReadColor(c));
    if (palette.empty())
        throw std::invalid_argument("empty palette");
    std::map<std::string, Svg::Color> colors;
    std::size_t index = 0;
    for (const auto &[name, b] : catalog.Buses()) {
        (void)b;
        colors[name] = palette[index++ % palette.size()];
    }
    json::Array layers = settings.Contains("layers") ? settings.At("layers").AsArray()
                         : mode == 'G'
                             ? json::Array{"bus_lines", "stop_points", "stop_labels"}
                             : json::Array{"bus_lines", "bus_labels", "stop_points", "stop_labels"};
    Svg::Document document;
    document.SetSize(number("width"), number("height"));
    const auto label = [&](const std::string &text, Svg::Point p, bool bus,
                           const Svg::Color &color) {
        Svg::Text base;
        base.SetPoint(p)
            .SetOffset(point(bus ? "bus_label_offset" : "stop_label_offset"))
            .SetFontSize(font(bus ? "bus_label_font_size" : "stop_label_font_size"))
            .SetFontFamily("Verdana")
            .SetData(text);
        if (bus)
            base.SetFontWeight("bold");
        auto under = base;
        auto shade = ReadColor(settings.At("underlayer_color"));
        under.SetFillColor(shade)
            .SetStrokeColor(shade)
            .SetStrokeWidth(number("underlayer_width"))
            .SetStrokeLineCap("round")
            .SetStrokeLineJoin("round");
        document.Add(under);
        base.SetFillColor(color);
        document.Add(base);
    };
    for (const auto &value : layers) {
        const auto &layer = value.AsString();
        if (layer == "bus_lines")
            for (const auto &[name, bus] : catalog.Buses()) {
                Svg::Polyline line;
                line.SetStrokeColor(colors.at(name))
                    .SetStrokeWidth(number("line_width"))
                    .SetStrokeLineCap("round")
                    .SetStrokeLineJoin("round");
                for (const auto &stop : museum::brown::Transport::Traversal(bus))
                    line.AddPoint(positions.at(stop));
                document.Add(line);
            }
        else if (layer == "bus_labels")
            for (const auto &[name, bus] : catalog.Buses()) {
                if (bus.stops.empty())
                    continue;
                label(name, positions.at(bus.stops.front()), true, colors.at(name));
                if (!bus.is_roundtrip && bus.stops.front() != bus.stops.back())
                    label(name, positions.at(bus.stops.back()), true, colors.at(name));
            }
        else if (layer == "stop_points")
            for (const auto &[name, p] : positions) {
                (void)name;
                document.Add(Svg::Circle{}
                                 .SetCenter(p)
                                 .SetRadius(number("stop_radius"))
                                 .SetFillColor("white"));
            }
        else if (layer == "stop_labels")
            for (const auto &[name, p] : positions)
                label(name, p, false, "black");
        else
            throw std::invalid_argument("map layer");
    }
    std::ostringstream out;
    document.Render(out);
    return out.str();
}
inline json::Node TransportAnswers(const json::Node &input, char projection = 'G') {
    const auto catalog = museum::brown::Transport::FromJson(input);
    std::optional<Router> router;
    if (input.Contains("routing_settings")) {
        const auto &s = input.At("routing_settings");
        router.emplace(catalog, s.At("bus_wait_time").AsNumber(), s.At("bus_velocity").AsNumber());
    }
    json::Array responses;
    for (const auto &r : input.At("stat_requests").AsArray()) {
        const auto &type = r.At("type").AsString();
        json::Node answer;
        if (type == "Route") {
            if (!router)
                throw std::invalid_argument("routing settings required");
            answer = router->Route(r.At("from").AsString(), r.At("to").AsString());
            answer.AsObject()["request_id"] = r.At("id");
        } else if (type == "Map")
            answer =
                json::Object{{"request_id", r.At("id")},
                             {"map", RenderMap(catalog, input.At("render_settings"), projection)}};
        else
            answer = catalog.Answer(r);
        responses.push_back(std::move(answer));
    }
    return responses;
}
} // namespace museum::black
