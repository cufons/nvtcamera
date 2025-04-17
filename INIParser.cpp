//
// Created by cufon on 06.05.24.
//


#include <algorithm>
#include <iostream>
#include "INIParser.h"
#include "debug_info.h"

inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}
inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}
INIParser::INIParser() {
    lastSection = nullptr;
}

bool INIParser::load() {
    sections.clear();
    std::ifstream file(filePath);
    if(!file) return false;
    std::string curLine;
    unsigned linectr = 1;
    while(std::getline(file,curLine)) {
        if(!parseLine(curLine)) {
            DBG_INFO("In file " << filePath << ":" << linectr);
        }
        linectr++;
    }
    return true;
}

bool INIParser::parseLine(std::string &line) {
    ltrim(line);
    rtrim(line);
    if(line[0] == '#' || !line.size()) {
        // Skip comment or empty line
        return true;
    }
    if(line[0] == '[') {
        auto sectClosingPos = std::find(line.begin(), line.end(), ']');
        if(sectClosingPos == line.end()) {
            DBG_ERROR("Syntax Error: unterminated section statement.");
            return false;
        }

        std::string sectName(line.begin() + 1, sectClosingPos);
        std::transform(sectName.begin(),sectName.end(),sectName.begin(),[](char c) {return tolower(c);});
        sections.insert(std::make_pair(sectName,ConfigSection()));
        lastSection = &sections[sectName];
    } else {
        auto confSepPos = std::find(line.begin(),line.end(),'=');
        if(confSepPos == line.end()) {
            DBG_ERROR("Syntax Error: config line does not contain separator.");
            return false;
        }

        std::string confKey(line.begin(),confSepPos);
        ltrim(confKey);
        rtrim(confKey);

        std::string confValue(confSepPos + 1, line.end());
        ltrim(confValue);
        rtrim(confValue);

        lastSection->items.insert(std::make_pair(std::move(confKey),std::move(confValue)));
    }
    return true;
}

bool INIParser::save() {
    std::ofstream file(filePath);
    if(!file) {
        return false;
    }
    for( auto& [sectName,sectContent] : sections) {
        file << "[" << sectName << "]" << std::endl;
        for( auto &[key,value] : sectContent.items ) {
            file << key << '=' << value << std::endl;
        }
    }
    return true;
}

ConfigSection *INIParser::findSection(const std::string &name) {

    auto sectIter = sections.find(name);
    if(sectIter == sections.end()) {
        return nullptr;
    } else {
        return &sectIter->second;
    }
}

std::unordered_map<std::string, ConfigSection>::iterator INIParser::getSectionTableBegin() {
    return sections.begin();
}

std::unordered_map<std::string, ConfigSection>::iterator INIParser::getSectionTableEnd() {
    return sections.end();
}

const std::string &INIParser::getFilePath() const {
    return filePath;
}

void INIParser::setFilePath(const std::string &filePath) {
    INIParser::filePath = filePath;
}

ConfigSection *INIParser::createSection(const std::string &name) {
    ConfigSection* foundSection = findSection(name);
    if(foundSection != nullptr) return foundSection;
    auto [elem,inserted] = sections.emplace(std::make_pair(name,ConfigSection()));

    if(inserted) {
        return &elem->second;
    } else {
        return nullptr;
    }
}

bool INIParser::deleteSection(const std::string &name) {
    return sections.erase(name) == 1;;
}

bool ConfigSection::getValueBool(const std::string &key, bool &valueRef) {
    std::string valueRaw;
    if(!getValueStr(key,valueRaw))return false;
    std::transform(valueRaw.begin(),valueRaw.end(),valueRaw.begin(),[](char c) {return tolower(c);});
    if(valueRaw == "true") {
        valueRef = true;
        return true;
    }
    if(valueRaw == "false") {
        valueRef = false;
        return true;
    }
    int num_val;
    try {
        num_val = std::stoi(valueRaw);
        if(num_val <0 || num_val > 1) {
            return false;
        }
        valueRef = num_val == 1;
        return true;
    } catch (std::exception& ex) {}

    return false;
}

bool ConfigSection::hasKey(const std::string &key) {
    return items.find(key) != items.end();
}

bool ConfigSection::getValueInt(const std::string &key, int &valueRef) {
    std::string valueRaw;
    if(!getValueStr(key,valueRaw))return false;
    int num_val;
    try {
        num_val = std::stoi(valueRaw);
        valueRef = num_val;
        return true;
    } catch (std::exception& ex) {
        return false;
    }
}

bool ConfigSection::getValueStr(const std::string &key, std::string &valueRef) {
    auto itemIter = items.find(key);
    if(itemIter == items.end()) return false;
    valueRef = itemIter->second;
    return true;
}

bool ConfigSection::getValueUint(const std::string &key, unsigned long &valueRef) {
    std::string valueRaw;
    if(!getValueStr(key,valueRaw))return false;
    try {
        valueRef = std::stoul(valueRaw);
        return true;
    } catch (std::exception& ex) {
        return false;
    }
}

void ConfigSection::addKVPair(const std::string &key, const  std::string &value) {
    items[key] = value;
}

bool ConfigSection::delKey(const std::string &key) {
    return items.erase(key) == 1;
}
