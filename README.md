# basalt

A C++ rewrite of [Regolith](https://github.com/Bedrock-OSS/regolith), the add-on compiler for Minecraft Bedrock Edition.

It's a fun side project. The aim is a fast, single-binary tool that can run existing Regolith projects (`config.json`, filters, profiles) without changes.

> Basalt is the solid rock beneath the regolith.

## Status

Early days. Nothing works yet.

## Building

Requires [xmake](https://xmake.io) and a C++23 compiler (GCC 14+, Clang 18+, MSVC 17.10+).

```sh
xmake            # build (deps are fetched automatically)
xmake run basalt run default
xmake test       # run tests
```

## License

MIT
