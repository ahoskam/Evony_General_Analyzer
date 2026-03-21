#include "analyzer/json.h"

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {

struct JsonValue {
  enum class Type { Null, Bool, Number, String, Array, Object };

  Type type = Type::Null;
  bool bool_value = false;
  double number_value = 0.0;
  std::string string_value;
  std::vector<JsonValue> array_value;
  std::map<std::string, JsonValue> object_value;
};

class JsonParser {
public:
  explicit JsonParser(std::string text) : text_(std::move(text)) {}

  JsonValue parse() {
    skip_ws();
    JsonValue value = parse_value();
    skip_ws();
    if (pos_ != text_.size()) {
      throw std::runtime_error("unexpected trailing JSON content");
    }
    return value;
  }

private:
  JsonValue parse_value() {
    skip_ws();
    if (pos_ >= text_.size()) {
      throw std::runtime_error("unexpected end of JSON");
    }

    const char ch = text_[pos_];
    if (ch == '{') {
      return parse_object();
    }
    if (ch == '[') {
      return parse_array();
    }
    if (ch == '"') {
      JsonValue value;
      value.type = JsonValue::Type::String;
      value.string_value = parse_string();
      return value;
    }
    if (ch == 't' || ch == 'f') {
      return parse_bool();
    }
    if (ch == 'n') {
      return parse_null();
    }
    if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
      return parse_number();
    }

    throw std::runtime_error("unexpected JSON token");
  }

  JsonValue parse_object() {
    expect('{');
    JsonValue value;
    value.type = JsonValue::Type::Object;
    skip_ws();
    if (peek('}')) {
      expect('}');
      return value;
    }

    while (true) {
      skip_ws();
      const std::string key = parse_string();
      skip_ws();
      expect(':');
      skip_ws();
      value.object_value.emplace(key, parse_value());
      skip_ws();
      if (peek('}')) {
        expect('}');
        break;
      }
      expect(',');
    }

    return value;
  }

  JsonValue parse_array() {
    expect('[');
    JsonValue value;
    value.type = JsonValue::Type::Array;
    skip_ws();
    if (peek(']')) {
      expect(']');
      return value;
    }

    while (true) {
      value.array_value.push_back(parse_value());
      skip_ws();
      if (peek(']')) {
        expect(']');
        break;
      }
      expect(',');
    }

    return value;
  }

  JsonValue parse_bool() {
    JsonValue value;
    value.type = JsonValue::Type::Bool;
    if (match("true")) {
      value.bool_value = true;
      return value;
    }
    if (match("false")) {
      value.bool_value = false;
      return value;
    }
    throw std::runtime_error("invalid JSON bool");
  }

  JsonValue parse_null() {
    if (!match("null")) {
      throw std::runtime_error("invalid JSON null");
    }
    JsonValue value;
    value.type = JsonValue::Type::Null;
    return value;
  }

  JsonValue parse_number() {
    const size_t start = pos_;
    if (text_[pos_] == '-') {
      ++pos_;
    }
    while (pos_ < text_.size() &&
           std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
      ++pos_;
    }
    if (pos_ < text_.size() && text_[pos_] == '.') {
      ++pos_;
      while (pos_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
        ++pos_;
      }
    }

    JsonValue value;
    value.type = JsonValue::Type::Number;
    value.number_value = std::stod(text_.substr(start, pos_ - start));
    return value;
  }

  std::string parse_string() {
    expect('"');
    std::string out;
    while (pos_ < text_.size()) {
      const char ch = text_[pos_++];
      if (ch == '"') {
        return out;
      }
      if (ch == '\\') {
        if (pos_ >= text_.size()) {
          throw std::runtime_error("invalid JSON escape");
        }
        const char esc = text_[pos_++];
        switch (esc) {
          case '"':
          case '\\':
          case '/':
            out.push_back(esc);
            break;
          case 'b':
            out.push_back('\b');
            break;
          case 'f':
            out.push_back('\f');
            break;
          case 'n':
            out.push_back('\n');
            break;
          case 'r':
            out.push_back('\r');
            break;
          case 't':
            out.push_back('\t');
            break;
          default:
            throw std::runtime_error("unsupported JSON escape");
        }
      } else {
        out.push_back(ch);
      }
    }
    throw std::runtime_error("unterminated JSON string");
  }

  bool peek(char ch) const { return pos_ < text_.size() && text_[pos_] == ch; }

  void expect(char ch) {
    if (pos_ >= text_.size() || text_[pos_] != ch) {
      throw std::runtime_error("unexpected JSON character");
    }
    ++pos_;
  }

  bool match(std::string_view token) {
    if (text_.compare(pos_, token.size(), token) != 0) {
      return false;
    }
    pos_ += token.size();
    return true;
  }

  void skip_ws() {
    while (pos_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[pos_]))) {
      ++pos_;
    }
  }

  std::string text_;
  size_t pos_ = 0;
};

const JsonValue* find_member(const JsonValue& value, const char* key) {
  if (value.type != JsonValue::Type::Object) {
    return nullptr;
  }
  auto it = value.object_value.find(key);
  return it == value.object_value.end() ? nullptr : &it->second;
}

int as_int(const JsonValue* value, int def = 0) {
  if (!value || value->type != JsonValue::Type::Number) {
    return def;
  }
  return static_cast<int>(value->number_value);
}

std::string as_string(const JsonValue* value, const std::string& def = {}) {
  if (!value || value->type != JsonValue::Type::String) {
    return def;
  }
  return value->string_value;
}

std::string escape_json(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (char ch : value) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  return out;
}

void write_indent(std::ostream& out, int indent) {
  for (int i = 0; i < indent; ++i) {
    out.put(' ');
  }
}

}  // namespace

