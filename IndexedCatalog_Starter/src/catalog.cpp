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

  if (!p.record.has_value()) {
    result.status = ReadStatus::MalformedPayload;
    result.detail = p.detail;
    return result;
  }

  result.record = p.record;
  result.detail = p.detail;
  result.next_offset = offset + RECORD_HEADER_SIZE + payload_length + RECORD_CHECKSUM_SIZE;
  result.status = ReadStatus::Ok;

  return result;
}

PrimaryBuildResult build_primary_index(std::istream& input) {
  PrimaryBuildResult primary_result;
  std::uint64_t offset = 0;
  std::optional<std::uint64_t> size = stream_size(input);

  while (offset < size.value()) {
    ReadResult result = read_record_at(input, offset);

    if (!result.ok()) {
      primary_result.status = BuildStatus::ReadError;
      primary_result.detail = result.detail;
      primary_result.error_offset = offset;

      return primary_result;
    }

    primary_result.entries.push_back({result.record->label_id, offset});

    offset = result.next_offset;
  }

  std::sort(primary_result.entries.begin(), primary_result.entries.end(),
            [](const PrimaryEntry& a, const PrimaryEntry& b) { return a.label_id < b.label_id; });

  for (std::size_t i = 1; i < primary_result.entries.size(); i++) {
    if (primary_result.entries[i].label_id == primary_result.entries[i - 1].label_id) {
      primary_result.status = BuildStatus::DuplicateKey;
      primary_result.error_key = primary_result.entries[i].label_id;
      primary_result.detail = "llave duplicada.";

      return primary_result;
    }
  }

  primary_result.status = BuildStatus::Ok;
  return primary_result;
}

std::optional<std::uint64_t> find_offset(std::span<const PrimaryEntry> index, std::string_view label_id) {
  int hi = index.size();
  int lo = 0;

  while (lo < hi) {
    int cur = (hi + lo) / 2;
    if (index[cur].label_id == label_id) {
      return index[cur].offset;
    } else if (index[cur].label_id < label_id) {
      lo = cur + 1;
    } else {
      hi = cur;
    }
  }

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
  ComposerBuildResult result;

  std::vector<std::pair<std::string, std::string>> pairs;

  for (const PrimaryEntry& entry : primary) {
    ReadResult read = read_record_at(input, entry.offset);

    if (!read.ok()) {
      SkippedRecord s;
      s.index_key = entry.label_id;
      s.offset = entry.offset;
      s.status = read.status;

      result.skipped.push_back(s);
      continue;
    }

    if (entry.label_id != read.record->label_id) {
      SkippedRecord s;
      s.index_key = entry.label_id;
      s.offset = entry.offset;
      s.status = ReadStatus::IndexKeyMismatch;

      result.skipped.push_back(s);
      continue;
    }

    pairs.push_back({read.record->composer,
                     read.record->label_id});
  }

  std::sort(pairs.begin(), pairs.end());

  for (const std::pair<std::string, std::string>& p : pairs) {
    if (result.entries.empty() ||
        result.entries.back().composer != p.first) {
      result.entries.push_back({p.first, {p.second}});
    } else if (result.entries.back().label_ids.back() != p.second) {
      result.entries.back().label_ids.push_back(p.second);
    }
  }

  return result;
}

std::span<const std::string> find_by_composer(const ComposerIndex& index, std::string_view composer) {
  int hi = index.size();
  int lo = 0;

  while (lo < hi) {
    int cur = (hi + lo) / 2;
    if (index[cur].composer == composer) {
      return index[cur].label_ids;
    } else if (index[cur].composer < composer) {
      lo = cur + 1;
    } else {
      hi = cur;
    }
  }

  return {};
}

VerificationReport verify_primary_index(std::istream& input, std::span<const PrimaryEntry> index) {
  VerificationReport v;

  v.entries_checked = index.size();

  for (std::size_t i = 0; i + 1 < index.size(); i++) {
    if (index[i].label_id > index[i + 1].label_id) {
      VerificationIssue issue;

      issue.type = VerificationIssueType::UnsortedIndex;
      issue.detail = "index no está ordenado.";
      issue.index_key = index[i + 1].label_id;
      issue.offset = index[i + 1].offset;

      v.issues.push_back(issue);
    }
  }

  std::vector<PrimaryEntry> sorted_index(index.begin(), index.end());
  std::sort(sorted_index.begin(), sorted_index.end(),
            [](const PrimaryEntry& a, const PrimaryEntry& b) { return a.label_id < b.label_id; });

  for (std::size_t i = 0; i + 1 < sorted_index.size(); i++) {
    if (sorted_index[i].label_id == sorted_index[i + 1].label_id) {
      VerificationIssue issue;

      issue.type = VerificationIssueType::DuplicateKey;
      issue.detail = "llave duplicada.";
      issue.index_key = sorted_index[i + 1].label_id;
      issue.offset = sorted_index[i + 1].offset;

      v.issues.push_back(issue);
    }
  }

  std::sort(sorted_index.begin(), sorted_index.end(),
            [](const PrimaryEntry& a, const PrimaryEntry& b) { return a.offset < b.offset; });

  for (std::size_t i = 0; i + 1 < sorted_index.size(); i++) {
    if (sorted_index[i].offset == sorted_index[i + 1].offset) {
      VerificationIssue issue;

      issue.type = VerificationIssueType::DuplicateOffset;
      issue.detail = "offset duplicado.";
      issue.index_key = sorted_index[i + 1].label_id;
      issue.offset = sorted_index[i + 1].offset;

      v.issues.push_back(issue);
    }
  }

  for (const PrimaryEntry& p : index) {
    ReadResult r = read_record_at(input, p.offset);

    if (!r.ok()) {
      VerificationIssue issue;

      issue.type = VerificationIssueType::RecordReadError;
      issue.index_key = p.label_id;
      issue.offset = p.offset;
      issue.read_status = r.status;
      issue.detail = r.detail;

      v.issues.push_back(issue);

      continue;
    }

    if (r.record->label_id != p.label_id) {
      VerificationIssue issue;

      issue.type = VerificationIssueType::KeyMismatch;
      issue.index_key = p.label_id;
      issue.offset = p.offset;
      issue.read_status = ReadStatus::Ok;
      issue.detail = "La llave del índice no coincide con la del registro.";

      v.issues.push_back(issue);

      continue;
    }

    v.readable_matching_entries++;
  }

  return v;
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
