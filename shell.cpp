#include <iostream>
#include <unistd.h>   // fork, execvp, chdir, dup2
#include <sys/wait.h> // waitpid
#include <vector>
#include <string>
#include <sstream>
#include <fcntl.h>    // open
#include <cstring>    // strerror
#include <csignal> // for signal handling
#include <readline/readline.h>    // <<< added
#include <readline/history.h>     // <<< added
#include <dirent.h>    // for listing directory contents

void sigint_handler(int signo) {
    std::cout << "\nmysh> " << std::flush;
}

void print_prompt() {
    std::cout << "mysh> " << std::flush;
}
std::string expand_variables(const std::string& input);  // <-- Forward declaration

std::vector<std::string> parse_command(const std::string& input) {
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
         token = expand_variables(token);  
        tokens.push_back(token);
    }
    return tokens;
}


std::string expand_variables(const std::string& input) {
    std::string result;
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '$') {
            i++;
            std::string var;
            while (i < input.size() && (isalnum(input[i]) || input[i] == '_')) {
                var += input[i++];
            }
            const char* val = getenv(var.c_str());
            if (val) {
                result += val;
            }
        } else {
            result += input[i++];
        }
    }
    return result;
}


std::vector<std::vector<std::string>> parse_pipeline(const std::string& input) {
    std::vector<std::vector<std::string>> commands;
    std::istringstream stream(input);
    std::string segment;

    while (std::getline(stream, segment, '|')) {
        std::istringstream cmdstream(segment);
        std::string token;
        std::vector<std::string> parts;
        while (cmdstream >> token) {
            parts.push_back(expand_variables(token));  // <-- here too

           
        }
        if (!parts.empty()) {
            commands.push_back(parts);
        }
    }
    return commands;
}

char** my_completion(const char* text, int start, int end);
char* command_generator(const char* text, int state);

        char* command_generator(const char* text, int state) {
    static DIR* dir;
    static struct dirent* ent;
    static std::string prefix;
    
    if (state == 0) {
        dir = opendir(".");
        prefix = text;
    }

    while ((ent = readdir(dir)) != nullptr) {
        if (strncmp(ent->d_name, prefix.c_str(), prefix.size()) == 0) {
            return strdup(ent->d_name);
        }
    }

    closedir(dir);
    return nullptr;
}

char** my_completion(const char* text, int start, int end) {
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, command_generator);
}
int main() {
    signal(SIGINT, sigint_handler);
    
    rl_attempted_completion_function = my_completion;

    while (true) {







      char* raw_input = readline("mysh> ");      // <<< added
if (!raw_input) break;                     // <<< added (Ctrl+D / EOF)
std::string input(raw_input);              // <<< added
free(raw_input);                           // <<< added
if (!input.empty()) add_history(input.c_str());  // <<< added

        if (input.empty()) continue;

        // Built-in: exit
        if (input == "exit") break;

        // Built-in: cd
        auto temp = parse_command(input);
        if (!temp.empty() && temp[0] == "cd") {
            if (temp.size() == 1) {
                const char* home = getenv("HOME");
                if (home) chdir(home);
                else std::cerr << "HOME not set\n";
            } else {
                if (chdir(temp[1].c_str()) != 0) perror("cd");
            }
            continue;
        }

        // ➤ Check for pipeline first
        auto pipeline = parse_pipeline(input);
        if (pipeline.size() > 1) {
            int num_cmds = pipeline.size();
            int prev_fd[2] = {-1, -1};

            for (int i = 0; i < num_cmds; ++i) {
                int pipe_fd[2];
                if (i < num_cmds - 1) {
                    if (pipe(pipe_fd) == -1) {
                        perror("pipe");
                        break;
                    }
                }

                pid_t pid = fork();
                if (pid == 0) {
                    signal(SIGINT, SIG_DFL);  
                    // CHILD
                    if (i > 0) {
                        dup2(prev_fd[0], STDIN_FILENO);
                        close(prev_fd[0]);
                        close(prev_fd[1]);
                    }

                    if (i < num_cmds - 1) {
                        close(pipe_fd[0]);
                        dup2(pipe_fd[1], STDOUT_FILENO);
                        close(pipe_fd[1]);
                    }

                    std::vector<char*> c_args;
                    for (auto& arg : pipeline[i]) c_args.push_back(&arg[0]);
                    c_args.push_back(nullptr);

                    execvp(c_args[0], c_args.data());
                    perror("execvp");
                    exit(1);
                }

                // PARENT
                if (i > 0) {
                    close(prev_fd[0]);
                    close(prev_fd[1]);
                }

                if (i < num_cmds - 1) {
                    prev_fd[0] = pipe_fd[0];
                    prev_fd[1] = pipe_fd[1];
                }
            }

            // Wait for all children
            for (int i = 0; i < num_cmds; ++i) {
                int status;
                wait(&status);
            }

            continue; // pipeline handled, go next iteration
        }

        // ➤ Regular command (redirection, background)
        auto args = parse_command(input);
        if (args.empty()) continue;

        bool is_background = false;
        std::string input_file, output_file;

        if (args.back() == "&") {
            is_background = true;
            args.pop_back();
        }

        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "<" && i + 1 < args.size()) {
                input_file = args[i + 1];
                args.erase(args.begin() + i, args.begin() + i + 2);
                i--;
            } else if (args[i] == ">" && i + 1 < args.size()) {
                output_file = args[i + 1];
                args.erase(args.begin() + i, args.begin() + i + 2);
                i--;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
             signal(SIGINT, SIG_DFL);       
            if (!input_file.empty()) {
                int fd_in = open(input_file.c_str(), O_RDONLY);
                if (fd_in < 0) {
                    perror("input file");
                    exit(1);
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }

            if (!output_file.empty()) {
                int fd_out = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    perror("output file");
                    exit(1);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }

            std::vector<char*> c_args;
            for (auto& arg : args) c_args.push_back(&arg[0]);
            c_args.push_back(nullptr);


            // Shell Script Handling (e.g. ./myscript.sh)
if (args[0].rfind("./", 0) == 0 || args[0].rfind("/", 0) == 0) {
    if (access(args[0].c_str(), F_OK) == 0) {
        std::vector<char*> script_args;
        script_args.push_back(const_cast<char*>("bash"));
        for (auto& arg : args) {
            script_args.push_back(const_cast<char*>(arg.c_str()));
        }
        script_args.push_back(nullptr);

        execvp("bash", script_args.data());
        perror("script exec");
        exit(1);
    }
}

            execvp(c_args[0], c_args.data());
            perror("execvp");
            exit(1);
        } else if (pid > 0 && !is_background) {
            int status;
            waitpid(pid, &status, 0);
        } else {
            std::cout << "Started background process with PID: " << pid << std::endl;
        }
    }

    return 0;
}
