#include <iostream>
#include <string>
#include <unistd.h>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <sys/wait.h>



/** HELPERS  **/ 


bool is_string_included_in_array(std::string *haystack, std::string needle, size_t size){
  for(int i = 0; i < size; i++){
    if (needle == haystack[i]){
      return true;
    }
  }
  return false;
}

std::string find_executable_in_path(std::string executable){
  std::string path_var = std::getenv("PATH");
  size_t start = 0;
  size_t end = 0;
  std::string curr_path = path_var.substr(start,end-start) + "/" + executable;
  while (start < path_var.size()){
    end = path_var.find(':', start);
    curr_path = path_var.substr(start,end-start) + "/" + executable;
    if(access(curr_path.c_str(),X_OK)==0){
      return curr_path;
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }

  return "";

}


/* Command methods */

std::vector<std::string> parse_arguments(std::string string){
    enum QuoteState { NONE, SINGLE, DOUBLE};
    QuoteState state = NONE;
    bool has_space = false;
    bool is_escaped = false;
    size_t pos = 0;    
    std::string current_word = "";
    char current_char;
    std::vector<std::string> args;
    for (size_t i = 0; i < string.size(); i++){
        current_char = string.at(i);
        if (is_escaped && state == NONE){
            current_word+=current_char;
            is_escaped = false;
            continue;
        }else if (is_escaped && state == DOUBLE){
            if (std::string("$\"\\\n").find(current_char) != std::string::npos){
                current_word += current_char;
            }else{
                current_word += '\\';
                current_word += current_char;
            }
            is_escaped = false;
            continue;
        }

        if (current_char == '\\' && state != SINGLE){
            is_escaped = true; 
            continue;
        }else if (current_char == '\''){
            switch (state)
            {
            case NONE:
                state = SINGLE;
                has_space = false;
                break;
            case SINGLE:
                state = NONE;
                has_space = false;
                break;
            case DOUBLE:
                current_word += '\'';
                break;
            }
            is_escaped=false;
            continue;
        }else if (current_char == '"'){
            switch (state)
            {
            case NONE:
                state = DOUBLE;
                has_space = false;
                break;
            case SINGLE:
                current_word += '"';
                break;
            case DOUBLE:
                state = NONE;
                has_space = false;
                break;
            }
            continue;
        }else{
            if (state != NONE){
                current_word += current_char;
            }else{
                if (!has_space && current_char == ' '){
                    if(!current_word.empty()) args.push_back(current_word);
                    current_word = "";
                    has_space = true;
                }else if(current_char != ' '){
                    current_word += current_char;
                    has_space = false;
                }
            }
        }
    }
    if(!current_word.empty())args.push_back(current_word);
    return args;
}


void main_loop(){
  std::string command;
  std::string first_command;
  std::string args[] = {};
  std::string builtins[] = {"exit","echo","type", "pwd"};
  while(true){
    std::cout << "$ ";
    std::getline(std::cin, command);

    std::vector<std::string> args = parse_arguments(command);
    if (args.empty()) continue;
    first_command = args[0];
    if (command=="exit"){
      break;
    }else if(first_command == "echo"){
        for (size_t i = 1; i < args.size(); i++){
            std::cout << args[i] ;
            if (i != args.size() - 1){
                std::cout << " " ;
            }
        }
        std::cout << std::endl;
    }
    else if(first_command == "pwd"){
      std::cout << std::filesystem::current_path().string() << std::endl;
    }else if (first_command == "cd") {
      std::string path = args[1];
      if (path == "~") {
          path = std::getenv("HOME");
      }

      try {
          std::filesystem::current_path(path);
      } catch (const std::filesystem::filesystem_error& e) {
          std::cout << "cd: " << path << ": No such file or directory\n";
      }
    }
    
    else if(first_command == "type"){
      std::string subcommand_type = command.substr(5);
      if (is_string_included_in_array(builtins,subcommand_type,builtins->size())){
        std::cout << subcommand_type << " is a shell builtin\n"; 
      }else if(std::string found = find_executable_in_path(subcommand_type); !found.empty()){

        std::cout << subcommand_type << " is " << found << "\n";
      }else{
        std::cout << subcommand_type << ": not found\n";
      }
    }
    else {
      size_t start = 0;
      size_t end = command.find(' ', start);
      if(std::string found = find_executable_in_path(first_command); !found.empty()){
        size_t start = 0;
        size_t end = command.find(' ', start);
        std::vector<std::string> args;
        while(end != std::string::npos){
          end = command.find(' ', start);
          args.push_back(command.substr(start,end-start));
          start = end + 1;
        }
        std::vector<char *>argv;
        std::vector<std::string> parsed_args;
        if (first_command == "cat"){
            std::string cat_args = command.substr(4);
            parsed_args = parse_arguments(cat_args);
            argv.push_back(first_command.data());
            for (auto&arg : parsed_args)argv.push_back(arg.data());
        }else{
            for (auto& s : args) argv.push_back(s.data());
        }
        argv.push_back(nullptr);
        pid_t pid = fork();
        if (pid == 0) {
            // child process
            execv(found.c_str(), argv.data());
            exit(1);
        } else {
            // parent process 
            
            waitpid(pid, nullptr, 0);
        }
      }else{
        std::cout << first_command << ": command not found\n";
      }
    }
  }
}
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  main_loop();
  
}
