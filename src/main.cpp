#include <iostream>
#include <string>
#include <unistd.h>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <map>
#include <array>



namespace fs = std::filesystem;

/** GLOBAL VARIABLE **/

static std::map<std::string, std::string> complete_map;

/** CLASS **/
struct Jobs
{
  size_t jobNumber;
  pid_t pid;
  std::string command;
  enum {RUNNING, JOBDONE} status;
  
  Jobs print(){
      std::cout << "[" << jobNumber << "] " << pid << "\n";
      return *this;
  }
  void list_print(size_t job_numbers){
    switch (status)
    {
    case RUNNING:
      std::cout << "[" << jobNumber << "]";
      if (job_numbers == jobNumber) std::cout << "+";
      else if (job_numbers - 1 == jobNumber) std::cout << "-";
      std::cout << "  Running                 " << command << "\n";
      break;
    
    default:
      break;
    }
  }

};



/** HELPERS  **/ 

bool is_string_included_in_array(std::vector<std::string> haystack, std::string needle){
  for(int i = 0; i < haystack.size(); i++){
    if (needle == haystack[i]){
      return true;
    }
  }
  return false;
}

std::vector<std::string> exec_completion_from_path(std::string text){
  std::string path_var = std::getenv("PATH");
  size_t start = 0;
  size_t end = 0;
  std::string curr_path = path_var.substr(start,end-start) + "/";
  std::vector<std::string> results = {};
  while (end != std::string::npos){
    end = path_var.find(':', start);
    curr_path = path_var.substr(start,end-start);
    std::vector<std::string> files;
    if (fs::exists(curr_path) && fs::is_directory(curr_path)) {
      for (const auto& entry : fs::directory_iterator(curr_path)) {
        std::string filename = entry.path().filename().string();
         if (filename.find(text) == 0){
          results.push_back(filename);
        }
      }
    }
    start = end + 1;
  }

  return results;

}

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

std::vector<std::string> wd_completions(std::string text){
  std::vector<std::string> results = {};
  std::string dir = "";
  std::string file = text;
  size_t start = 0;
  size_t end = text.find('/');
  while (end != std::string::npos){
    start = end + 1;
    end = text.find('/',start);
  }
  if (start != 0){
    file = text.substr(start,text.size());
    dir = text.substr(0,start);
    
  }

  if (fs::exists(fs::current_path()/dir) && fs::is_directory(fs::current_path()/dir)) {
    for (const auto& entry : fs::directory_iterator(fs::current_path()/dir)) {
      std::string filename = entry.path().filename().string();
        if (filename.find(file) == 0){
          if (entry.is_directory()) {
            filename += "/";
          }
        results.push_back(dir  + filename);
      }
    }
  }
  
    return results;

}

char* words_generator(const char* text, int state){
  static std::vector<std::string> path_executable = {};
  static std::vector<std::string> wd_files = {};
  static size_t list_range;
  static std::vector<std::string> results;
  static size_t builtin_range = 2;
  static size_t path_range;
  static size_t wd_range;
  std::string strtext = text;
  if (state == 5) return nullptr;
  if (state == 0){
    path_executable = exec_completion_from_path(text);
    wd_files = wd_completions(text);
    results = {};
    list_range = 0;
    path_range = path_executable.size();
    wd_range = wd_files.size();
  }

  while (list_range < builtin_range){
      std::vector<std::string> to_match = {"echo","exit"};
      if(to_match[list_range].find(text) == 0){
        results.push_back(to_match[list_range]);
        return strdup(results.at(state).c_str());
      }
      list_range++;
  } 
  
  while (list_range < builtin_range + wd_range){
    if(wd_files[list_range - builtin_range].find(text) == 0){
      results.push_back(wd_files.at(list_range++ - builtin_range));
      return strdup(results.at(state).c_str());
    }
    list_range++;
  } 

  while (list_range < builtin_range + path_range + wd_range){
    if(path_executable[list_range - builtin_range - wd_range].find(text) == 0){
      results.push_back(path_executable.at(list_range++ - builtin_range - wd_range));
      return strdup(results.at(state).c_str());
    }
    list_range++;
  } 
  return nullptr;
  
}

