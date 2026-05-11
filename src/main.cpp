#include <iostream>
#include <string>
#include <unistd.h>

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
  size_t end = path_var.find(':', start);
  std::string curr_path = path_var.substr(start,end-start) + "/" + executable;
  while (end != std::string::npos){
    start = end + 1;
    end = path_var.find(':', start);
    curr_path = path_var.substr(start,end-start) + "/" + executable;
    if(access(curr_path.c_str(),X_OK)==0){
      return curr_path;
    }
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
        std::cout << subcommand_type << ": found\n";
      }
    }
    else {
      std::cout << command << ": command not found\n";
    }
  }
}
