#include <iostream>
#include <string>


bool is_string_included_in_array(std::string *haystack, std::string needle, size_t size){
  for(int i = 0; i < size; i++){
    if (needle == haystack[i]){
      return true;
    }
  }
  return false;
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
      } else{
        std::cout << subcommand_type << ": command not found\n";
      }
    }
    else {
      std::cout << command << ": command not found\n";
    }
  }
}
