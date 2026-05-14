#include <catch2/catch_test_macros.hpp>

#include "fiasco/http/request.hpp"
#include "fiasco/serialization/model.hpp"

// -- Test model --------------------------------------------------------------

struct test_user {
  std::string username;
  int age;
};

FIASCO_MODEL(test_user, username, age)

struct test_response_model {
  std::string id;
  std::string status;
};

FIASCO_MODEL(test_response_model, id, status)

// -- FIASCO_MODEL tests -----------------------------------------------------

TEST_CASE("FIASCO_MODEL generates to_json correctly", "[serialization]") {
  test_user u{"abdurrahman", 21};
  nlohmann::json j = u;

  REQUIRE(j["username"] == "abdurrahman");
  REQUIRE(j["age"] == 21);
}

TEST_CASE("FIASCO_MODEL generates from_json correctly", "[serialization]") {
  nlohmann::json j = {{"username", "fiasco"}, {"age", 99}};
  auto u = j.get<test_user>();

  REQUIRE(u.username == "fiasco");
  REQUIRE(u.age == 99);
}

TEST_CASE("FIASCO_MODEL round-trips through JSON string", "[serialization]") {
  test_user original{"roundtrip", 42};
  std::string serialized = nlohmann::json(original).dump();
  auto restored = nlohmann::json::parse(serialized).get<test_user>();

  REQUIRE(restored.username == original.username);
  REQUIRE(restored.age == original.age);
}
