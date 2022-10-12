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

static void PrintConfigs(std::ostream& os, const Configs& config)
{
    os << "[general]\n";
    os << "enabled = " << ((config.enabled) ? "true" : "false") << "\n";
    os << "arcDPS = " << ((config.arcDPS) ? "true" : "false") << "\n";
    os << "extras = " << ((config.extras) ? "true" : "false") << "\n";

    os << "\n[server]\n";
    os << "maxClients = " << config.maxClients << "\n";
    os << "clientTimeoutTimer = " << config.clientTimeoutTimer << "\n";
    os << "msgQueueSize = " << config.msgQueueSize << "\n";
}

Configs InitConfigs(const std::string& filepath)
{
    if (std::filesystem::exists(filepath))
        return LoadConfigFile(filepath);

    BRIDGE_INFO("No Config File found at \"{}\", using default values.", filepath);
    const Configs config{};
    std::ostringstream oss{};
    PrintConfigs(oss, config);
    BRIDGE_DEBUG("Configs values set: \n\n{}", oss.str());

    return config;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void to_json(nlohmann::json& j, const BridgeInfo& info)
{
    j = nlohmann::json{{"version", std::string{info.version}},
                       {"extrasVersion", nullptr},
                       {"arcVersion", nullptr},
                       {"arcLoaded", info.arcLoaded},
                       {"extrasFound", info.extrasFound},
                       {"extrasLoaded", info.extrasLoaded},
                       {"extrasInfoVersion", info.extrasInfoVersion},
                       {"validator", info.validator},
                       {"majorApiVersion", info.majorApiVersion},
                       {"minorApiVersion", info.minorApiVersion}};

    if (!info.extrasVersion.empty())
        j["extrasVersion"] = info.extrasVersion;

    if (!info.arcvers.empty())
        j["arcVersion"] = info.arcvers;
}

std::size_t serial_size(const BridgeInfo& info)
{
    return info.version.size() + info.extrasVersion.size() + info.arcvers.size() + 3 + (3 * sizeof(uint8_t)) +
           sizeof(info.validator) + sizeof(info.extrasInfoVersion) + sizeof(info.majorApiVersion) +
           sizeof(info.minorApiVersion);
}

void to_serial(const BridgeInfo& info, uint8_t* storage, std::size_t)
{
    uint8_t* location = storage;

    // API version is written first!
    location = serial_w_integral(location, info.majorApiVersion);
    location = serial_w_integral(location, info.minorApiVersion);

    // Runtime version of BridgeInfo.
    location = serial_w_integral(location, info.validator);

    // Version strings.
    location = serial_w_string(location, info.version.data(), info.version.size());
    location = serial_w_string(location, info.extrasVersion.data(), info.extrasVersion.size());
    location = serial_w_string(location, info.arcvers.data(), info.arcvers.size());

    // Extras InfoVersion used.
    location = serial_w_integral(location, info.extrasInfoVersion);

    // Booleans.
    location[0] = static_cast<uint8_t>(info.arcLoaded);
    location[1] = static_cast<uint8_t>(info.extrasFound);
    location[2] = static_cast<uint8_t>(info.extrasLoaded);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Configs::set(const std::string& header, const std::string& entry, const std::string& value)
{
    if (header == "general")
    {
        if (entry == "enabled")
            enabled = (value == "true");
        else if (entry == "extras")
            extras = (value == "true");
        else if (entry == "arcDPS")
            arcDPS = (value == "true");
    }
    else if (header == "server")
    {
        if (entry == "maxClients")
        {
            if (auto conv = StringTo<std::size_t>(value))
                maxClients = *conv;
            else
                BRIDGE_WARN("Failed to convert \"{}\" to std::size_t", value);
        }
        else if (entry == "clientTimeoutTimer")
        {
            if (auto conv = StringTo<std::size_t>(value))
                clientTimeoutTimer = *conv;
            else
                BRIDGE_WARN("Failed to convert \"{}\" to std::size_t", value);
        }
        else if (entry == "msgQueueSize")
        {
            if (auto conv = StringTo<std::size_t>(value))
                msgQueueSize = *conv;
            else
                BRIDGE_WARN("Failed to convert \"{}\" to std::size_t", value);
        }
    }
}

Configs LoadConfigFile(const std::string& filepath)
{
    BRIDGE_INFO("Loading Config File \"{}\".", filepath);
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
                BRIDGE_DEBUG("Found Config Header \"{}\"", header);
            }
            else if (!header.empty())
            {
                std::size_t equalPos = line.find('=');
                if (equalPos != std::string::npos && equalPos > 0 && equalPos < line.size() - 1)
                {
                    std::string name = line.substr(0, equalPos);
                    std::string value = line.substr(equalPos + 1);
                    BRIDGE_INFO("Found Config Entry \"{}\" = {}", name, value);
                    config.set(header, name, value);
                }
            }
        }
    }

#if BRIDGE_LOG_LEVEL >= BRIDGE_LOG_LEVEL_DEBUG
    std::ostringstream oss{};
    PrintConfigs(oss, config);
    BRIDGE_DEBUG("Configs values set: \n\n{}", oss.str());
#endif

    return config;
}