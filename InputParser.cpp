#include "InputParser.h"

InputParser::InputParser(int &argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        this->arguments.push_back(std::string(argv[i]));
    }
}

const std::string& InputParser::getCmdOption(const std::string &option) const {
    std::vector<std::string>::const_iterator itr = find(this->arguments.begin(), this->arguments.end(), option);
    if (itr != this->arguments.end() && ++itr != this->arguments.end()){
        return *itr;
    }
    static const std::string empty_string("");
    return empty_string;
}

bool InputParser::cmdOptionExists(const std::string &option) const{
    return find(this->arguments.begin(), this->arguments.end(), option) != this->arguments.end();
}

/*Checking for correctness of arguments. Multiple bind addresses or port numbers are incorrect*/
bool InputParser::checkForMultipleOptions() const {
    return (
    count(this->arguments.begin(), this->arguments.end(), "-b") >1 ||
    count(this->arguments.begin(), this->arguments.end(), "-p") > 1);
}

/*Checking for other arguments than -b, -p, -a, -r */
bool InputParser::hasUnexpectedArgs() const {
    std::vector<std::string> validFlags = {"-p", "-b", "-r", "-a"};
    for (size_t i = 0; i < arguments.size(); ++i) {
      const std::string& arg = arguments[i];

      if (arg[0] == '-') {
        if (std::find(validFlags.begin(), validFlags.end(), arg) == validFlags.end()) {
            return true;
        }
        i++; //the flag is valid, skipping one char
      }
      else if (i == 0 || arguments[i-1][0] != '-' ||
          std::find(validFlags.begin(), validFlags.end(), arguments[i-1]) == validFlags.end()) {
            return true; //argument is not following a valid flag
      }
  }
  return false;
}