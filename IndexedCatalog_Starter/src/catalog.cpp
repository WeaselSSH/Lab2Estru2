#include "catalog.hpp"

#include <algorithm>
#include <utility>

#include "binary_io.hpp"
#include "catalog_codec.hpp"
#include "crc32.hpp"

namespace lab2 {

const char* to_string(ReadStatus status) {
  switch (status) {
    case ReadStatus::Ok:
      return "Ok";
    case ReadStatus::NotFound:
      return "NotFound";
    case ReadStatus::IndexKeyMismatch:
      return "IndexKeyMismatch";
    case ReadStatus::InvalidOffset:
      return "InvalidOffset";
    case ReadStatus::TruncatedHeader:
      return "TruncatedHeader";
    case ReadStatus::BadMagic:
      return "BadMagic";
    case ReadStatus::UnsupportedVersion:
      return "UnsupportedVersion";
    case ReadStatus::InvalidLength:
      return "InvalidLength";
    case ReadStatus::TruncatedPayload:
      return "TruncatedPayload";
    case ReadStatus::MissingChecksum:
      return "MissingChecksum";
    case ReadStatus::ChecksumMismatch:
      return "ChecksumMismatch";
    case ReadStatus::MalformedPayload:
      return "MalformedPayload";
  }
  return "UnknownReadStatus";
}

const char* to_string(BuildStatus status) {
  switch (status) {
    case BuildStatus::Ok:
      return "Ok";
    case BuildStatus::ReadError:
      return "ReadError";
    case BuildStatus::DuplicateKey:
      return "DuplicateKey";
  }
  return "UnknownBuildStatus";
}

const char* to_string(VerificationIssueType type) {
  switch (type) {
    case VerificationIssueType::UnsortedIndex:
      return "UnsortedIndex";
    case VerificationIssueType::DuplicateKey:
      return "DuplicateKey";
    case VerificationIssueType::DuplicateOffset:
      return "DuplicateOffset";
    case VerificationIssueType::RecordReadError:
      return "RecordReadError";
    case VerificationIssueType::KeyMismatch:
      return "KeyMismatch";
  }
  return "UnknownVerificationIssue";
}

ReadResult read_record_at(std::istream& input, std::uint64_t offset) {
  ReadResult result;
  result.offset = offset;
  result.next_offset = offset;
  std::optional<std::uint64_t> size = stream_size(input);

  if (offset >= size.value()) {
    result.status = ReadStatus::InvalidOffset;
    result.detail = "Offset se encuentra fuera del archivo.";

    return result;
  }

  if (!seek_absolute(input, offset)) {
    result.status = ReadStatus::InvalidOffset;
    result.detail = "No se pudo mover el cursor al offset indicado.";

    return result;
  }

  std::uint32_t magic = 0;
  std::uint16_t version = 0;
  std::uint32_t payload_length = 0;

  if (!read_u32_le(input, magic) || !read_u16_le(input, version) || !read_u32_le(input, payload_length)) {
    result.status = ReadStatus::TruncatedHeader;
    result.detail = "Header se encuentra truncado.";
    return result;
  }

  if (magic != RECORD_MAGIC) {
    result.status = ReadStatus::BadMagic;
    result.detail = "Magic inválido.";
    return result;
  }

  if (version != RECORD_VERSION) {
    result.status = ReadStatus::UnsupportedVersion;
    result.detail = "Versión diferente a la actual.";
    return result;
  }

  if (payload_length <= 0 || payload_length > MAX_PAYLOAD_SIZE) {
    result.status = ReadStatus::InvalidLength;
    result.detail = "Longitud inválida.";
    return result;
  }

  std::vector<std::byte> payload(payload_length);

  if (!read_exact(input, payload)) {
    result.status = ReadStatus::TruncatedPayload;
    result.detail = "Payload incompleto.";
    return result;
  }

  std::uint32_t crc_stored;

  if (!read_u32_le(input, crc_stored)) {
    result.status = ReadStatus::MissingChecksum;
    result.detail = "Error al leer crc.";
    return result;
  }

  if (crc32(payload) != crc_stored) {
    result.status = ReadStatus::ChecksumMismatch;
    result.detail = "Datos se encuentran corruptos.";
    return result;
  }

  PayloadDecodeResult p = decode_payload(payload);

  result.record = p.record;
  result.detail = p.detail;
  result.next_offset = offset + RECORD_HEADER_SIZE + payload_length + RECORD_CHECKSUM_SIZE;
  result.status = ReadStatus::Ok;

  return result;
}

PrimaryBuildResult build_primary_index(std::istream& input) {
  // TODO 2
  // Recorra el archivo con next_offset, ordene por label_id y detecte duplicados.
  (void)input;
  return {BuildStatus::ReadError, {}, 0, {}, "TODO: implementar build_primary_index"};
}

std::optional<std::uint64_t> find_offset(
    std::span<const PrimaryEntry> index,
    std::string_view label_id) {
  // TODO 3
  // Implemente búsqueda binaria manual. No use std::lower_bound,
  // std::binary_search ni std::equal_range.
  (void)index;
  (void)label_id;
  return std::nullopt;
}

ReadResult find_record(
    std::istream& input,
    std::span<const PrimaryEntry> index,
    std::string_view label_id) {
  const auto offset = find_offset(index, label_id);
  if (!offset.has_value()) {
    return {ReadStatus::NotFound, std::nullopt, 0, 0,
            "La clave no existe en el índice primario."};
  }
  ReadResult result = read_record_at(input, *offset);
  if (result.ok() && result.record->label_id != label_id) {
    result.status = ReadStatus::IndexKeyMismatch;
    result.record.reset();
    result.detail = "La clave del índice no coincide con la clave del registro.";
  }
  return result;
}

ComposerBuildResult build_composer_index(
    std::istream& input,
    std::span<const PrimaryEntry> primary) {
  // TODO 4
  // Recomendación: reúna pares (composer, label_id), ordénelos y agrúpelos.
  // No almacene offsets en este índice secundario.
  (void)input;
  (void)primary;
  return {};
}

std::span<const std::string> find_by_composer(
    const ComposerIndex& index,
    std::string_view composer) {
  // TODO 5
  // El ComposerIndex está ordenado por compositor: use búsqueda binaria.
  (void)index;
  (void)composer;
  return {};
}

VerificationReport verify_primary_index(
    std::istream& input,
    std::span<const PrimaryEntry> index) {
  // TODO 6
  // Haga primero las verificaciones estructurales del índice y luego valide
  // cada referencia con read_record_at. No imprima desde esta función.
  (void)input;
  (void)index;
  return {};
}

std::vector<std::string> intersect_sorted(
    std::span<const std::string> left,
    std::span<const std::string> right) {
  // TODO BONO: dos punteros, O(n + m), sin duplicados.
  (void)left;
  (void)right;
  return {};
}

}  // namespace lab2
