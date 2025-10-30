#include <iostream>
#include <string>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cctype>
using namespace std;

const string ECHO = "debug"; // "echo";
const string HISTORY_FILE = ".kubsh_history";
string history_path;

void help() {
    cout << "Commands: " << "\n";
    cout << "help \t\t - help\n";
    cout << "\\p \t\t - print text and exit\n";
    cout << "\\pw \t\t - print text until Ctrl+D not pressed\n";
    cout << "\\q \t\t - exit\n";
    cout << "history \t - history\n";
    cout << "\\e <var> \t - show environment variable\n";
    cout << "\\debug <text> \t - print text\n";
    cout << "Ctrl+D \t\t - exit\n";
}

// 1. Печатает введённую строку и выходит (выход в main)
void print(const string& args) {
    cout << args << "\n";
}

// 2. Печатает введённую строку в цикле, пока не Ctrl+D
// затем выход из терминала
void print_while(const string& args) {
    string input;
    while (true) {
        cout << args << "\n";
        this_thread::sleep_for(chrono::seconds(2));
        if (!getline(cin, input)) break;
    }
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
    if (clean_var[0] == '$')
        clean_var = clean_var.substr(1);
    clean_var.erase(remove_if(clean_var.begin(), clean_var.end(), ::isspace), clean_var.end());
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
    else cout << clean_var << "not found" << "\n";
}

// Обработка команды и выбор функции
void process_command(const string& input) {
    if (input.empty()) return;
    if (input == "help") {
        help();
        return;
    }
    if (input.find("\\p") == 0) { // 1.
        string args = input.substr(3);
        size_t pos = args.find_first_not_of(" ");
        if (pos != string::npos) args = args.substr(pos);
        print(args);
        return;
    }
    if (input.find("\\pw") == 0) { // 2.
        string args = input.substr(4);
        size_t pos = args.find_first_not_of(" ");
        if (pos != string::npos) args = args.substr(pos);
        print_while(args);
        return;
    }
    if (input == "\\q") return; // 3.
    if (input == "history") { // 4.
        history();
        return;
    }
    if (input.find(ECHO) == 0) { // 5.
        string args = input.substr(ECHO.length());
        size_t pos = args.find_first_not_of(" ");
        if (pos != string::npos) args = args.substr(pos);
        echo(args);
        return;
    }
    if (input.find("\\e") == 0) { // 7.
        string args = input.substr(2);
        size_t pos = args.find_first_not_of(" ");
        if (pos != string::npos) args = args.substr(pos);
        env(args);
        return;
    }
    cout << "Command is not found\n"; // 6.
}

int main() {
    history_path = get_history_path();
    string input;
    while (true) {
        cout << "kubsh > ";
        if (!getline(cin, input)) break; // Ctrl+D
        if (input.empty()) continue;
        add_to_history(input);
        process_command(input);
        if (input.find("\\p") == 0 || input == "\\q") break;
    }
    cout << ":(\n";
    return 0;
}


