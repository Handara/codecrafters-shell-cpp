#include <iostream>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <vector>
#include <sys/wait.h>
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

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::string command;
  std::string builtins[] = {"exit","echo","type"};
  while(true){
    std::cout << "$ ";
    std::getline(std::cin, command);
    if (command=="exit"){
      break;
    }else if(command.substr(0,5) == "echo "){
      std::cout << command.substr(5) << std::endl;
    }else if(command.substr(0,5) == "type "){
      std::string subcommand_type = command.substr(5);
      
      
      if (is_string_included_in_array(builtins,subcommand_type,3)){
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
        std::cout << command << ": command not found\n";
      }
    }
  }
}
