#include "basalt/config_error.hpp"

#include <catch2/catch_test_macros.hpp>

using basalt::ConfigError;

TEST_CASE("ConfigError::to_string joins the path with arrows", "[config_error]")
{
    SECTION("no path is just the message")
    {
        CHECK(ConfigError{.path = {}, .message = "boom"}.to_string() == "boom");
    }
    SECTION("one segment")
    {
        CHECK(ConfigError{.path = {"name"}, .message = "expected string"}
                  .to_string() == "name: expected string");
    }
    SECTION("many segments, including array indices")
    {
        const ConfigError error{
            .path = {"regolith", "profiles", "default", "filters", "1"},
            .message = "missing required property \"filter\""};
        CHECK(error.to_string() ==
              "regolith -> profiles -> default -> filters -> 1: "
              "missing required property \"filter\"");
    }
}
