#include <string>
#include <sstream>

std::string uniqueName(const std::string& base) {
    static int counter = 0;
    std::ostringstream oss;
    oss << base << "." << counter++;
    return oss.str();
}
