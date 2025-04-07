#include "InputParser.h"

InputParser::InputParser(int &argc, char **argv) {
    for (int i = 1; i < argc; ++i)
        this->arguments.push_back(std::string(argv[i]));
}


[[nodiscard]] const std::string& InputParser::getCmdOption(const std::string &option) const {
        std::vector<std::string>::const_iterator itr;
        itr =  find(this->arguments.begin(), this->arguments.end(), option);
        if (itr != this->arguments.end() && ++itr != this->arguments.end()){
            return *itr;
        }

        static const std::string empty_string("");
        return empty_string;
}

[[nodiscard]] bool InputParser::cmdOptionExists(const std::string &option) const{
        return find(this->arguments.begin(), this->arguments.end(), option)
               != this->arguments.end();
}