#include <iostream>
#include <unistd.h>   // fork, execvp, chdir, dup2
#include <sys/wait.h> // waitpid
#include <vector>
#include <string>
#include <sstream>
#include <fcntl.h>    // open
#include <cstring>    // strerror

void print_prompt() {
    std::cout << "mysh> " << std::flush;
}

std::vector<std::string> parse_command(const std::string& input) {
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

int main() {
    while (true) {
        print_prompt();

        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cout << std::endl;
            break; // EOF (Ctrl+D)
        }

        if (input.empty()) {
            continue;
        }

        auto args = parse_command(input);
        if (args.empty()) {
            continue;
        }

        // Built-in exit
        if (args[0] == "exit") {
            break;
        }

        // Built-in cd
        if (args[0] == "cd") {
            if (args.size() == 1) {
                const char* home = getenv("HOME");
                if (home == nullptr) {
                    std::cerr << "mysh: HOME environment variable not set\n";
                } else if (chdir(home) != 0) {
                    perror("mysh");
                }
            } else {
                if (chdir(args[1].c_str()) != 0) {
                    perror("mysh");
                }
            }
            continue;
        }

        // Background check
        bool is_background = false;
        if (args.back() == "&") {
            is_background = true;
            args.pop_back();
        }

        // Variables for redirection files
        std::string input_file;
        std::string output_file;

        // Parse redirection symbols and remove them from args
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "<") {
                if (i + 1 < args.size()) {
                    input_file = args[i + 1];
                    args.erase(args.begin() + i, args.begin() + i + 2);
                    i -= 1; // adjust index after erase
                } else {
                    std::cerr << "mysh: syntax error near unexpected token `newline'\n";
                    input_file.clear();
                    break;
                }
            } else if (args[i] == ">") {
                if (i + 1 < args.size()) {
                    output_file = args[i + 1];
                    args.erase(args.begin() + i, args.begin() + i + 2);
                    i -= 1; // adjust index after erase
                } else {
                    std::cerr << "mysh: syntax error near unexpected token `newline'\n";
                    output_file.clear();
                    break;
                }
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }

        if (pid == 0) {
            // Child process

            // Handle input redirection
            if (!input_file.empty()) {
                int fd_in = open(input_file.c_str(), O_RDONLY);
                if (fd_in < 0) {
                    std::cerr << "mysh: cannot open input file '" << input_file << "': " << strerror(errno) << std::endl;
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd_in, STDIN_FILENO) < 0) {
                    perror("dup2 input");
                    close(fd_in);
                    exit(EXIT_FAILURE);
                }
                close(fd_in);
            }

            // Handle output redirection
            if (!output_file.empty()) {
                int fd_out = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    std::cerr << "mysh: cannot open output file '" << output_file << "': " << strerror(errno) << std::endl;
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd_out, STDOUT_FILENO) < 0) {
                    perror("dup2 output");
                    close(fd_out);
                    exit(EXIT_FAILURE);
                }
                close(fd_out);
            }

            // Prepare argv
            std::vector<char*> c_args;
            for (auto& arg : args) {
                c_args.push_back(const_cast<char*>(arg.c_str()));
            }
            c_args.push_back(nullptr);

            if (execvp(c_args[0], c_args.data()) == -1) {
                perror("mysh");
                exit(EXIT_FAILURE);
            }
        } else {
            // Parent process
            if (!is_background) {
                int status;
                waitpid(pid, &status, 0);
            } else {
                std::cout << "Started background process with PID: " << pid << std::endl;
            }
        }
    }

    return 0;
}

