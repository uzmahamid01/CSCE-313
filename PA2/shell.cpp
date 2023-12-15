#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <sys/param.h>
#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main() {
    //create copies of stdin/stdout -- using dup()
    int original_stdin = dup(0);
    int original_stdout = dup(1);

    // Variable to store previous working directory
    char prevWorkingDir[PATH_MAX];
    //Vector to store background process PIDs
    vector<pid_t> background_pids;

    if (getcwd(prevWorkingDir, sizeof(prevWorkingDir)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    for (;;) {

        //implement date/time with TODO
        time_t now = time(0);
        struct tm current_time;
        char timestamp[80];
        current_time = *localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-d %H:%M:%S", &current_time);

        //implement username with getlogin()
        //struct passwd* pw = getpwuid(getuid());
        //const char* username = pw->pw_name;
        string username = getlogin();

        //implement curdir with getcwd()
        char current_dir[PATH_MAX];
        if (getcwd(current_dir, sizeof(current_dir)) != NULL) {
            // Successfully obtained the current working directory
        } else {
            perror("getcwd");
            exit(EXIT_FAILURE);
        }

        // need date/time, username, and absolute path to current dir
        cout << "Shell " << timestamp << " " << username << ":" << current_dir << "$ ";

        string input;
        getline(cin, input);

        if (input == "exit") {
            cout << "Now exiting shell..." << endl << "Goodbye" << endl;
            break;
        }

        //add check for bg process -- add pid to vector if bg and dont waitpid() in parent process
        //boolean

        bool is_background = false;
        if (!input.empty() && input[input.length() - 1] == '&') {
            is_background = true;
            input.pop_back();
        }
        //input before tokenization
        //cout << "Input: " << input << endl;


        //chdir()   change directory
        //if dir (cd <dir>) is "-", then go to the previous working dorectory
        //variable storung previous working directory (it needs to be declared outside loop)
        if (input == "cd") {
            // Handle "cd" command without forking a new process
            if (chdir(getenv("HOME")) == -1) {
                perror("chdir");
            } else {
                // Save the previous working directory
                strcpy(prevWorkingDir, current_dir);
            }
        } else if (input == "cd -") {
            // Handle "cd -" command without forking a new process
            if (prevWorkingDir[0] != '\0') {
                if (chdir(prevWorkingDir) == -1) {
                    perror("chdir");
                }
            } else {
                cerr << "Previous working directory is not set.\n";
            }
        } else if (input.compare(0, 3, "cd ") == 0) {
            char* newDir = &input[3];
            if (chdir(newDir) == -1) {
                perror("chdir");
            } else {
                // Save the previous working directory
                strcpy(prevWorkingDir, current_dir);
            }
        } else {
            // Tokenize the user input
            Tokenizer tknr(input);
            //parsed commands and associated input/output files
            for (size_t i = 0; i < tknr.commands.size(); ++i) {
                cout << "Command " << i << ": ";
                for (const string& arg : tknr.commands[i]->args) {
                    cout << arg << " ";
                }
                //cout << "Input file: " << tknr.commands[i]->in_file << " Output file: " << tknr.commands[i]->out_file << endl;
            }

            // Check if a single command or piped commands
            if (tknr.commands.size() == 1) {
                pid_t pid = fork();
                if (pid < 0) {
                    perror("fork");
                    exit(2);
                }

                if (pid == 0) {  // Child process
                    // Implement checks for I/O redirection and handle them using dup2

                    string input_file = tknr.commands[0]->in_file;
                    string output_file = tknr.commands[0]->out_file;

                    if (!input_file.empty()) {
                        int in_fd = open(input_file.c_str(), O_RDONLY);
                        if (in_fd == -1) {
                            perror("open");
                            exit(2);
                        }
                        dup2(in_fd, 0);
                        close(in_fd);
                    }

                    if (!output_file.empty()) {
                        int out_fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                        if (out_fd == -1) {
                            perror("open");
                            exit(2);
                        }
                        dup2(out_fd, 1);
                        close(out_fd);
                    }

                    ////implement multiple arguments -- iterate over args of current command to make 
                        //char * array
                    vector<char*> args;
                    for (const string& arg : tknr.commands[0]->args) {
                        args.push_back(const_cast<char*>(arg.c_str()));
                    }
                    args.push_back(nullptr);

                    //execute the command in the child process
                    execvp(args[0], args.data());
                    perror("execvp");
                    exit(2);
                } else {  // Parent process
                    if (!is_background) {
                        int status = 0;
                        waitpid(pid, &status, 0);
                        if (status > 1) {
                            exit(status);
                        }
                    }
                }
            } else {
                int prev_pipe_fd[2] = {-1, -1};
                int next_pipe_fd[2] = {-1, -1};

                for (size_t i = 0; i < tknr.commands.size(); ++i) {
                    if (i < tknr.commands.size() - 1) {
                        if (pipe(next_pipe_fd) == -1) {
                            perror("pipe");
                            exit(2);
                        }
                    }

                    pid_t pid = fork();
                    if (pid < 0) {
                        perror("fork");
                        exit(2);
                    }

                    if (pid == 0) {  // Child process
                        if (i < tknr.commands.size() - 1) {
                            close(next_pipe_fd[0]);
                            dup2(next_pipe_fd[1], 1);
                            close(next_pipe_fd[1]);
                        }

                        if (i > 0) {
                            close(prev_pipe_fd[1]);
                            dup2(prev_pipe_fd[0], 0);
                            close(prev_pipe_fd[0]);
                        }

                        // implement checks for I/O redirection
                        if (i == 0) {
                            string input_file = tknr.commands[0]->in_file;
                            if (!input_file.empty()) {
                                int in_fd = open(input_file.c_str(), O_RDONLY);
                                if (in_fd == -1) {
                                    perror("open");
                                    exit(2);
                                }
                                dup2(in_fd, 0);
                                close(in_fd);
                            }
                        }

                        if (i == tknr.commands.size() - 1) {
                            string output_file = tknr.commands[i]->out_file;
                            if (!output_file.empty()) {
                                int out_fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                                if (out_fd == -1) {
                                    perror("open");
                                    exit(2);
                                }
                                dup2(out_fd, 1);
                                close(out_fd);
                            }
                        }

                        //implement multiple arguments -- iterate over args of current command to make 
                        //char * array
                        vector<char*> args;
                        for (const string& arg : tknr.commands[i]->args) {
                            args.push_back(const_cast<char*>(arg.c_str()));
                        }
                        args.push_back(nullptr);

                        //execute the command in the child process
                        execvp(args[0], args.data());
                        perror("execvp");
                        exit(2);
                    } else {  // Parent process
                        if (!is_background) {
                            // Close pipe file descriptors in the parent
                            if (prev_pipe_fd[0] != -1) {
                                close(prev_pipe_fd[0]);
                            }
                            if (prev_pipe_fd[1] != -1) {
                                close(prev_pipe_fd[1]);
                            }

                            int status = 0;
                            waitpid(pid, &status, 0);
                            if (status > 1) {
                                exit(status);
                            }
                        }

                        //update previous pipe file descriptors
                        if (prev_pipe_fd[0] != -1) {
                            close(prev_pipe_fd[0]);
                        }
                        if (prev_pipe_fd[1] != -1) {
                            close(prev_pipe_fd[1]);
                        }
                        if (i < tknr.commands.size() - 1) {
                            prev_pipe_fd[0] = next_pipe_fd[0];
                            prev_pipe_fd[1] = next_pipe_fd[1];
                        }
                    }
                }
            }
        }
    }
    //restore stdin/stdout( variable would be outside the loop)
    dup2(original_stdin, 0);
    dup2(original_stdout, 1);

    return 0;
}
