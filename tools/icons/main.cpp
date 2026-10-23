// Generates the basalt logo and app icons.
//
// The logo is hexagonal basalt columns rising out of lava, clipped to a circle.
// Shapes are drawn back to front. Each shape is masked by the silhouettes of
// everything in front of it (stroked a little wider), which leaves transparent
// gaps between shapes so the logo works on light and dark backgrounds.
//
// Run `xmake icons` from the repo root. Writes, relative to the repo root
// (first argument, defaults to the working directory):
//   assets/logo.svg, assets/logo.png (512px), res/basalt.png (256px),
//   res/basalt.ico

#include <lunasvg/lunasvg.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numbers>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr double canvas = 512;
constexpr double gap = 12;     // width of the transparent gaps
constexpr double squash = 0.5; // flattening of hexagon tops (fake perspective)
constexpr double radius = 74;  // column radius
constexpr double lava_y = 410; // height of the lava surface at the centre

struct Palette {
  std::string_view left;
  std::string_view mid;
  std::string_view right;
};

constexpr std::string_view column_top = "#9fb0d6";
constexpr Palette column_faces{
    .left = "#4a5a85", .mid = "#3a486f", .right = "#2b3657"};
constexpr Palette glow_faces{
    .left = "#c4573f", .mid = "#a8463a", .right = "#7e3a3d"};
constexpr std::string_view lava_glow = "#ffc857";
constexpr std::string_view lava_surface = "#f7932f";
constexpr std::string_view lava_deep = "#e14b2b";
constexpr std::string_view lava_deepest = "#c73a24";

struct Point {
  double x;
  double y;
};

// One shape in the stack: its silhouette (used to cut gaps into the shapes
// behind it) and the markup that draws it.
struct Layer {
  std::string silhouette;
  std::string draw;
};

using Hexagon = std::array<Point, 6>;

[[nodiscard]] std::string points(std::span<const Point> pts)
{
  std::string out;
  for (const auto &[x, y] : pts) {
    std::format_to(std::back_inserter(out), "{}{:.1f},{:.1f}",
                   out.empty() ? "" : " ", x, y);
  }
  return out;
}

[[nodiscard]] std::string polygon(std::span<const Point> pts,
                                  std::string_view fill = {})
{
  if (fill.empty()) {
    return std::format(R"(<polygon points="{}"/>)", points(pts));
  }
  return std::format(R"(<polygon points="{}" fill="{}"/>)", points(pts), fill);
}

[[nodiscard]] std::string path(std::string_view d, std::string_view fill = {})
{
  if (fill.empty()) {
    return std::format(R"(<path d="{}"/>)", d);
  }
  return std::format(R"(<path d="{}" fill="{}"/>)", d, fill);
}

[[nodiscard]] Hexagon hexagon(double cx, double cy, double r)
{
  Hexagon out{};
  for (std::size_t i = 0; i < out.size(); ++i) {
    const double angle = static_cast<double>(i) * std::numbers::pi / 3;
    out[i] = {.x = cx + r * std::cos(angle),
              .y = cy + r * squash * std::sin(angle)};
  }
  return out;
}

[[nodiscard]] Layer column_layer(double cx, double top, double r)
{
  const Hexagon t = hexagon(cx, top, r);
  const Hexagon b = hexagon(cx, canvas + 120, r);

  // A solid body underneath hides anti-aliasing seams between the faces.
  std::string draw =
      polygon(std::array{t[3], t[2], t[1], t[0], b[0], b[1], b[2], b[3]},
              column_faces.mid);
  draw += polygon(std::array{t[3], t[2], b[2], b[3]}, "url(#glow-left)");
  draw += polygon(std::array{t[2], t[1], b[1], b[2]}, "url(#glow-mid)");
  draw += polygon(std::array{t[1], t[0], b[0], b[1]}, "url(#glow-right)");
  draw += polygon(t, column_top);

  return {
      .silhouette =
          polygon(std::array{t[3], t[4], t[5], t[0], b[0], b[1], b[2], b[3]}),
      .draw = std::move(draw),
  };
}

