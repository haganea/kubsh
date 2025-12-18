#include <iostream>
#include <string>
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <vector>

// 8.
#include <sys/wait.h> // waitpid
#include <unistd.h> // fork, execvp, getpid
#include <sstream> // для разбиения строки на части
#include <csignal> // 9.
#include <cstdint> // 10.
#include <cstring> // 10.
#include "vfs.hpp" // 11.

using namespace std;

const string ECHO = "debug"; // "echo";
const string HISTORY_FILE = ".kubsh_history";
string history_path;

void help() {
    cout << "Commands: " << "\n";
    cout << "help \t\t - help\n";
    cout << "\\p \t\t - print text and exit\n";
    cout << "\\q \t\t - exit\n";
    cout << "history \t - history\n";
    cout << "\\e <var> \t - show environment variable\n";
    cout << ECHO << " <text> \t - print text\n";
    cout << "Ctrl+D \t\t - exit\n";
    cout << "<command> \t - execute external program\n";
    cout << "\\l <disk> \t - scan disk partitions\n";
}

// 1. Печатает введённую строку и выходит (выход в main)
void print(const string& args) {
    cout << args << "\n";
}

// 4. История введённых команд и её сохранение в ~./kubsh_history
string get_history_path() {
	const char* home = getenv("HOME");
	return string(home) + "/" + HISTORY_FILE;
}
void add_to_history(const string& command) {
    if (!command.empty() && command != "\\q") {
        ofstream file(history_path, ios::app);
        if (file.is_open()) {
            file << command << "\n";
            file.close();
        }
        else cerr << "File is not opened\n";
    }
}
void history() {
    ifstream file(history_path);
    if (file.is_open()) {
        file.seekg(0);
        string line;
        while (getline(file, line))
            cout << line << "\n";
        file.close();
    }
    else cerr << "File is not opened\n";
}

// 5. Команда echo (debug)
void echo(const string& args) {
    cout << args << "\n";
}

// 7. Вывод переменной окружения
void env(const string& var_name) {
    string clean_var = var_name;
    //if (clean_var[0] == '$')
    //    clean_var = clean_var.substr(1);
    //clean_var.erase(remove_if(clean_var.begin(), clean_var.end(), ::isspace), clean_var.end());
    //clean_var.erase(remove(clean_var.begin(), clean_var.end(), ':'), clean_var.end());
    size_t start = clean_var.find_first_not_of(" \t");
    if (start != string::npos) {
        clean_var = clean_var.substr(start);
    }
    if (!clean_var.empty() && clean_var[0] == '$') {
        clean_var = clean_var.substr(1);
        // Убираем возможные пробелы после $
        start = clean_var.find_first_not_of(" \t");
        if (start != string::npos) {
            clean_var = clean_var.substr(start);
        }
    }
    size_t space_pos = clean_var.find_first_of(" \t");
    if (space_pos != string::npos) {
        clean_var = clean_var.substr(0, space_pos);
    }

    const char* value = getenv(clean_var.c_str());
    if (value != nullptr) {
        string env_value(value);
        if (env_value.find(':') != string::npos) {
            string curr = "";
            for (char c : env_value) {
                if (c == ':') {
                    cout << curr << "\n";
                    curr = "";
                }
                else curr += c;
            }
            if (!curr.empty()) cout << curr << "\n";
        }
        else cout << env_value << "\n";
    }
    else cout << clean_var << " not found" << "\n";
}

// 8. Выполнение бинарника
void execute_external_command(const string& cmd_line) {
    pid_t pid = fork(); // новый процесс
    if (pid == 0) { // если дочерний процесс
        vector<string> tokens;
        vector<char*> args;
        string token;
        istringstream iss(cmd_line);
        while (iss >> token) {
            tokens.push_back(token);
        }
        for (auto& t : tokens) {
            args.push_back(const_cast<char*>(t.c_str()));
        }
        args.push_back(nullptr); // конец массива
        execvp(args[0], args.data());
        cout << "command not found: " << args[0] << "\n"; // если execvp не удался 
        exit(1);  // завершаем дочерний процесс
    } 
    else if (pid > 0) { //  если родительский процесс
        int status;
        waitpid(pid, &status, 0);  // ждем завершения дочернего процесса
    } 
    else {
        cerr << "Failed to create process\n";
    }
}

// 9. Обработка сигнала SIGHUP
void handle_sighup(int sig_num) {
    if (sig_num == SIGHUP) {
        cout << "Configuration reloaded\n";
        cout << "$ ";
    }
}

