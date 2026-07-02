#ifndef YASCRIPT_INTERFACE_HPP
#define YASCRIPT_INTERFACE_HPP

#include <iostream>
#include <string>

namespace yascript {

int runSource(const std::string &source, std::istream &input = std::cin, std::ostream &output = std::cout);
int runFile(const std::string &path, std::istream &input = std::cin, std::ostream &output = std::cout);

} // namespace yascript

#endif // YASCRIPT_INTERFACE_HPP