// Closed path: a gentle sloped wave at height y, filled past the bottom edge.
[[nodiscard]] std::string wave(double y, double amp, int phase = 0,
                               double slope = -0.09)
{
  constexpr int steps = 4;
  constexpr double width = (canvas + 40) / steps;
  const auto at = [&](double x) {
    return y + slope * (x - 256);
  };

  std::string d = std::format("M-20,{:.1f}", at(-20));
  for (int k = 0; k < steps; ++k) {
    const double x0 = -20 + k * width;
    const double sign = (k + phase) % 2 == 0 ? 1 : -1;
    const double xm = x0 + width / 2;
    const double xe = x0 + width;
    std::format_to(std::back_inserter(d), " Q{:.1f},{:.1f} {:.1f},{:.1f}", xm,
                   at(xm) + sign * amp, xe, at(xe));
  }
  std::format_to(std::back_inserter(d), " L{0:.1f},{1:.1f} L-20,{1:.1f} Z",
                 canvas + 20, canvas + 40);
  return d;
}

[[nodiscard]] std::string gradient(std::string_view name, std::string_view from,
                                   std::string_view to)
{
  return std::format(
      R"(<linearGradient id="glow-{}" gradientUnits="userSpaceOnUse" )"
      R"(x1="0" y1="{:.1f}" x2="0" y2="{:.1f}">)"
      R"(<stop offset="0" stop-color="{}"/>)"
      R"(<stop offset="0.55" stop-color="{}"/>)"
      R"(<stop offset="1" stop-color="{}"/></linearGradient>)",
      name, lava_y - 150, lava_y + 10, from, from, to);
}

// Embers: small square sparks drifting up from the lava (a nod to Minecraft's
// pixels).
[[nodiscard]] Layer embers_layer()
{
  struct Ember {
    double x;
    double y;
    double size;
    std::string_view color;
  };

  constexpr std::array embers{
      Ember{.x = 92, .y = 128, .size = 15, .color = lava_surface},
      Ember{.x = 128, .y = 72, .size = 9, .color = lava_glow},
      Ember{.x = 408, .y = 96, .size = 12, .color = lava_glow},
      Ember{.x = 448, .y = 168, .size = 8, .color = lava_surface},
  };

  Layer layer;
  for (const auto &e : embers) {
    std::format_to(std::back_inserter(layer.draw),
                   R"(<rect x="{0:.1f}" y="{1:.1f}" width="{2:.1f}" )"
                   R"(height="{2:.1f}" fill="{3}" )"
                   R"svg(transform="rotate(45 {4:.1f} {5:.1f})"/>)svg",
                   e.x - e.size / 2, e.y - e.size / 2, e.size, e.color, e.x,
                   e.y);
  }
  return layer;
}

[[nodiscard]] std::vector<Layer> column_layers()
{
  struct Column {
    double x_offset; // in radii from the centre
    int row;         // 0 = back, 1 = front
    double height;   // above the base line
  };

  constexpr std::array columns{
      Column{.x_offset = -1.5, .row = 0, .height = 250},
      Column{.x_offset = 1.5, .row = 0, .height = 280},
      Column{.x_offset = -3, .row = 1, .height = 150},
      Column{.x_offset = 0, .row = 1, .height = 345},
      Column{.x_offset = 3, .row = 1, .height = 175},
  };
  constexpr double row_depth = radius * squash * std::numbers::sqrt3 / 2;
  constexpr double base = 470;

  std::vector<Layer> layers;
  layers.reserve(columns.size());
  for (const auto &c : columns) {
    layers.push_back(column_layer(256 + c.x_offset * radius,
                                  base - c.height + c.row * row_depth,
                                  radius - 3));
  }
  return layers;
}

[[nodiscard]] Layer lava_layer()
{
  const std::string surface = wave(lava_y, 9);
  return {
      .silhouette = path(surface),
      .draw = path(surface, lava_surface) +
              path(wave(lava_y + 34, 9, 1), lava_deep) +
              path(wave(lava_y + 92, 9), lava_deepest),
  };
}