// 10. Получение информации о разделах на диске
void scan_disk(const string& disk_path) {
    ifstream disk_file(disk_path, ios::binary);
    if (!disk_file) {
        cout << "Failed to access " << disk_path << "\n";
        return;
    }
    // Читаем первые 512 байт (загрузочный сектор)
    unsigned char boot_sector[512];
    disk_file.read((char*)boot_sector, 512);
    if (disk_file.gcount() != 512) {
        cout << "Disk read failure\n";
        return;
    }

    // Проверяем сигнатуру MBR
    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
        cout << "Invalid disk marker\n";
        return;
    }
    
    // Проверяем, является ли диск GPT (имеет раздел типа 0xEE)
    bool is_gpt_format = false;
    for (int part_idx = 0; part_idx < 4; part_idx++) {
        int table_start = 446 + part_idx * 16;
        if (boot_sector[table_start + 4] == 0xEE) {
            is_gpt_format = true;
            break;
        }
    }
    
    if (!is_gpt_format) {
        // Обработка MBR диска
        for (int part_idx = 0; part_idx < 4; part_idx++) {
            int table_offset = 446 + part_idx * 16;
            unsigned char partition_type = boot_sector[table_offset + 4];
            if (partition_type != 0) {
                //uint32_t sector_count = *(uint32_t*)&boot_sector[table_offset + 12];
                uint32_t sector_count;
                memcpy(&sector_count, &boot_sector[table_offset + 12], sizeof(sector_count));
                uint32_t size_mb = sector_count / 2048;
                bool can_boot = (boot_sector[table_offset] == 0x80);
                cout << "Part " << (part_idx + 1) << ": " 
                      << size_mb << "MB, Boot: " 
                      << (can_boot ? "Yes" : "No") << "\n";
            }
        }
    } else {
        // Обработка GPT диска
        disk_file.read((char*)boot_sector, 512);
        if (disk_file.gcount() == 512 && 
            boot_sector[0] == 'E' && boot_sector[1] == 'F' && 
            boot_sector[2] == 'I' && boot_sector[3] == ' ' &&
            boot_sector[4] == 'P' && boot_sector[5] == 'A' &&
            boot_sector[6] == 'R' && boot_sector[7] == 'T') {
            uint32_t part_count;
            memcpy(&part_count, &boot_sector[80], sizeof(part_count)); 
            cout << "GPT partitions found: " << part_count << "\n";
        } else {
            cout << "GPT data unavailable\n";
        }
    }
}

// Обработка команды и выбор функции
void process_command(const string& input) {
    if (input.empty()) return;
    if (input == "help") {
        help();
        return;
    }
    if (input.find("\\p") == 0) { // 1.
        string args = "";
        if (input.length() > 2) {
            args = input.substr(3);
            size_t pos = args.find_first_not_of(" ");
            if (pos != string::npos) {
                args = args.substr(pos);
            }
        }
        print(args);
        return;
    }
    if (input == "\\q") return; // 3.
    if (input == "history") { // 4.
        history();
        return;
    }
    if (input.find(ECHO) == 0) { // 5.
        string args = "";
        if (input.length() > ECHO.length()) {
            args = input.substr(ECHO.length());
            size_t pos = args.find_first_not_of(" ");
            if (pos != string::npos) {
                args = args.substr(pos);
            }
        }
        echo(args);
        return;
    }
    if (input.find("\\e $") == 0) { // 7.
        string args = "";
        if (input.length() > 4) {
            args = input.substr(4);
            size_t pos = args.find_first_not_of(" \t");
            if (pos != string::npos) {
                args = args.substr(pos);
            }
        }
        const char* value = getenv(args.c_str());
        if (value != nullptr) {
            string env_value(value);
            if (env_value.find(':') != string::npos) {
                string curr = "";
                for (char c : env_value) {
                    if (c == ':') {
                        cout << curr << "\n";
                        curr = "";
                    }
                    else curr += c;
                }
                if (!curr.empty()) cout << curr << "\n";
            }
            else {
                cout << env_value << "\n";
            }
        }
        else {
            cout << args << ": not found\n";
        }
        return;
    }
    if (input.find("\\l") == 0) { // 10.
        string args = "";
        if (input.length() > 2) {
            args = input.substr(2);
            size_t pos = args.find_first_not_of(" ");
            if (pos != string::npos) args = args.substr(pos);
        }
        if (args.empty()) {
            cout << "Format: \\l /dev/disk_name\n";
        } else {
            scan_disk(args);
        }
        return;
    }
    execute_external_command(input); // 8.
    // Убрала cout << "Command is not found\n"
}

int main() {
    init_virtual_fs();
    cout << unitbuf;
    cerr << unitbuf;
    history_path = get_history_path();
    signal(SIGHUP, handle_sighup);
    cerr << "$ ";
    string input;
    while (true) {
        if (!getline(cin, input)) break; // Ctrl+D
        if (input.empty()) {
            cerr << "$ ";
            continue;
        }
        add_to_history(input);
        process_command(input);
        if (input.find("\\p") == 0 || input == "\\q") break;
        cerr << "$ ";
    }
    return 0;
}



