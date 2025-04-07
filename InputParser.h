#ifndef INPUTPARSER_H
#define INPUTPARSER_H

#include <bits/stdc++.h>

class InputParser{
public:
    InputParser (int &argc, char **argv);

    [[nodiscard]] const std::string& getCmdOption(const std::string &option) const;

    [[nodiscard]] bool cmdOptionExists(const std::string &option) const;

private:
    std::vector <std::string> arguments;
};


#endif //INPUTPARSER_H