char* arguments_generator(const char* text, int state){
  static std::vector<std::string> wd_files = {};
  static std::vector<std::string> script_completions = {};
  static size_t list_range;
  static std::vector<std::string> results;
  static size_t wd_range;
  static size_t script_completions_range;
  std::string completion_buffer = rl_line_buffer;
  std::string size_buffer = std::to_string(completion_buffer.size());

  std::vector<std::string> arguments = parse_arguments(completion_buffer);
  std::string completion_first = arguments.at(0);
  setenv("COMP_LINE",completion_buffer.c_str(),1);
  setenv("COMP_POINT",size_buffer.c_str(),1);
  if (state == 5) return nullptr;
  if (state == 0){
    if(!complete_map.empty() && complete_map.find(completion_first) != complete_map.end()){
    std::array<char, 128> buffer;
    std::string result;
    std::string prev_word = "";
    list_range = 0;
    script_completions = {};
    if (std::string(text).empty()) {
        prev_word = arguments.back();
    } else if (arguments.size() >= 2) {
        prev_word = arguments[arguments.size() - 2];
    }
    std::string exec_cmd = complete_map.at(completion_first) + " '" + completion_first + "' '" + text + "' '" + prev_word + "'";
    FILE* pipe = popen(exec_cmd.c_str(),"r");
    if (!pipe){
      pclose(pipe);
    }else{
      while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                  
                  result += buffer.data();
                  if (!result.empty() && result.back() == '\n') {
                    result.pop_back();
                  }
                  script_completions.push_back(result);
                  result = "";
                }
      pclose(pipe);
      script_completions_range = script_completions.size();
        
      while (list_range < script_completions_range) {
        if(script_completions[list_range].find(text) == 0){
          return strdup(script_completions.at(list_range++).c_str());
        }
        list_range++;
      }
      return nullptr;
      
    }
    }
    wd_files = wd_completions(text);
    results = {};
    list_range = 0;
    wd_range = wd_files.size();
  }else{
    while (list_range <  script_completions_range){
    if(script_completions[list_range].find(text) == 0){
      return strdup(script_completions.at(list_range++).c_str());
    }
    
    list_range++;
    } 
    while (list_range <  wd_range + script_completions_range){
    if(wd_files[list_range].find(text) == 0){
      results.push_back(wd_files.at(list_range++));
      return strdup(results.at(state).c_str());
    }
    
    list_range++;
    } 
  }
  return nullptr;
}

