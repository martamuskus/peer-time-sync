#ifndef INPUTPARSER_H
#define INPUTPARSER_H

#include <bits/stdc++.h>

class InputParser{
public:
    InputParser (int &argc, char **argv);

    const std::string& getCmdOption(const std::string &option) const;

    bool cmdOptionExists(const std::string &option) const;
    bool checkForMultipleOptions() const;
    bool hasUnexpectedArgs() const;

private:
    std::vector <std::string> arguments;
};


#endif //INPUTPARSER_H
