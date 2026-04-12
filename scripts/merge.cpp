#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>

static std::unordered_map<std::string, std::string> loadRuntimeValues(
  std::string_view path) {
  std::unordered_map<std::string, std::string> values;
  std::ifstream                                val_file(path.data());

  if (!val_file.is_open()) {
    std::cerr << "Warning: " << path << " did not found.\n";
    return values;
  }

  std::string id, val;
  while (val_file >> id >> val) {
    values[id] = val;
  }

  return values;
}

static bool mergeGraph(std::string_view dot_path, std::string_view out_path,
                       const std::unordered_map<std::string, std::string>& values) {
  std::ifstream dot_file(dot_path.data());
  if (!dot_file.is_open()) {
    std::cerr << "Error: Can not open " << dot_path << "\n";
    return false;
  }

  std::ofstream out_file(out_path.data());
  if (!out_file.is_open()) {
    std::cerr << "Error: Can not open " << out_path << "\n";
    return false;
  }

  std::string line;
  while (std::getline(dot_file, line)) {
    if (line.find("[label=\"") != std::string::npos) {
      size_t id_start = line.find_first_not_of(" \t");
      size_t id_end   = line.find(" ", id_start);

      std::string node_id = line.substr(id_start, id_end - id_start);

      if (values.find(node_id) != values.end()) {
        std::string val = values.at(node_id);

        size_t insert_pos = line.find("\", shape=record");

        if (insert_pos != std::string::npos) {
          std::string string_start = line.substr(0, insert_pos);
          std::string string_end   = line.substr(insert_pos);

          out_file << string_start << " | VALUE=" << val << string_end << "\n";
          continue;
        }
      }
    }

    out_file << line << "\n";
  }

  std::cout << "Final graph built: " << out_path << "\n";
  return true;
}

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " <dot_file> <runtime_vals> <output_dot>\n";
    return 1;
  }

  std::string_view dot_path = argv[1];
  std::string_view val_path = argv[2];
  std::string_view out_path = argv[3];

  auto values = loadRuntimeValues(val_path);
  if (!mergeGraph(dot_path, out_path, values)) {
    return 1;
  }

  return 0;
}
