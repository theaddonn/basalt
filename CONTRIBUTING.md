# Contributing

## Building

```sh
xmake          # build (debug by default, deps fetched automatically)
xmake test     # run tests
xmake f -m release && xmake   # release build
```

Needs xmake and a C++23 compiler (GCC 14+, Clang 18+, MSVC 17.10+).

## Code style

- Format with `clang-format` (config in `.clang-format`) before committing.
- Headers (`.hpp`) live next to their sources (`.cpp`) under `src/`.
- Return errors with `tl::expected` rather than throwing.
- Source files are UTF-8, and so are strings at runtime.

## Commits

This repo uses [Conventional Commits](https://www.conventionalcommits.org):

```
<type>(<optional scope>): <short summary>
```

| Type       | Use for                                      |
|------------|----------------------------------------------|
| `feat`     | a new feature                                |
| `fix`      | a bug fix                                    |
| `refactor` | code change that is neither feat nor fix     |
| `perf`     | performance improvement                      |
| `test`     | adding or fixing tests                       |
| `docs`     | documentation only                           |
| `build`    | xmake, dependencies                          |
| `ci`       | GitHub Actions                               |
| `chore`    | anything else (tooling, formatting, etc.)    |

Examples:

```
feat(config): parse filter definitions
fix(cli): handle missing config.json gracefully
build: add reproc dependency
```

Breaking changes get a `!` after the type (`feat!: ...`) or a `BREAKING CHANGE:` footer.
