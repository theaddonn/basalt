// Generates the basalt logo and app icons.
//
// The logo is hexagonal basalt columns rising out of lava, clipped to a circle.
// Shapes are drawn back to front. Each shape is masked by the silhouettes of
// everything in front of it (stroked a little wider), which leaves transparent
// gaps between shapes so the logo works on light and dark backgrounds.
//
// Run with `xmake icons` from the repo root. Writes, relative to the repo root
// (passed as the first argument, defaults to the working directory):
//   assets/logo.svg, assets/logo.png (512px), res/basalt.png (256px), res/basalt.ico

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

#include <lunasvg/lunasvg.h>

namespace {

constexpr double size = 512;
constexpr double gap = 12;     // width of the transparent gaps
constexpr double squash = 0.5; // flattening of the hexagon tops (fake perspective)
constexpr double radius = 74;  // column radius
constexpr double lava_y = 410; // height of the lava surface at the centre

struct Palette {
    const char* left;
    const char* mid;
    const char* right;
};

constexpr const char* column_top = "#9fb0d6";
constexpr Palette column = {"#4a5a85", "#3a486f", "#2b3657"};
constexpr Palette glow = {"#c4573f", "#a8463a", "#7e3a3d"};
constexpr const char* lava_glow = "#ffc857";
constexpr const char* lava_surface = "#f7932f";
constexpr const char* lava_deep = "#e14b2b";
constexpr const char* lava_deepest = "#c73a24";

struct Point {
    double x;
    double y;
};

// One shape in the stack: its silhouette (used to cut gaps into shapes behind
// it) and the markup that draws it.
struct Layer {
    std::string silhouette;
    std::string draw;
};

std::string num(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", v);
    return buf;
}

std::string points(const std::vector<Point>& pts) {
    std::string out;
    for (const auto& p : pts) {
        if (!out.empty()) {
            out += ' ';
        }
        out += num(p.x) + ',' + num(p.y);
    }
    return out;
}

std::string polygon(const std::vector<Point>& pts, const std::string& fill = "") {
    std::string out = "<polygon points=\"" + points(pts) + '"';
    if (!fill.empty()) {
        out += " fill=\"" + fill + '"';
    }
    return out + "/>";
}

std::array<Point, 6> hexagon(double cx, double cy, double r) {
    std::array<Point, 6> out{};
    for (int i = 0; i < 6; ++i) {
        double a = i * std::numbers::pi / 3;
        out[i] = {cx + r * std::cos(a), cy + r * squash * std::sin(a)};
    }
    return out;
}

Layer column_layer(double cx, double top, double r) {
    auto t = hexagon(cx, top, r);
    auto b = hexagon(cx, size + 120, r);

    // A solid body underneath hides anti-aliasing seams between the faces.
    Layer layer;
    layer.draw = polygon({t[3], t[2], t[1], t[0], b[0], b[1], b[2], b[3]}, column.mid);
    layer.draw += polygon({t[3], t[2], b[2], b[3]}, "url(#glow-left)");
    layer.draw += polygon({t[2], t[1], b[1], b[2]}, "url(#glow-mid)");
    layer.draw += polygon({t[1], t[0], b[0], b[1]}, "url(#glow-right)");
    layer.draw += polygon({t.begin(), t.end()}, column_top);
    layer.silhouette = polygon({t[3], t[4], t[5], t[0], b[0], b[1], b[2], b[3]});
    return layer;
}

// Closed path: a gentle sloped wave at height y, filled down past the bottom edge.
std::string wave(double y, double amp, int phase = 0, double slope = -0.09) {
    constexpr int steps = 4;
    constexpr double w = (size + 40) / steps;
    std::string d = "M-20," + num(y + slope * (-20 - 256));
    for (int k = 0; k < steps; ++k) {
        double x0 = -20 + k * w;
        double sign = (k + phase) % 2 == 0 ? 1 : -1;
        double ym = y + slope * (x0 + w / 2 - 256);
        double ye = y + slope * (x0 + w - 256);
        d +=
            " Q" + num(x0 + w / 2) + ',' + num(ym + sign * amp) + ' ' + num(x0 + w) + ',' + num(ye);
    }
    return d + " L" + num(size + 20) + ',' + num(size + 40) + " L-20," + num(size + 40) + " Z";
}

std::string path(const std::string& d, const std::string& fill = "") {
    std::string out = "<path d=\"" + d + '"';
    if (!fill.empty()) {
        out += " fill=\"" + fill + '"';
    }
    return out + "/>";
}

std::string gradient(const char* name, const char* from, const char* to) {
    return std::string("<linearGradient id=\"glow-") + name +
           "\" gradientUnits=\"userSpaceOnUse\" x1=\"0\" y1=\"" + num(lava_y - 150) +
           "\" x2=\"0\" y2=\"" + num(lava_y + 10) + "\"><stop offset=\"0\" stop-color=\"" + from +
           "\"/><stop offset=\"0.55\" stop-color=\"" + from +
           "\"/><stop offset=\"1\" stop-color=\"" + to + "\"/></linearGradient>";
}

std::string build_svg() {
    std::vector<Layer> layers;

    // Embers: small square sparks drifting up from the lava (a nod to Minecraft pixels).
    struct Ember {
        double x, y, s;
        const char* color;
    };
    constexpr std::array<Ember, 4> embers = {{
        {92, 128, 15, lava_surface},
        {128, 72, 9, lava_glow},
        {408, 96, 12, lava_glow},
        {448, 168, 8, lava_surface},
    }};
    Layer sparks;
    for (const auto& e : embers) {
        sparks.draw += "<rect x=\"" + num(e.x - e.s / 2) + "\" y=\"" + num(e.y - e.s / 2) +
                       "\" width=\"" + num(e.s) + "\" height=\"" + num(e.s) + "\" fill=\"" +
                       e.color + "\" transform=\"rotate(45 " + num(e.x) + ' ' + num(e.y) + ")\"/>";
    }
    layers.push_back(sparks);

    // Columns: back row first. x offset in radii, row, height above the base line.
    struct Column {
        double x_offset;
        int row;
        double height;
    };
    constexpr std::array<Column, 5> columns = {{
        {-1.5, 0, 250},
        {1.5, 0, 280},
        {-3, 1, 150},
        {0, 1, 345},
        {3, 1, 175},
    }};
    const double row_depth = radius * squash * std::sqrt(3.0) / 2;
    constexpr double base = 470;
    for (const auto& c : columns) {
        layers.push_back(column_layer(256 + c.x_offset * radius,
                                      base - c.height + c.row * row_depth, radius - 3));
    }

    layers.push_back({path(wave(lava_y, 9)), path(wave(lava_y, 9), lava_surface) +
                                                 path(wave(lava_y + 34, 9, 1), lava_deep) +
                                                 path(wave(lava_y + 92, 9), lava_deepest)});

    std::string defs = gradient("left", column.left, glow.left) + '\n' +
                       gradient("mid", column.mid, glow.mid) + '\n' +
                       gradient("right", column.right, glow.right) + '\n';
    std::string body;
    for (std::size_t k = 0; k < layers.size(); ++k) {
        if (k + 1 == layers.size()) {
            body += layers[k].draw + '\n';
            continue;
        }
        std::string cut = "<g fill=\"#000\" stroke=\"#000\" stroke-width=\"" + num(gap) +
                          "\" stroke-linejoin=\"round\">";
        for (std::size_t f = k + 1; f < layers.size(); ++f) {
            cut += layers[f].silhouette;
        }
        cut += "</g>";
        auto id = "m" + std::to_string(k);
        defs += "<mask id=\"" + id + "\" maskUnits=\"userSpaceOnUse\" x=\"0\" y=\"0\" width=\"" +
                num(size) + "\" height=\"" + num(size) + "\"><rect width=\"" + num(size) +
                "\" height=\"" + num(size) + "\" fill=\"#fff\"/>" + cut + "</mask>\n";
        body += "<g mask=\"url(#" + id + ")\">" + layers[k].draw + "</g>\n";
    }

    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " + num(size) + ' ' +
           num(size) + "\" width=\"" + num(size) + "\" height=\"" + num(size) + "\">\n<defs>\n" +
           "<clipPath id=\"badge\"><circle cx=\"" + num(size / 2) + "\" cy=\"" + num(size / 2) +
           "\" r=\"" + num(size / 2 - 4) + "\"/></clipPath>\n" + defs +
           "</defs>\n<g clip-path=\"url(#badge)\">\n" + body + "</g>\n</svg>\n";
}

std::vector<std::uint8_t> render_png(const lunasvg::Document& doc, int px) {
    std::vector<std::uint8_t> png;
    auto bitmap = doc.renderToBitmap(px, px);
    bitmap.writeToPng(
        [](void* closure, void* data, int len) {
            auto* out = static_cast<std::vector<std::uint8_t>*>(closure);
            auto* bytes = static_cast<std::uint8_t*>(data);
            out->insert(out->end(), bytes, bytes + len);
        },
        &png);
    return png;
}

bool write_file(const std::string& name, const void* data, std::size_t len) {
    std::ofstream file(name, std::ios::binary);
    file.write(static_cast<const char*>(data), static_cast<std::streamsize>(len));
    if (!file) {
        std::cerr << "error: could not write " << name << '\n';
        return false;
    }
    std::cout << "wrote " << name << '\n';
    return true;
}

// An .ico file is a small directory header followed by one PNG per size.
std::vector<std::uint8_t> build_ico(const lunasvg::Document& doc) {
    constexpr std::array<int, 7> sizes = {16, 24, 32, 48, 64, 128, 256};
    std::vector<std::vector<std::uint8_t>> images;
    for (int s : sizes) {
        images.push_back(render_png(doc, s));
    }

    std::vector<std::uint8_t> out;
    auto u16 = [&](std::uint16_t v) {
        out.push_back(v & 0xff);
        out.push_back(v >> 8);
    };
    auto u32 = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            out.push_back((v >> (8 * i)) & 0xff);
        }
    };

    u16(0);            // reserved
    u16(1);            // type: icon
    u16(sizes.size()); // image count
    std::uint32_t offset = 6 + 16 * sizes.size();
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        out.push_back(sizes[i] >= 256 ? 0 : sizes[i]); // width, 0 means 256
        out.push_back(sizes[i] >= 256 ? 0 : sizes[i]); // height
        out.push_back(0);                              // palette size
        out.push_back(0);                              // reserved
        u16(1);                                        // colour planes
        u16(32);                                       // bits per pixel
        u32(images[i].size());
        u32(offset);
        offset += images[i].size();
    }
    for (const auto& img : images) {
        out.insert(out.end(), img.begin(), img.end());
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        std::filesystem::current_path(argv[1]);
    }

    auto svg = build_svg();
    auto doc = lunasvg::Document::loadFromData(svg);
    if (!doc) {
        std::cerr << "error: generated svg failed to parse\n";
        return 1;
    }

    auto logo = render_png(*doc, 512);
    auto app = render_png(*doc, 256);
    auto ico = build_ico(*doc);

    bool ok = write_file("assets/logo.svg", svg.data(), svg.size()) &&
              write_file("assets/logo.png", logo.data(), logo.size()) &&
              write_file("res/basalt.png", app.data(), app.size()) &&
              write_file("res/basalt.ico", ico.data(), ico.size());
    return ok ? 0 : 1;
}