char **custom_auto_complete_function(const char* text, int start, int end){

  rl_attempted_completion_over = 0;
  char **result;
  if (start == 0)result = rl_completion_matches(text, words_generator);
  else{
    result = rl_completion_matches(text, arguments_generator);
  } 
  if (result == nullptr) std::cout<<"\a";
  else{
    std::string match = result[0];
    if (!match.empty() && match.back()=='/')rl_completion_append_character = '\0';
    else rl_completion_append_character = ' ';
  }
  return result;
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



/* MAIN LOOP */

void main_loop(){
  std::string command;
  std::string first_command;
  std::string args[] = {};
  std::size_t job_number = 0;
  std::vector<std::string> builtins = {"exit","echo","type", "pwd", "complete", "jobs"};
  rl_bind_key('\t', rl_complete);
  std::vector<Jobs> jobsList;
  rl_attempted_completion_function = custom_auto_complete_function;
  while(true){
    char* input = readline("$ ");
    command = input;
    bool is_job = false;
    free(input);
    std::vector<std::string> args = parse_arguments(command);
    
    if (args.empty()) continue;
    
    enum STDRedirectType{NONE, STDOUT_REDIR, STDERR_REDIR};
    STDRedirectType redir_type = NONE;

    first_command = args[0];
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);

    is_job = args.at(args.size()-1) == "&";
    if (is_job){
      std::string command = "";
      for (auto& a:args) command += a;
      args.pop_back();
      job_number++;
    } 
    for (size_t i = 0; i < args.size(); i++){
      
      if ((args[i] == ">" || args[i] == "1>")&&(i + 1 < args.size())){
        std::string redirect_file = args[i+1];
        int fd = open(redirect_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd != -1){
          dup2(fd, STDOUT_FILENO);
          close(fd);
          redir_type = STDOUT_REDIR;
        }
        args.erase(args.begin()+i, args.end());
        break;
      }else if ((args[i] == ">>" || args[i] == "1>>")&&(i + 1 < args.size())){
        std::string redirect_file = args[i+1];
        int fd = open(redirect_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd != -1){
          dup2(fd, STDOUT_FILENO);
          close(fd);
          redir_type = STDOUT_REDIR;
        }
        args.erase(args.begin()+i, args.end());
        break;
      }else if ((args[i] == "2>")&&(i + 1 < args.size())){
        std::string redirect_file = args[i+1];
        int fd = open(redirect_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd != -1){
          dup2(fd, STDERR_FILENO);
          close(fd);
          redir_type = STDERR_REDIR;
        }
        args.erase(args.begin()+i, args.end());
        break;
      }else if ((args[i] == "2>>")&&(i + 1 < args.size())){
        std::string redirect_file = args[i+1];
        int fd = open(redirect_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd != -1){
          dup2(fd, STDERR_FILENO);
          close(fd);
          redir_type = STDERR_REDIR;
        }
        args.erase(args.begin()+i, args.end());
        break;
      }
    }



    if (first_command=="exit"){
      break;
    }else if(first_command == "echo"){
        for (size_t i = 1; i < args.size(); i++){
            std::cout << args[i] ;
            if (i != args.size() - 1){
                std::cout << " " ;
            }
        }
        std::cout << std::endl;
    }else if(first_command == "jobs"){
      for (auto& job : jobsList)job.list_print(job_number);
    }
    else if(first_command == "pwd"){
      std::cout << fs::current_path().string() << std::endl;
    }else if (first_command == "cd") {
      std::string path = args[1];
      if (path == "~") {
          path = std::getenv("HOME");
      }

      try {
          fs::current_path(path);
      } catch (const fs::filesystem_error& e) {
          std::cout << "cd: " << path << ": No such file or directory\n";
      }
    }
    
    else if(first_command == "type"){
      std::string subcommand_type = args[1];
      if (is_string_included_in_array(builtins,subcommand_type)){
        std::cout << subcommand_type << " is a shell builtin\n"; 
      }else if(std::string found = find_executable_in_path(subcommand_type); !found.empty()){

        std::cout << subcommand_type << " is " << found << "\n";
      }else{
        std::cout << subcommand_type << ": not found\n";
      }
    }else if(first_command == "complete"){
      std::string subcommand_type = args[1];
      args.erase(args.begin(), args.begin()+2);
      if (subcommand_type == "-C" && args.size()==2){
        complete_map[args[1]] = args[0];
      }else if (subcommand_type == "-r" && args.size()==1 && complete_map.find(args.at(0)) != complete_map.end()){
        complete_map.erase(args.at(0));
      }
      else if(subcommand_type == "-p"){
        if (complete_map.find(args[0]) == complete_map.end()){
          std::cout << "complete: ";
          for (auto& s : args) std::cout << s;
          std::cout << ": no completion specification\n";
        }else{
          std::cout << "complete -C '" << complete_map[args[0]] << "' " << args[0] << "\n";
        }
      }
      
    }
    else {
      if(std::string found = find_executable_in_path(first_command); !found.empty()){
        
        std::vector<char *>argv;
        for (auto& s : args)argv.push_back(s.data());
        argv.push_back(nullptr);
        pid_t pid = fork();
        if (pid == 0) {
            // child process
            execv(found.c_str(), argv.data());
            exit(1);
        } else {
            // parent process 
            if (is_job) {
              Jobs job{job_number, pid, command, Jobs::RUNNING};
              jobsList.push_back(job.print());
            }
            else waitpid(pid, nullptr, 0);
        }
      }else{
        std::cout << first_command << ": command not found\n";
      }
    }
    switch (redir_type)
    {
    case STDOUT_REDIR:
      dup2(saved_stdout, STDOUT_FILENO);
      break;
      case STDERR_REDIR:
      dup2(saved_stderr, STDERR_FILENO);
      break;
      default:
      break;
    }
    close(saved_stdout);
    close(saved_stderr);

  }
}
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  main_loop();
  
}
