#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <regex>

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <dot_file> <runtime_vals> <output_dot>\n";
        return 1;
    }

    std::string dot_path = argv[1];
    std::string val_path = argv[2];
    std::string out_path = argv[3];

    std::map<std::string, std::string> values;
    std::ifstream val_file(val_path);
    if (!val_file.is_open()) {
        std::cerr << "Warning: " << val_path << " did not found.\n";
    } else {
        std::string id, val;
        while (val_file >> id >> val) {
            values[id] = val;
        }
        val_file.close();
    }

    std::regex node_regex(R"delim(^(\s*)(\d+)\s+\[label="(.*)"\];(.*)$)delim");
    std::regex edge_regex(R"delim(^(\s*\d+\s*->\s*\d+\s*;.*)$)delim");
    
    std::ifstream dot_file(dot_path);
    if (!dot_file.is_open()) {
        std::cerr << "Error: Can not open " << dot_path << "\n";
        return 1;
    }

    std::ofstream out_file(out_path);
    if (!out_file.is_open()) {
        std::cerr << "Error: Can not open " << out_path << "\n";
        return 1;
    }

    std::string line;
    while (std::getline(dot_file, line)) {
        std::smatch match;

        if (std::regex_match(line, match, node_regex)) {
            std::string indent = match[1];
            std::string node_id = match[2];
            std::string label = match[3];
            std::string rest = match[4];

            if (values.find(node_id) != values.end()) {
                std::string val = values[node_id];
                std::string new_label = label + " | VALUE=" + val;
                out_file << indent << node_id << " [label=\"" << new_label 
                         << "\", shape=record, style=filled, fillcolor=lightgrey];" << rest << "\n";
            } else {
                out_file << indent << node_id << " [label=\"" << label 
                         << "\", shape=record, style=filled, fillcolor=lightgrey];" << rest << "\n";
            }
        } 
        else if (std::regex_match(line, match, edge_regex)) {
            std::string new_line = line;
            size_t pos = new_line.find(';');
            if (pos != std::string::npos) {
                new_line.replace(pos, 1, " [color=pink, fontcolor=pink, label=\"user\"];");
            }
            out_file << new_line << "\n";
        } 
        else {
            out_file << line << "\n";
        }
    }

    dot_file.close();
    out_file.close();

    std::cout << "Final graph built: " << out_path << "\n";
    return 0;
}
