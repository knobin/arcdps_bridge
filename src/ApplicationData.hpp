//
//  src/ApplicationData.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#ifndef BRIDGE_APPLICATIONDATA_HPP
#define BRIDGE_APPLICATIONDATA_HPP

// Local Headers
#include "PlayerContainer.hpp"

// nlohmann_json Headers
#include <nlohmann/json.hpp>

// C++ Headers
#include <atomic>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>

#ifdef BRIDGE_BUILD_DEBUG
    #define BRIDGE_BUILD_STR "DEBUG"
#else
    #define BRIDGE_BUILD_STR "RELEASE"
#endif

// clang-format off

struct BridgeVersionInfo
{
    std::string_view version{BRIDGE_VERSION_STR};       // Bridge version.
    uint32_t majorApiVersion{BRIDGE_API_VERSION_MAJOR}; // Incremented when there is a change that breaks backwards compatibility in any way.
    uint32_t minorApiVersion{BRIDGE_API_VERSION_MINOR}; // Incremented when the API is extended in a way that does not break backwards compatibility.
} static constexpr BridgeVersion;

struct BridgeInfo
{
    std::string extrasVersion{};    // Unofficial Extras version.
    std::string arcVersion{};       // ArcDPS version.

    uint64_t validator{1};  // Runtime version of the BridgeInfo, if any value changes this will be incremented.

    uint32_t extrasInfoVersion{0};  // Extras InfoVersion the bridge is using. 0 if unknown or extras is not initialized.

    bool arcLoaded{false};      // Is ArcDPS loaded (enabled in the bridge).
    bool extrasFound{false};    // Has the Unofficial Extras init callback been called.
    bool extrasLoaded{false};   // Is Unofficial Extras loaded (enabled in the bridge).

    mutable std::mutex mutex;
};

nlohmann::json ToJSON(const BridgeInfo& info);

class BridgeInfoSerializer
{
public:
    BridgeInfoSerializer() = delete;
    explicit BridgeInfoSerializer(const BridgeInfo& info)
        : m_info{info}
    {}

    [[nodiscard]] inline std::size_t size() const noexcept
    {
        return fixedSize() + dynamicSize();
    }

    [[nodiscard]] static constexpr std::size_t fixedSize() noexcept
    {
        constexpr auto evStr{2 * sizeof(uint16_t)}; // extrasVersion "pointer" and size.
        constexpr auto avStr{2 * sizeof(uint16_t)}; // arcvers "pointer" and size.
        return evStr + avStr + (3 * sizeof(uint8_t)) + sizeof(m_info.validator) + sizeof(m_info.extrasInfoVersion);
    }

    [[nodiscard]] std::size_t dynamicSize() const noexcept
    {
        const auto evSize{(!m_info.extrasVersion.empty() ? m_info.extrasVersion.size() + 1 : 0)};
        const auto avSize{(!m_info.arcVersion.empty() ? m_info.arcVersion.size() + 1 : 0)};
        return evSize + avSize;
    }

    void writeToBuffers(MessageBuffers buffers) const
    {
        // Runtime version of BridgeInfo.
        buffers.fixed = serial_w_integral(buffers.fixed, m_info.validator);

        // extrasVersion.
        const auto evIndex = static_cast<uint16_t>(buffers.dynamic - buffers.fixed);
        buffers.dynamic = serial_w_string(buffers.dynamic, m_info.extrasVersion.data(), m_info.extrasVersion.size());
        buffers.fixed = serial_w_integral(buffers.fixed, evIndex);
        buffers.fixed = serial_w_integral(buffers.fixed, m_info.extrasVersion.size() + 1);

        // arcVersion.
        const auto avIndex = static_cast<uint16_t>(buffers.dynamic - buffers.fixed);
        buffers.dynamic = serial_w_string(buffers.dynamic, m_info.arcVersion.data(), m_info.arcVersion.size());
        buffers.fixed = serial_w_integral(buffers.fixed, avIndex);
        buffers.fixed = serial_w_integral(buffers.fixed, m_info.arcVersion.size() + 1);

        // Extras InfoVersion used.
        buffers.fixed = serial_w_integral(buffers.fixed, m_info.extrasInfoVersion);

        // Booleans.
        buffers.fixed[0] = static_cast<uint8_t>(m_info.arcLoaded);
        buffers.fixed[1] = static_cast<uint8_t>(m_info.extrasFound);
        buffers.fixed[2] = static_cast<uint8_t>(m_info.extrasLoaded);
    }

private:
    const BridgeInfo& m_info;
};

// clang-format on

///////////////////////////////////////////////////////////////////////////////////////////////////

struct Configs
{
    // General.
    bool enabled{true}; // Should the extension be enabled.
    bool arcDPS{true};  // Should ArcDPS be used.
    bool extras{true};  // Should the Unofficial Extras be used.

    // Server.
    std::size_t maxClients{32};             // Max amount of clinets at one time.
    std::size_t clientTimeoutTimer{120000}; // Check if client disconnected after specified amount of milliseconds.
    std::size_t msgQueueSize{64};           // How many messages can be queued before being dropped.

    void set(const std::string& header, const std::string& entry, const std::string& value);

private:
    template <typename T>
    std::optional<T> StringTo(const std::string& str)
    {
        T value{};
        std::istringstream iss(str);
        iss >> value;
        if (iss.fail())
            return std::nullopt;
        return value;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

struct CharacterType
{
    uint32_t profession{};
    uint32_t elite{};
};

///////////////////////////////////////////////////////////////////////////////////////////////////

struct ApplicationData
{
    Squad::PlayerContainer Squad{};
    std::string SelfAccountName{};

    Configs Config{};
    BridgeInfo Info{};

    std::string_view ConfigFile{"arcdps_bridge.ini"};
    std::string_view LogFile{"arcdps_bridge.log"};
    std::string_view PipeName{R"(\\.\pipe\arcdps-bridge)"};

    [[nodiscard]] uint64_t requestID() const noexcept { return counter++; }

private:
    mutable std::atomic<uint64_t> counter{1};
};

Configs InitConfigs(const std::string& filepath);
Configs LoadConfigFile(const std::string& filepath);

#endif // BRIDGE_APPLICATIONDATA_HPP