OwnedStateFile load_owned_state_file(const std::string& path) {
  OwnedStateFile result;

  std::ifstream in(path);
  if (!in) {
    return result;
  }

  std::ostringstream buffer;
  buffer << in.rdbuf();

  JsonParser parser(buffer.str());
  const JsonValue root = parser.parse();
  result.schema_version = as_int(find_member(root, "schema_version"), 1);
  result.db_path_hint = as_string(find_member(root, "db_path_hint"));

  const JsonValue* generals = find_member(root, "generals");
  if (!generals || generals->type != JsonValue::Type::Object) {
    return result;
  }

  for (const auto& [id_text, entry] : generals->object_value) {
    if (entry.type != JsonValue::Type::Object) {
      continue;
    }

    OwnedGeneralState owned;
    owned.general_id = as_int(find_member(entry, "general_id"),
                              std::stoi(id_text));
    owned.general_name = as_string(find_member(entry, "general_name"));
    owned.locked = as_int(find_member(entry, "locked"), 0) != 0;
    owned.has_dragon = as_int(find_member(entry, "has_dragon"), 0) != 0;
    owned.has_spirit_beast =
        as_int(find_member(entry, "has_spirit_beast"), 0) != 0;
    owned.general_level = as_int(find_member(entry, "general_level"), 1);
    owned.ascension_level = as_int(find_member(entry, "ascension_level"));
    owned.covenant_level = as_int(find_member(entry, "covenant_level"));

    const JsonValue* specialty_levels = find_member(entry, "specialty_levels");
    if (specialty_levels &&
        specialty_levels->type == JsonValue::Type::Array) {
      for (size_t i = 0; i < owned.specialty_levels.size() &&
                         i < specialty_levels->array_value.size();
           ++i) {
        owned.specialty_levels[i] =
            as_int(&specialty_levels->array_value[i], 0);
      }
    }

    const JsonValue* cached_totals = find_member(entry, "cached_totals");
    if (cached_totals && cached_totals->type == JsonValue::Type::Object) {
      owned.cached_input_key = as_string(find_member(*cached_totals, "input_key"));
      const JsonValue* stats = find_member(*cached_totals, "stats");
      if (stats && stats->type == JsonValue::Type::Object) {
        for (const auto& [key, value] : stats->object_value) {
          if (value.type == JsonValue::Type::Number) {
            owned.cached_totals[key] = value.number_value;
          }
        }
      }
    }

    result.generals[owned.general_id] = std::move(owned);
  }

  return result;
}

void save_owned_state_file(const std::string& path, const OwnedStateFile& file) {
  const std::filesystem::path out_path(path);
  if (out_path.has_parent_path()) {
    std::filesystem::create_directories(out_path.parent_path());
  }

  std::ofstream out(path, std::ios::trunc);
  if (!out) {
    throw std::runtime_error("failed to open state file for write: " + path);
  }

  out << "{\n";
  write_indent(out, 2);
  out << "\"schema_version\": " << file.schema_version << ",\n";
  write_indent(out, 2);
  out << "\"db_path_hint\": \"" << escape_json(file.db_path_hint) << "\",\n";
  write_indent(out, 2);
  out << "\"generals\": {\n";

  bool first_general = true;
  for (const auto& [general_id, owned] : file.generals) {
    if (!first_general) {
      out << ",\n";
    }
    first_general = false;

    write_indent(out, 4);
    out << "\"" << general_id << "\": {\n";
    write_indent(out, 6);
    out << "\"general_id\": " << owned.general_id << ",\n";
    write_indent(out, 6);
    out << "\"general_name\": \"" << escape_json(owned.general_name) << "\",\n";
    write_indent(out, 6);
    out << "\"locked\": " << (owned.locked ? 1 : 0) << ",\n";
    write_indent(out, 6);
    out << "\"has_dragon\": " << (owned.has_dragon ? 1 : 0) << ",\n";
    write_indent(out, 6);
    out << "\"has_spirit_beast\": " << (owned.has_spirit_beast ? 1 : 0)
        << ",\n";
    write_indent(out, 6);
    out << "\"general_level\": " << owned.general_level << ",\n";
    write_indent(out, 6);
    out << "\"ascension_level\": " << owned.ascension_level << ",\n";
    write_indent(out, 6);
    out << "\"specialty_levels\": ["
        << owned.specialty_levels[0] << ", "
        << owned.specialty_levels[1] << ", "
        << owned.specialty_levels[2] << ", "
        << owned.specialty_levels[3] << "],\n";
    write_indent(out, 6);
    out << "\"covenant_level\": " << owned.covenant_level;

    if (!owned.cached_input_key.empty() || !owned.cached_totals.empty()) {
      out << ",\n";
      write_indent(out, 6);
      out << "\"cached_totals\": {\n";
      write_indent(out, 8);
      out << "\"input_key\": \"" << escape_json(owned.cached_input_key)
          << "\",\n";
      write_indent(out, 8);
      out << "\"stats\": {\n";

      bool first_stat = true;
      for (const auto& [stat_key, value] : owned.cached_totals) {
        if (!first_stat) {
          out << ",\n";
        }
        first_stat = false;
        write_indent(out, 10);
        out << "\"" << escape_json(stat_key) << "\": " << value;
      }
      if (!owned.cached_totals.empty()) {
        out << "\n";
      }
      write_indent(out, 8);
      out << "}\n";
      write_indent(out, 6);
      out << "}";
    }

    out << "\n";
    write_indent(out, 4);
    out << "}";
  }

  if (!file.generals.empty()) {
    out << "\n";
  }
  write_indent(out, 2);
  out << "}\n";
  out << "}\n";
}
