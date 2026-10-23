<img src="assets/logo.svg" alt="basalt logo" width="160" align="right">

# basalt

A C++ rewrite of [Regolith](https://github.com/Bedrock-OSS/regolith), the Add-On compiler for Minecraft Bedrock Edition.

It's a fun side project. The aim is a fast, single-binary tool that can run existing Regolith projects (`config.json`, filters, profiles) without changes.

> Basalt is the solid rock beneath the regolith.

## Status

Early days. Nothing works yet.

## Building

Requires [xmake](https://xmake.io) and a C++20 compiler (GCC 11+, Clang 14+, MSVC 17.0+).

```sh
xmake            # build (deps are fetched automatically)
xmake run basalt run default
xmake test       # run tests
```

## License

MIT
