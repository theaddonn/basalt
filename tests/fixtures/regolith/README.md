# Regolith test fixtures

The `config.json` files here are copied unmodified from the test projects of
[Regolith](https://github.com/Bedrock-OSS/regolith)
(`test/testdata/**/config.json`), keeping their relative directory layout. Only
the config files are included, not the projects around them. They are used by
`tests/config/fixtures_test.cpp` to check that basalt accepts everything
Regolith's own tests accept.

Regolith is Copyright (c) 2021 Bedrock-OSS and is released under the MIT
License: <https://github.com/Bedrock-OSS/regolith/blob/main/LICENSE>.

`profile_filter/project/config.json` intentionally contains circular profile
references; Regolith rejects it, and so does basalt.

`double_remote_project_installed/config.json` has a remote filter definition
without `version`, which Regolith also rejects; the test expects that error.
