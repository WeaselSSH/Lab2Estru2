#include <cstdio>
#include <fstream>

#include "catalog.hpp"
#include "catalog_codec.hpp"
#include "doctest/doctest.h"

// Agregue aquí al menos tres casos de prueba propios.
// No defina DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN en este archivo.

TEST_CASE("read_record_at no retorna record cuando offset es inválido") {
  const std::string path = "test_empty.bin";

  // Arrange
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
  }

  std::ifstream input(path, std::ios::binary);

  // Act
  lab2::ReadResult result = lab2::read_record_at(input, 0);

  // Assert
  CHECK(result.status == lab2::ReadStatus::InvalidOffset);
  CHECK_FALSE(result.record.has_value());

  input.close();
  std::remove(path.c_str());
}

TEST_CASE("find_offset retorna nullopt cuando la clave no existe") {
  // Arrange
  std::vector<lab2::PrimaryEntry> index = {
      {"A100", 0},
      {"B200", 100},
      {"C300", 200}};

  // Act
  std::optional<std::uint64_t> result =
      lab2::find_offset(index, "X999");

  // Assert
  CHECK_FALSE(result.has_value());
}

TEST_CASE("build_composer_index descarta una clave que no coincide") {
  const std::string path = "test_mismatch.bin";

  // Arrange
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);

    lab2::Record record;
    record.label_id = "REAL";
    record.composer = "MOZART";
    record.title = "TEST";

    std::uint64_t written_offset = 0;
    std::string error;

    REQUIRE(lab2::write_record(out, record, &written_offset, error));
  }

  std::vector<lab2::PrimaryEntry> primary = {{"FALSA", 0}};

  std::ifstream input(path, std::ios::binary);

  // Act
  lab2::ComposerBuildResult result =
      lab2::build_composer_index(input, primary);

  // Assert
  CHECK(result.entries.empty());

  REQUIRE(result.skipped.size() == 1);

  CHECK(result.skipped[0].status == lab2::ReadStatus::IndexKeyMismatch);

  CHECK(result.skipped[0].index_key == "FALSA");

  input.close();
  std::remove(path.c_str());
}