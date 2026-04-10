#include <iostream>
#include <fstream>
#include <string>
#include <map>

int main(int argc, char** argv) { // TODO[flops]: Split it into separate funcs
    if (argc < 4) { // TODO[flops]: !=, We need all 4 args
        std::cerr << "Usage: " << argv[0] << " <dot_file> <runtime_vals> <output_dot>\n";
        return 1;
    }

    std::string dot_path = argv[1]; // TODO[flops]: std::string_view
    std::string val_path = argv[2];
    std::string out_path = argv[3];

    std::map<std::string, std::string> values; // FIXME[flops]: You don't need ordering there too
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
        
        if (line.find("->") != std::string::npos) {
            out_file << line << "\n"; // FIXME[flops]: You do it in every condition edge, so you can move this operation from if and work only with `line.find("[label=\"")` case
        } 
        else if (line.find("[label=\"") != std::string::npos) {
            
            size_t id_start = line.find_first_not_of(" \t");
            size_t id_end   = line.find(" ", id_start);
           
            std::string node_id = line.substr(id_start, id_end - id_start);

            if (values.find(node_id) != values.end()) {
                std::string val = values[node_id];
                
                size_t insert_pos = line.find("\", shape=record");
                
                if (insert_pos != std::string::npos) {
                    std::string string_start = line.substr(0, insert_pos);
                    std::string string_end   = line.substr(insert_pos);
                    
                    out_file << string_start << " | VALUE=" << val << string_end << "\n";
                    continue;
                }
            }
            
            out_file << line << "\n";
        } 
        else {
            out_file << line << "\n";
        }
    }

    dot_file.close(); // FIXME[flops]: Close is unnecessary. RAII handles that
    out_file.close();

    std::cout << "Final graph built: " << out_path << "\n";
    return 0;
}
