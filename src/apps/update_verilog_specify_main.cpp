// ***************************************************************************************
// Copyright (c) 2026
// ***************************************************************************************
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Lib.hh"

namespace {

struct VerilogToken
{
  std::string text;
  std::size_t begin = 0;
  std::size_t end = 0;
};

struct VerilogModule
{
  std::string name;
  std::size_t specify_end = 0;
  std::size_t endspecify_begin = 0;
};

struct VerilogReplacement
{
  std::size_t begin = 0;
  std::size_t end = 0;
  std::string text;
};

struct LibertySpecifyArc
{
  std::string source_port;
  std::string sink_port;
  std::string condition;
};

bool isVerilogIdentifierStart(const char ch)
{
  const auto character = static_cast<unsigned char>(ch);
  return std::isalpha(character) || ch == '_' || ch == '$';
}

bool isVerilogIdentifierCharacter(const char ch)
{
  const auto character = static_cast<unsigned char>(ch);
  return std::isalnum(character) || ch == '_' || ch == '$';
}

std::vector<VerilogToken> tokenizeVerilog(std::string_view text)
{
  std::vector<VerilogToken> tokens;
  for (std::size_t index = 0; index < text.size();) {
    if (text[index] == '/' && index + 1 < text.size() && text[index + 1] == '/') {
      index = text.find('\n', index + 2);
      if (index == std::string::npos) {
        break;
      }
      ++index;
      continue;
    }
    if (text[index] == '/' && index + 1 < text.size() && text[index + 1] == '*') {
      const std::size_t comment_end = text.find("*/", index + 2);
      if (comment_end == std::string::npos) {
        break;
      }
      index = comment_end + 2;
      continue;
    }
    if (text[index] == '"') {
      ++index;
      while (index < text.size()) {
        if (text[index] == '\\' && index + 1 < text.size()) {
          index += 2;
        } else if (text[index++] == '"') {
          break;
        }
      }
      continue;
    }
    if (text[index] == '\\') {
      const std::size_t begin = index++;
      while (index < text.size() && !std::isspace(static_cast<unsigned char>(text[index]))) {
        ++index;
      }
      tokens.push_back({std::string{text.substr(begin, index - begin)}, begin, index});
      continue;
    }
    if (isVerilogIdentifierStart(text[index])) {
      const std::size_t begin = index++;
      while (index < text.size() && isVerilogIdentifierCharacter(text[index])) {
        ++index;
      }
      tokens.push_back({std::string{text.substr(begin, index - begin)}, begin, index});
      continue;
    }
    ++index;
  }
  return tokens;
}

std::string_view normalizeVerilogIdentifier(const std::string_view identifier)
{
  return !identifier.empty() && identifier.front() == '\\' ? identifier.substr(1) : identifier;
}

bool findVerilogModules(const std::string_view text, std::vector<VerilogModule>& modules, std::string& error_message)
{
  const std::vector<VerilogToken> tokens = tokenizeVerilog(text);
  for (std::size_t index = 0; index < tokens.size(); ++index) {
    if (tokens[index].text != "module") {
      continue;
    }
    if (index + 1 >= tokens.size()) {
      error_message = "Verilog module declaration is missing a module name";
      return false;
    }

    std::size_t module_end_index = index + 2;
    while (module_end_index < tokens.size() && tokens[module_end_index].text != "endmodule") {
      if (tokens[module_end_index].text == "module") {
        error_message = "Verilog module " + std::string{normalizeVerilogIdentifier(tokens[index + 1].text)} + " is missing endmodule";
        return false;
      }
      ++module_end_index;
    }
    if (module_end_index == tokens.size()) {
      error_message = "Verilog module " + std::string{normalizeVerilogIdentifier(tokens[index + 1].text)} + " is missing endmodule";
      return false;
    }

    VerilogModule module;
    module.name = normalizeVerilogIdentifier(tokens[index + 1].text);

    std::size_t specify_index = tokens.size();
    std::size_t endspecify_index = tokens.size();
    for (std::size_t token_index = index + 2; token_index < module_end_index; ++token_index) {
      if (tokens[token_index].text == "specify") {
        if (specify_index != tokens.size()) {
          error_message = "Verilog module " + module.name + " has multiple specify blocks";
          return false;
        }
        specify_index = token_index;
      } else if (tokens[token_index].text == "endspecify") {
        if (specify_index == tokens.size() || endspecify_index != tokens.size()) {
          error_message = "Verilog module " + module.name + " has an unmatched endspecify";
          return false;
        }
        endspecify_index = token_index;
      }
    }

    if (specify_index != tokens.size() || endspecify_index != tokens.size()) {
      if (specify_index == tokens.size() || endspecify_index == tokens.size()) {
        error_message = "Verilog module " + module.name + " has an incomplete specify block";
        return false;
      }
      module.specify_end = tokens[specify_index].end;
      module.endspecify_begin = tokens[endspecify_index].begin;
    }

    modules.push_back(std::move(module));
    index = module_end_index;
  }
  return true;
}

bool containsPathDelayArrow(std::string_view text, const std::size_t begin, const std::size_t end)
{
  for (std::size_t index = begin; index < end;) {
    if (text[index] == '/' && index + 1 < end && text[index + 1] == '/') {
      index = text.find('\n', index + 2);
      if (index == std::string::npos || index >= end) {
        return false;
      }
      ++index;
      continue;
    }
    if (text[index] == '/' && index + 1 < end && text[index + 1] == '*') {
      const std::size_t comment_end = text.find("*/", index + 2);
      if (comment_end == std::string::npos || comment_end >= end) {
        return false;
      }
      index = comment_end + 2;
      continue;
    }
    if (text[index] == '"') {
      ++index;
      while (index < end) {
        if (text[index] == '\\' && index + 1 < end) {
          index += 2;
        } else if (text[index++] == '"') {
          break;
        }
      }
      continue;
    }
    if (text[index] == '\\') {
      ++index;
      while (index < end && !std::isspace(static_cast<unsigned char>(text[index]))) {
        ++index;
      }
      continue;
    }
    if ((text[index] == '=' || text[index] == '*') && index + 1 < end && text[index + 1] == '>') {
      return true;
    }
    ++index;
  }
  return false;
}

std::size_t findStatementCodeBegin(const std::string_view text, const std::size_t begin, const std::size_t end)
{
  std::size_t index = begin;
  while (index < end) {
    if (std::isspace(static_cast<unsigned char>(text[index]))) {
      ++index;
      continue;
    }
    if (text[index] == '/' && index + 1 < end && text[index + 1] == '/') {
      const std::size_t comment_end = text.find('\n', index + 2);
      index = comment_end == std::string::npos || comment_end >= end ? end : comment_end + 1;
      continue;
    }
    if (text[index] == '/' && index + 1 < end && text[index + 1] == '*') {
      const std::size_t comment_end = text.find("*/", index + 2);
      index = comment_end == std::string::npos || comment_end + 2 > end ? end : comment_end + 2;
      continue;
    }
    break;
  }
  return index;
}

std::string removePathDelayStatements(std::string_view specify_body)
{
  std::string preserved;
  std::size_t statement_begin = 0;
  for (std::size_t index = 0; index < specify_body.size();) {
    if (specify_body[index] == '/' && index + 1 < specify_body.size() && specify_body[index + 1] == '/') {
      const std::size_t comment_end = specify_body.find('\n', index + 2);
      index = comment_end == std::string::npos ? specify_body.size() : comment_end + 1;
      continue;
    }
    if (specify_body[index] == '/' && index + 1 < specify_body.size() && specify_body[index + 1] == '*') {
      const std::size_t comment_end = specify_body.find("*/", index + 2);
      index = comment_end == std::string::npos ? specify_body.size() : comment_end + 2;
      continue;
    }
    if (specify_body[index] == '"') {
      ++index;
      while (index < specify_body.size()) {
        if (specify_body[index] == '\\' && index + 1 < specify_body.size()) {
          index += 2;
        } else if (specify_body[index++] == '"') {
          break;
        }
      }
      continue;
    }
    if (specify_body[index] == '\\') {
      ++index;
      while (index < specify_body.size() && !std::isspace(static_cast<unsigned char>(specify_body[index]))) {
        ++index;
      }
      continue;
    }
    if (specify_body[index++] != ';') {
      continue;
    }
    if (containsPathDelayArrow(specify_body, statement_begin, index)) {
      const std::size_t code_begin = findStatementCodeBegin(specify_body, statement_begin, index);
      preserved.append(specify_body, statement_begin, code_begin - statement_begin);
    } else {
      preserved.append(specify_body, statement_begin, index - statement_begin);
    }
    statement_begin = index;
  }
  preserved.append(specify_body, statement_begin, std::string::npos);
  return preserved;
}

bool isPureCombinationalCell(idb::LibCell& cell, std::vector<LibertySpecifyArc>& arcs)
{
  bool has_delay_arc = false;
  for (const std::unique_ptr<idb::LibArcSet>& arc_set : cell.get_cell_arcs()) {
    for (const std::unique_ptr<idb::LibArc>& arc : arc_set->get_arcs()) {
      const idb::LibArc::TimingType timing_type = arc->get_timing_type();
      const bool is_combinational = timing_type == idb::LibArc::TimingType::kComb || timing_type == idb::LibArc::TimingType::kCombRise
                                    || timing_type == idb::LibArc::TimingType::kCombFall
                                    || timing_type == idb::LibArc::TimingType::kDefault;
      if (!is_combinational || !arc->isDelayArc()) {
        return false;
      }
      if (std::string(arc->get_src_port()).empty() || std::string(arc->get_snk_port()).empty()) {
        return false;
      }
      has_delay_arc = true;
      arcs.push_back({arc->get_src_port(), arc->get_snk_port(), arc->get_sdf_cond()});
    }
  }
  return has_delay_arc;
}

std::string trimString(std::string value)
{
  const auto first = std::ranges::find_if_not(value, [](const auto ch) { return std::isspace(ch); });
  const auto last = std::ranges::find_if_not(value | std::views::reverse, [](const auto ch) { return std::isspace(ch); }).base();

  return first < last ? std::string(first, last) : std::string{};
}

std::string makePathDelayStatements(const std::vector<LibertySpecifyArc>& arcs)
{
  std::ostringstream stream;
  for (const LibertySpecifyArc& arc : arcs) {
    stream << "\n  ";
    const std::string condition = trimString(arc.condition);
    if (!condition.empty()) {
      stream << "if (" << condition << ") ";
    }
    stream << '(' << arc.source_port << " => " << arc.sink_port << ") = (1.0,1.0);";
  }
  return stream.str();
}

std::string collapseConsecutiveBlankLines(std::string_view text)
{
  std::string collapsed;
  bool previous_line_was_blank = false;
  std::size_t line_begin = 0;
  while (line_begin < text.size()) {
    const std::size_t newline = text.find('\n', line_begin);
    const std::size_t line_end = newline == std::string::npos ? text.size() : newline;
    const bool is_blank = std::all_of(text.begin() + line_begin, text.begin() + line_end, [](const auto ch) { return std::isspace(ch); });
    if (!is_blank || !previous_line_was_blank) {
      collapsed.append(text, line_begin, newline == std::string::npos ? std::string::npos : line_end - line_begin + 1);
    }
    previous_line_was_blank = is_blank;
    if (newline == std::string::npos) {
      break;
    }
    line_begin = newline + 1;
  }
  return collapsed;
}

bool readTextFile(const std::filesystem::path& path, std::string& text, std::string& error_message)
{
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    error_message = "failed to read Verilog file " + path.string();
    return false;
  }
  text.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
  if (!stream.good() && !stream.eof()) {
    error_message = "failed while reading Verilog file " + path.string();
    return false;
  }
  return true;
}

