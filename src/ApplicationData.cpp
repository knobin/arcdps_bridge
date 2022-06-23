//
//  src/ApplicationData.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

// Local Headers
#include "ApplicationData.hpp"
#include "Log.hpp"

// C++ Headers
#include <filesystem>
#include <fstream>
#include <sstream>

Configs InitConfigs(const std::string& filepath)
{
    if (std::filesystem::exists(filepath))
        return LoadConfigFile(filepath);

    CreateConfigFile(filepath);
    return Configs{};
}

std::string BridgeInfoToJSON(const BridgeInfo& info)
{
    std::ostringstream ss{};
    ss << "{\"type\":\"Info\",";
    ss << "\"Version\":\"" << std::string{info.version} << "\","
       << "\"ExtraVersion\":\"" << info.extraVersion << "\","
       << "\"ArcVersion\":\"" << info.arcvers << "\","
       << "\"ArcLoaded\":" << ((info.arcLoaded) ? "true" : "false") << ","
       << "\"ExtraLoaded\":" << ((info.extraLoaded) ? "true" : "false") << "}";
    return ss.str();
}

void CreateConfigFile(const std::string& filepath)
{
    BRIDGE_INFO("Creating Config File \"", filepath, "\"");

    std::ofstream configFile;
    configFile.open(filepath);

    Configs config{};
    configFile << "[general]\n";
    configFile << "enabled = " << ((config.enabled) ? "true" : "false") << "\n";
    configFile << "extras = " << ((config.extras) ? "true" : "false") << "\n";
    configFile << "arcDPS = " << ((config.arcDPS) ? "true" : "false") << "\n";
    configFile << "msgQueueSize = " << config.msgQueueSize << "\n";
    configFile.close();
}

Configs LoadConfigFile(const std::string& filepath)
{
    BRIDGE_INFO("Loading Config File \"", filepath, "\"");
    Configs config{};

    std::ifstream configFile;
    configFile.open(filepath, std::ifstream::in);

    std::string line{};
    unsigned int lineNumber{0};
    std::string header{};
    while (std::getline(configFile, line))
    {
        ++lineNumber;
        auto it = std::remove_if(line.begin(), line.end(), ::isspace);
        line.erase(it, line.end());

        if (!line.empty())
        {
            if (line.front() == '[' && line.back() == ']')
            {
                header = line.substr(1, line.size() - 2);
                BRIDGE_INFO("Found Config Header \"", header, "\"");
            }
            else if (!header.empty())
            {
                std::size_t equalPos = line.find('=');
                if (equalPos != std::string::npos && equalPos > 0 && equalPos < line.size() - 1)
                {
                    std::string name = line.substr(0, equalPos);
                    std::string value = line.substr(equalPos + 1);
                    BRIDGE_INFO("Found Config Entry \"", name, "\" = ", value);

                    if (header == "general")
                    {
                        if (name == "enabled")
                            config.enabled = ((value == "true") ? true : false);
                        else if (name == "extras")
                            config.extras = ((value == "true") ? true : false);
                        else if (name == "arcDPS")
                            config.arcDPS = ((value == "true") ? true : false);
                        else if (name == "msgQueueSize")
                        {
                            std::istringstream iss(value);
                            iss >> config.msgQueueSize;
#ifdef BRIDGE_DEBUG
                            if (iss.fail())
                            {
                                BRIDGE_INFO("Failed to convert \"", value, "\" to std::size_t");
                            }
#endif
                        }
                    }
                }
            }
        }
    }

    return config;
}