[[nodiscard]] std::string build_svg()
{
  std::vector<Layer> layers;
  layers.push_back(embers_layer());
  std::ranges::move(column_layers(), std::back_inserter(layers));
  layers.push_back(lava_layer());

  std::string defs = gradient("left", column_faces.left, glow_faces.left);
  defs += gradient("mid", column_faces.mid, glow_faces.mid);
  defs += gradient("right", column_faces.right, glow_faces.right);

  std::string body;
  for (std::size_t k = 0; k < layers.size(); ++k) {
    const auto in_front = layers | std::views::drop(k + 1);
    if (in_front.empty()) {
      body += layers[k].draw;
      continue;
    }

    std::string cut =
        std::format(R"(<g fill="#000" stroke="#000" stroke-width="{}" )"
                    R"(stroke-linejoin="round">)",
                    gap);
    for (const auto &front : in_front) {
      cut += front.silhouette;
    }
    cut += "</g>";

    std::format_to(std::back_inserter(defs),
                   R"(<mask id="m{0}" maskUnits="userSpaceOnUse" x="0" y="0" )"
                   R"(width="{1}" height="{1}">)"
                   R"(<rect width="{1}" height="{1}" fill="#fff"/>{2}</mask>)",
                   k, canvas, cut);
    std::format_to(std::back_inserter(body),
                   R"svg(<g mask="url(#m{})">{}</g>)svg", k, layers[k].draw);
  }

  return std::format(
      R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {0} {0}" )"
      R"(width="{0}" height="{0}">)"
      "\n<defs>\n"
      R"(<clipPath id="badge"><circle cx="{1}" cy="{1}" r="{2}"/></clipPath>)"
      "\n{3}\n</defs>\n"
      R"svg(<g clip-path="url(#badge)">)svg"
      "\n{4}\n</g>\n</svg>\n",
      canvas, canvas / 2, canvas / 2 - 4, defs, body);
}

[[nodiscard]] std::vector<std::byte> render_png(const lunasvg::Document &doc,
                                                int px)
{
  std::vector<std::byte> png;
  const lunasvg::Bitmap bitmap = doc.renderToBitmap(px, px);
  bitmap.writeToPng(
      [](void *closure, void *data, int len) {
        const std::span bytes{static_cast<const std::byte *>(data),
                              static_cast<std::size_t>(len)};
        auto &out = *static_cast<std::vector<std::byte> *>(closure);
        std::ranges::copy(bytes, std::back_inserter(out));
      },
      &png);
  return png;
}

// An .ico file is a small directory header followed by one PNG per size.
[[nodiscard]] std::vector<std::byte> build_ico(const lunasvg::Document &doc)
{
  constexpr std::array sizes{16U, 24U, 32U, 48U, 64U, 128U, 256U};

  std::vector<std::byte> out;
  const auto u8 = [&](std::size_t v) {
    out.push_back(static_cast<std::byte>(v & 0xffU));
  };
  const auto u16 = [&](std::size_t v) {
    u8(v);
    u8(v >> 8U);
  };
  const auto u32 = [&](std::size_t v) {
    u16(v);
    u16(v >> 16U);
  };

  std::vector<std::vector<std::byte>> images;
  images.reserve(sizes.size());
  for (const unsigned px : sizes) {
    images.push_back(render_png(doc, static_cast<int>(px)));
  }

  u16(0);            // reserved
  u16(1);            // type: icon
  u16(sizes.size()); // image count
  std::size_t offset = 6 + (16 * sizes.size());
  for (std::size_t i = 0; i < sizes.size(); ++i) {
    const unsigned dim = sizes.at(i) >= 256 ? 0 : sizes.at(i);
    u8(dim); // width, 0 means 256
    u8(dim); // height
    u8(0);   // palette size
    u8(0);   // reserved
    u16(1);  // colour planes
    u16(32); // bits per pixel
    u32(images[i].size());
    u32(offset);
    offset += images[i].size();
  }
  for (const auto &image : images) {
    std::ranges::copy(image, std::back_inserter(out));
  }
  return out;
}

void write_file(const fs::path &file, std::span<const std::byte> data)
{
  std::ofstream out(file, std::ios::binary);
  out.write(reinterpret_cast<const char *>(data.data()),
            static_cast<std::streamsize>(data.size()));
  if (!out) {
    throw std::runtime_error(std::format("could not write {}", file.string()));
  }
  std::cout << std::format("wrote {}\n", file.string());
}

} // namespace

int main(int argc, char **argv)
{
  try {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    if (args.size() > 1) {
      fs::current_path(args[1]);
    }

    const std::string svg = build_svg();
    const auto doc = lunasvg::Document::loadFromData(svg);
    if (!doc) {
      std::cerr << "error: generated svg failed to parse\n";
      return 1;
    }

    write_file("assets/logo.svg", std::as_bytes(std::span{svg}));
    write_file("assets/logo.png", render_png(*doc, 512));
    write_file("res/basalt.png", render_png(*doc, 256));
    write_file("res/basalt.ico", build_ico(*doc));
  } catch (const std::exception &e) {
    std::cerr << std::format("error: {}\n", e.what());
    return 1;
  }
  return 0;
}