bool updateVerilogSpecify(std::string_view liberty_file, std::string_view verilog_file, std::string& report, std::string& error_message)
{
  const std::filesystem::path liberty_path(liberty_file);
  const std::filesystem::path verilog_path(verilog_file);
  std::error_code filesystem_error;
  if (!std::filesystem::is_regular_file(liberty_path, filesystem_error) || filesystem_error) {
    error_message = "cannot read Liberty file " + liberty_path.string();
    return false;
  }
  if (!std::filesystem::is_regular_file(verilog_path, filesystem_error) || filesystem_error) {
    error_message = "cannot read Verilog file " + verilog_path.string();
    return false;
  }

  std::string verilog_text;
  if (!readTextFile(verilog_path, verilog_text, error_message)) {
    return false;
  }

  std::vector<VerilogModule> verilog_modules;
  if (!findVerilogModules(verilog_text, verilog_modules, error_message)) {
    return false;
  }

  const bool old_silent_output = idb::Lib::isSilentOutput();
  idb::Lib::setSilentOutput(true);
  idb::Lib lib;
  idb::LibertyReader liberty_reader = lib.loadLibertyWithCppParser(liberty_path.string().c_str());
  if (!liberty_reader.linkLib()) {
    idb::Lib::setSilentOutput(old_silent_output);
    error_message = "failed to parse Liberty file " + liberty_path.string();
    return false;
  }
  idb::LibBuilder* library_builder = liberty_reader.get_library_builder();
  if (library_builder == nullptr) {
    idb::Lib::setSilentOutput(old_silent_output);
    error_message = "did not build a Liberty library from " + liberty_path.string();
    return false;
  }
  std::unique_ptr<idb::LibLibrary> liberty_library = library_builder->takeLib();
  delete library_builder;
  liberty_reader.set_library_builder(nullptr);
  idb::Lib::setSilentOutput(old_silent_output);
  if (liberty_library == nullptr) {
    error_message = "did not build a Liberty library from " + liberty_path.string();
    return false;
  }

  std::map<std::string, std::vector<LibertySpecifyArc>> combinational_cell_arcs;
  std::set<std::string> skipped_cell_names;
  for (const std::unique_ptr<idb::LibCell>& cell : liberty_library->get_cells()) {
    std::vector<LibertySpecifyArc> arcs;
    if (isPureCombinationalCell(*cell, arcs)) {
      combinational_cell_arcs.emplace(cell->get_cell_name(), std::move(arcs));
    } else {
      skipped_cell_names.emplace(cell->get_cell_name());
    }
  }

  std::vector<VerilogReplacement> replacements;
  std::size_t skipped_module_count = 0;
  for (const auto& [name, specify_end, endspecify_begin] : verilog_modules) {
    const auto combinational_cell_iter = combinational_cell_arcs.find(name);
    if (combinational_cell_iter == combinational_cell_arcs.end()) {
      if (skipped_cell_names.contains(name)) {
        ++skipped_module_count;
      }
      continue;
    }
    if (specify_end == 0 || endspecify_begin == 0) {
      error_message = "requires a complete specify block in pure combinational module " + name;
      return false;
    }

    const std::string original_body = verilog_text.substr(specify_end, endspecify_begin - specify_end);
    const std::string replacement_body = collapseConsecutiveBlankLines(makePathDelayStatements(combinational_cell_iter->second)
                                                                       + removePathDelayStatements(original_body));
    replacements.push_back({specify_end, endspecify_begin, replacement_body});
  }

  if (replacements.empty()) {
    std::ostringstream stream;
    stream << "updated 0 pure combinational modules; skipped " << skipped_module_count
           << " sequential or non-combinational modules; no backup created";
    report = stream.str();
    return true;
  }

  std::ranges::sort(replacements,
            [](const VerilogReplacement& lhs, const VerilogReplacement& rhs) { return lhs.begin > rhs.begin; });
  std::string updated_verilog = verilog_text;
  for (const auto& [begin, end, text] : replacements) {
    updated_verilog.replace(begin, end - begin, text);
  }

  std::filesystem::path temporary_path = verilog_path;
  temporary_path += ".update_verilog_specify.tmp";
  for (std::size_t suffix = 1; std::filesystem::exists(temporary_path, filesystem_error) && !filesystem_error; ++suffix) {
    temporary_path = verilog_path;
    temporary_path += ".update_verilog_specify.tmp." + std::to_string(suffix);
  }
  if (filesystem_error) {
    error_message = "cannot create a temporary file next to " + verilog_path.string();
    return false;
  }

  {
    std::ofstream stream(temporary_path, std::ios::binary | std::ios::trunc);
    if (!stream) {
      error_message = "cannot write temporary file " + temporary_path.string();
      return false;
    }
    stream << updated_verilog;
    if (!stream) {
      stream.close();
      std::filesystem::remove(temporary_path, filesystem_error);
      error_message = "failed while writing temporary file " + temporary_path.string();
      return false;
    }
  }

  const std::filesystem::path backup_path = verilog_path.string() + ".bak";
  std::filesystem::copy_file(verilog_path, backup_path, std::filesystem::copy_options::overwrite_existing, filesystem_error);
  if (filesystem_error) {
    std::filesystem::remove(temporary_path, filesystem_error);
    error_message = "cannot write backup file " + backup_path.string();
    return false;
  }
  std::filesystem::rename(temporary_path, verilog_path, filesystem_error);
  if (filesystem_error) {
    std::filesystem::remove(temporary_path, filesystem_error);
    error_message = "cannot replace Verilog file " + verilog_path.string();
    return false;
  }

  std::ostringstream stream;
  stream << "updated " << replacements.size() << " pure combinational modules; skipped " << skipped_module_count
         << " sequential or non-combinational modules; backup " << backup_path.string();
  report = stream.str();
  return true;
}

