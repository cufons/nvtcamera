//
// Created by cufon on 06.05.24.
//

#ifndef NVTCAMERA_INIPARSER_H
#define NVTCAMERA_INIPARSER_H

#include <unordered_map>
#include <fstream>
#include <string>
class ConfigSection {
    friend class INIParser;
    std::unordered_map<std::string,std::string> items{};
public:
    void addKVPair(const std::string& key, const std::string& value);
    bool delKey(const std::string& key);

    bool getValueStr(const std::string& key,std::string& valueRef);
    bool getValueBool(const std::string& key,bool& valueRef);
    bool getValueInt(const std::string& key,int& valueRef);
    bool getValueUint(const std::string& key, unsigned long& valueRef);
    bool hasKey(const std::string& key);
};
class INIParser {
public:
    using SectionTable = std::unordered_map<std::string,ConfigSection>;
private:
    SectionTable sections{};
    ConfigSection* lastSection;
    bool parseLine(std::string& line);
    std::string filePath;
public:
    const std::string &getFilePath() const;
    void setFilePath(const std::string &filePath);

public:
    INIParser();
    bool load();
    bool save();
    ConfigSection* findSection(const std::string& name);
    ConfigSection* createSection(const std::string& name);
    bool deleteSection(const std::string& name);
    SectionTable::iterator getSectionTableBegin();
    SectionTable::iterator getSectionTableEnd();
public:

};


#endif //NVTCAMERA_INIPARSER_H
