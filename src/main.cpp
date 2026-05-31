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
#include <iomanip>
#include <fstream>


namespace fs = std::filesystem;

/** GLOBAL VARIABLE **/

static std::map<std::string, std::string> complete_map;
enum JobPosition {FIRST, SECOND, OTHER};
static size_t history_index = 0;
static size_t history_written = 0;
static std::vector<std::string> history;

/** CLASS **/
struct Jobs
{
  size_t jobNumber;
  pid_t pid;
  std::string command;
  int pidstatus;
  enum {RUNNING, DONE} status;
  
  Jobs& print(){
      std::cout << "[" << jobNumber << "] " << pid << "\n";
      return *this;
  }
  int8_t list_print(JobPosition position, bool print_running){
    pid_t result = waitpid(pid, &pidstatus, WNOHANG);
    if (result == pid){
      status = DONE;
      if (WIFEXITED(pidstatus)) {
          int exit_code = WEXITSTATUS(pidstatus);
      } 
      else if (WIFSIGNALED(pidstatus)) {
          int signal_number = WTERMSIG(pidstatus);
      }
    }

    switch (status)
    {
    case RUNNING:
      
      if (print_running){
        std::cout << "[" << jobNumber << "]";
        if (position == FIRST) std::cout << "+";
        else if (position == SECOND) std::cout << "-";
        std::cout << "  Running                 " << command << "\n";
      }
      return 0;
    
    case DONE:
      std::cout << "[" << jobNumber << "]";
      if (position == FIRST) std::cout << "+";
      else if (position == SECOND) std::cout << "-";
      std::cout << "  Done                 " << command.substr(0,command.size()-2) << "\n";
      return 1;
    }
    return 0;

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

void reap_jobs(std::vector<Jobs>& jobsList, size_t &job_number, bool print_running){
  size_t index = 0;
  for (auto it = jobsList.begin(); it != jobsList.end();){
    JobPosition position;
    if (it == jobsList.end() - 1) position = FIRST;
    else if (jobsList.size() >= 2 && it == jobsList.end() - 2) position = SECOND;
    else position = OTHER;
    int8_t job_code = it->list_print(position, print_running);
    if (job_code == 1){
        job_number--;
        it = jobsList.erase(it);
    }else{
      it++;
    }
  }
}

int custom_history_up(int count, int key){
   
  if (history_index >= history.size()) return 0;
  size_t startfrom = history.size() - ++history_index + 1;
  rl_replace_line(history.at(startfrom - 1).c_str(), 0);
  rl_point = 0;  
  rl_redisplay(); 
  return 0;
}

int custom_history_down(int count, int key){
   
  if (history_index <= 1) return 0;
  size_t startfrom = history.size() - --history_index + 1;
  rl_replace_line(history.at(startfrom - 1).c_str(), 0);
  rl_point = 0;  
  rl_redisplay(); 
  return 0;
}

/* MAIN LOOP */

void main_loop(){
  std::string command;
  std::string first_command;
  std::string args[] = {};
  std::size_t job_number = 0;
  std::size_t *job_ptr = &job_number;
  std::vector<std::string> builtins = {"exit","echo","type", "pwd", "complete", "jobs", "history"};
  rl_bind_key('\t', rl_complete);
  rl_bind_keyseq("\\e[A", custom_history_up);
  rl_bind_keyseq("\\e[B", custom_history_down); // Down arrow
  std::vector<Jobs> jobsList;
  rl_attempted_completion_function = custom_auto_complete_function;
  while (true) {
    bool should_reap = true;
    char* input = readline("$ ");
    command = input;
    bool is_job = false;
    free(input);
    std::vector<std::vector<std::string>> pipes; 
    std::vector<std::string> args = parse_arguments(command);
    std::vector<std::string> args_temp;

    history.push_back(command);
    history_index = 0;
    for (auto it = args.begin(); it != args.end(); it++){
      if (*it == "|"){
        pipes.push_back(args_temp);
        args_temp.clear();
      }else{
        args_temp.push_back(*it);
      }
    }
    pipes.push_back(args_temp);
    

    if (pipes.empty() || pipes[0].empty()) continue;

    size_t n = pipes.size();
    first_command = pipes[0][0];
    if (n == 1) {
      auto seg = pipes[0];
      enum STDRedirectType { NONE, STDOUT_REDIR, STDERR_REDIR };
      STDRedirectType redir_type = NONE;
      int saved_stdout = dup(STDOUT_FILENO);
      int saved_stderr = dup(STDERR_FILENO);
      for (size_t i = 0; i < seg.size(); i++) {
        if ((seg[i] == ">" || seg[i] == "1>") && i+1 < seg.size()) {
          int fd = open(seg[i+1].c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
          if (fd != -1) { dup2(fd, STDOUT_FILENO); close(fd); redir_type = STDOUT_REDIR; }
          seg.erase(seg.begin()+i, seg.end()); break;
        } else if ((seg[i] == ">>" || seg[i] == "1>>") && i+1 < seg.size()) {
          int fd = open(seg[i+1].c_str(), O_WRONLY|O_CREAT|O_APPEND, 0644);
          if (fd != -1) { dup2(fd, STDOUT_FILENO); close(fd); redir_type = STDOUT_REDIR; }
          seg.erase(seg.begin()+i, seg.end()); break;
        } else if (seg[i] == "2>" && i+1 < seg.size()) {
          int fd = open(seg[i+1].c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
          if (fd != -1) { dup2(fd, STDERR_FILENO); close(fd); redir_type = STDERR_REDIR; }
          seg.erase(seg.begin()+i, seg.end()); break;
        } else if (seg[i] == "2>>" && i+1 < seg.size()) {
          int fd = open(seg[i+1].c_str(), O_WRONLY|O_CREAT|O_APPEND, 0644);
          if (fd != -1) { dup2(fd, STDERR_FILENO); close(fd); redir_type = STDERR_REDIR; }
          seg.erase(seg.begin()+i, seg.end()); break;
        }
      }
      bool handled = true;
      if (first_command == "exit") {
        close(saved_stdout); close(saved_stderr);
        break;
      } else if (first_command == "history") {
        if (args.size() > 2 && args.at(1) == "-r"){
          std::ifstream file(args.at(2).c_str());
          std::string line;
          while (std::getline(file, line)) {
              history.push_back(line);
          }
          continue;
        }else if(args.size() > 2 && args.at(1) == "-w"){
          std::ofstream file(args.at(2).c_str());
          for (auto it = history.begin(); it != history.end(); it++)file << *it << "\n";
          continue;
        }else if(args.size() > 2 && args.at(1) == "-a"){
          std::ofstream file(args.at(2).c_str(),std::ios::app);
          for (size_t i = history_written; i < history.size(); i++) file << history[i] << "\n";
          history_written = history.size();
          continue;
        }
        size_t startfrom = history.size();
        if (args.size() == 2){
          try { startfrom = std::stoi(args.at(1)); }
          catch(const std::exception& e) { std::cerr << e.what() << '\n'; }
        }
        if (startfrom > history.size()) startfrom = history.size();
        size_t index = history.size() - startfrom + 1;
        for (auto it = history.end() - startfrom; it != history.end(); it++){
          std::cout << std::setw(5) << index++ << "  " << *it << "\n";
        }
      } else if (first_command == "jobs") {
        reap_jobs(jobsList, job_number, true);
        should_reap = false;
      } else if (first_command == "cd") {
        std::string path = seg.size() > 1 ? seg[1] : std::string(std::getenv("HOME"));
        if (path == "~") path = std::getenv("HOME");
        try { fs::current_path(path); }
        catch (const fs::filesystem_error&) {
          std::cout << "cd: " << path << ": No such file or directory\n";
        }
      } else if (first_command == "complete") {
        std::string subcommand_type = seg[1];
        seg.erase(seg.begin(), seg.begin()+2);
        if (subcommand_type == "-C" && seg.size() == 2) {
          complete_map[seg[1]] = seg[0];
        } else if (subcommand_type == "-r" && seg.size() == 1 && complete_map.count(seg[0])) {
          complete_map.erase(seg[0]);
        } else if (subcommand_type == "-p") {
          if (!complete_map.count(seg[0])) {
            std::cout << "complete: ";
            for (auto& s : seg) std::cout << s;
            std::cout << ": no completion specification\n";
          } else {
            std::cout << "complete -C '" << complete_map[seg[0]] << "' " << seg[0] << "\n";
          }
        }
      } else {
        handled = false;
      }
      switch (redir_type) {
        case STDOUT_REDIR: dup2(saved_stdout, STDOUT_FILENO); break;
        case STDERR_REDIR: dup2(saved_stderr, STDERR_FILENO); break;
        default: break;
      }
      close(saved_stdout);
      close(saved_stderr);

      if (handled) {
        if (should_reap) reap_jobs(jobsList, job_number, false);
        continue;
      }
    }
    {
      std::vector<std::array<int,2>> pipe_fds(n > 1 ? n-1 : 0);
      for (size_t i = 0; i < pipe_fds.size(); i++) pipe(pipe_fds[i].data());

      std::vector<pid_t> pids;
      for (size_t i = 0; i < n; i++) {
        auto seg = pipes[i];
        if (seg.empty()) continue;
        std::string cmd = seg[0];
        if (n == 1 && !seg.empty() && seg.back() == "&") {
          is_job = true;
          seg.pop_back();
          job_number++;
          should_reap = false;
        }

        pid_t pid = fork();
        if (pid == 0) {
          if (i > 0) dup2(pipe_fds[i-1][0], STDIN_FILENO);
          if (i < n-1) dup2(pipe_fds[i][1], STDOUT_FILENO);
          for (size_t j = 0; j < pipe_fds.size(); j++) {
            close(pipe_fds[j][0]);
            close(pipe_fds[j][1]);
          }
          for (size_t k = 0; k < seg.size(); k++) {
            if ((seg[k] == ">" || seg[k] == "1>") && k+1 < seg.size()) {
              int fd = open(seg[k+1].c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
              if (fd != -1) { dup2(fd, STDOUT_FILENO); close(fd); }
              seg.erase(seg.begin()+k, seg.end()); break;
            } else if ((seg[k] == ">>" || seg[k] == "1>>") && k+1 < seg.size()) {
              int fd = open(seg[k+1].c_str(), O_WRONLY|O_CREAT|O_APPEND, 0644);
              if (fd != -1) { dup2(fd, STDOUT_FILENO); close(fd); }
              seg.erase(seg.begin()+k, seg.end()); break;
            } else if (seg[k] == "2>" && k+1 < seg.size()) {
              int fd = open(seg[k+1].c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
              if (fd != -1) { dup2(fd, STDERR_FILENO); close(fd); }
              seg.erase(seg.begin()+k, seg.end()); break;
            } else if (seg[k] == "2>>" && k+1 < seg.size()) {
              int fd = open(seg[k+1].c_str(), O_WRONLY|O_CREAT|O_APPEND, 0644);
              if (fd != -1) { dup2(fd, STDERR_FILENO); close(fd); }
              seg.erase(seg.begin()+k, seg.end()); break;
            }
          }
          if (cmd == "echo") {
            for (size_t k = 1; k < seg.size(); k++) {
              std::cout << seg[k];
              if (k != seg.size()-1) std::cout << " ";
            }
            std::cout << "\n";
            exit(0);
          } else if (cmd == "pwd") {
            std::cout << fs::current_path().string() << "\n";
            exit(0);
          } else if (cmd == "type") {
            std::string sub = seg[1];
            if (is_string_included_in_array(builtins, sub))
              std::cout << sub << " is a shell builtin\n";
            else if (std::string found = find_executable_in_path(sub); !found.empty())
              std::cout << sub << " is " << found << "\n";
            else
              std::cout << sub << ": not found\n";
            exit(0);
          } else {
            std::string found = find_executable_in_path(cmd);
            if (found.empty()) {
              std::cerr << cmd << ": command not found\n";
              exit(1);
            }
            std::vector<char*> argv;
            for (auto& s : seg) argv.push_back(s.data());
            argv.push_back(nullptr);
            execv(found.c_str(), argv.data());
            exit(1);
          }
        }
        pids.push_back(pid);
      }

      for (size_t j = 0; j < pipe_fds.size(); j++) {
        close(pipe_fds[j][0]);
        close(pipe_fds[j][1]);
      }

      if (n == 1 && is_job && !pids.empty()) {
        Jobs job{job_number, pids[0], command, Jobs::RUNNING};
        jobsList.push_back(job.print());
      } else {
        for (pid_t pid : pids) waitpid(pid, nullptr, 0);
      }

      if (should_reap) reap_jobs(jobsList, job_number, false);
    }
  }
}
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  main_loop();
  
}