void printUsage(const char* program)
{
  std::cerr << "Usage: " << program << " -liberty <library.lib> -verilog <cell_library.v>\n";
}

}  // namespace

#ifndef UPDATE_VERILOG_SPECIFY_TEST
int main(int argc, char* argv[])
{
  std::string liberty_file;
  std::string verilog_file;
  for (int index = 1; index < argc; ++index) {
    const std::string option(argv[index]);
    if (option == "-h" || option == "--help") {
      printUsage(argv[0]);
      return 0;
    }
    if (option != "-liberty" && option != "-verilog") {
      std::cerr << "Unknown option: " << option << '\n';
      printUsage(argv[0]);
      return 2;
    }
    if (++index == argc) {
      std::cerr << "Missing value for " << option << '\n';
      printUsage(argv[0]);
      return 2;
    }
    if (option == "-liberty") {
      liberty_file = argv[index];
    } else {
      verilog_file = argv[index];
    }
  }
  if (liberty_file.empty() || verilog_file.empty()) {
    printUsage(argv[0]);
    return 2;
  }

  std::string report;
  std::string error_message;
  if (!updateVerilogSpecify(liberty_file, verilog_file, report, error_message)) {
    std::cerr << "update_verilog_specify: " << error_message << '\n';
    return 1;
  }
  std::cout << report << '\n';
  return 0;
}
#endif
