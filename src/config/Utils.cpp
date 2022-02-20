#include "Utils.hpp"
//TODO split value function both splits and saves the value in the vector. So, it's actually doing more than split. Adjusting the name?
void Utils::split_value(std::string line, std::vector<std::string> &vector_to_save)
{
    int i = line.find_first_not_of("     ;{");
    int found = line.find_first_of("     ;{", i);
    int len = line.length();
    while(found < len && found != -1)
    {
        std::string sub = line.substr(i, found - i);
        vector_to_save.push_back(sub);
        if(line[found] == ';')
            break;
        else
        {
            i = line.find_first_not_of("     ;{", found + 1);
            found = line.find_first_of("     ;{", i);
        }
    }
}

bool Utils::check_first_keyword(std::string line, std::string keyword)
{
	int first = line.find_first_not_of(" 	");
	if (first < 0)
		return false;
	int end = line.find_first_of(" 	;{", first + 1);
	if (line.substr(first, end - first).compare(keyword) == 0)
		return true;
	else
		return false;
}

void Utils::remove_first_keyword(std::string &line)
{
	int first = line.find_first_not_of("    ");
	int end = line.find_first_of("  ", first + 1);
	line.erase(first, end - first);
}

void Utils::remove_white_space(std::string &temp)
{
	temp.erase(std::remove_if(temp.begin(), temp.end(), ::isspace), temp.end());
}