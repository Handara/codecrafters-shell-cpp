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

void remove_trailing_spaces(std::string& string){

  // remove trailing spaces from start
  while (string.size() != 0 && string.at(0) == ' '){
    string.erase(0,1);
  }

  //remove trailing spaces from end
  while (string.size() != 0 && string.at(string.size() - 1) == ' '){
    string.erase(string.size()-1,1);
  }

}

// split string into args and strip trailing whitespace from args
std::vector<std::string> split_string(std::string to_split, char token){
  
  std::vector<std::string> split_array;
  size_t start = 0;
  size_t end = 0;
  while(start < to_split.size()){
    //  next occurence of token
    end = to_split.find(token,start);
    // if no occurence found, force end to be end of string.
    if (end == std::string::npos)end = to_split.size();
    // extract arg
    std::string stripped_arg = to_split.substr(start,end-start);
    // remove trailing whitespaces from the arg
    remove_trailing_spaces(stripped_arg);
    // if arg is empty string, ignore
    if (!stripped_arg.empty())split_array.push_back(stripped_arg);
    // start from next position for next iteration
    start = end + 1;
  }

  return split_array;

}

/* Command methods */

void echo_command(std::string& string){
    bool is_inside_quotes = false;
    bool has_space = false;
    size_t pos = 0;    
    std::string current_word = "";
    char current_char;
    for (size_t i = 0; i < string.size(); i++){
        current_char = string.at(i);
        if (current_char == '\''){
            is_inside_quotes = !is_inside_quotes;
            has_space = false;
            continue;
        }
        if (is_inside_quotes){
            current_word += current_char;
        }else{
            if (!has_space && current_char == ' '){
                current_word += ' ';
                has_space = true;
            }else if(current_char != ' '){
                current_word += current_char;
                has_space = false;
            }
        }
    }
    std::cout << current_word << std::endl;
}


void main_loop(){
  std::string command;
  std::string first_command;
  std::string args[] = {};
  std::string builtins[] = {"exit","echo","type", "pwd"};
  while(true){
    std::cout << "$ ";
    std::getline(std::cin, command);

    
    std::vector<std::string> args = split_string(command,' ');

    if (command=="exit"){
      break;
    }else if(args[0] == "echo"){
      std::string echo_args = command.substr(5);
      echo_command(echo_args); 
    }else if(first_command == "pwd"){
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
      std::string first_command = command.substr(start, end);
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
        for (auto& s : args) argv.push_back(s.data());
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
        std::cout << command.substr(0) << ": command not found\n";
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
