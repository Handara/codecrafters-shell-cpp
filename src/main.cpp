#include <iostream>
#include <string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::string command;
  while(true){
    std::cout << "$ ";
    std::getline(std::cin, command);
    if (command=="exit"){
      break;
    }
    std::cout << command << ": command not found\n";
  }
}
