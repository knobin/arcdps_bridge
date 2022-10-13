//
//  src/PlayerContainer.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#ifndef BRIDGE_PLAYERCONTAINER_HPP
#define BRIDGE_PLAYERCONTAINER_HPP

// Local Headers
#include "Message.hpp"

// ArcDPS Unofficial Extras Header
#include "Definitions.h"

// C++ Headers
#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace Squad
{
    constexpr uint64_t ValidatorStartValue = 1;

    struct PlayerInfo
    {
        std::string accountName{};
        std::string characterName{};
        int64_t joinTime{};
        uint32_t profession{};
        uint32_t elite{};
        uint8_t role{static_cast<uint8_t>(UserRole::None)};
        uint8_t subgroup{};
        bool inInstance{false};
        bool self{false};
        bool readyStatus{false};
    };

    struct PlayerInfoEntry
    {
        PlayerInfo player{};
        uint64_t validator{};
    };

    class PlayerContainer
    {
    public:
        enum class Status : uint8_t
        {
            Invalid = 0,    // Operation can never be successful.
            ValidatorError, // Invalid validator.
            ExistsError,    // Operation could not be completed because item already exists.
            Equal,          // New value to update with is the same is the old value. Increases validator.
            Success         // Operation was successful. Increases validator.
        };

        struct PlayerInfoUpdate
        {
            std::optional<PlayerInfoEntry> entry;
            Status status{Status::Invalid};
        };

    public:
        Status add(const PlayerInfo& player);
        PlayerInfoUpdate update(const PlayerInfoEntry& playerEntry);
        std::optional<PlayerInfoEntry> remove(const std::string& accountName);
        [[nodiscard]] std::optional<PlayerInfoEntry> find(const std::string& accountName) const;

        template <typename UnaryPredicate>
        [[nodiscard]] std::optional<PlayerInfoEntry> find_if(UnaryPredicate p)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            auto it = std::find_if(m_squad.cbegin(), m_squad.cend(), [&p](const auto& entry) {
                return entry.first && p(entry.second.player);
            });

            if (it != m_squad.cend())
                return it->second;

            return std::nullopt;
        }

        void clear();

        [[nodiscard]] nlohmann::json toJSON() const;
        [[nodiscard]] SerialData toSerial(std::size_t startPadding = 0) const;

    private:
        std::array<std::pair<bool, PlayerInfoEntry>, 65> m_squad{};
        mutable std::mutex m_mutex;
    };

    constexpr std::size_t PlayerInfoPartialSize = sizeof(PlayerInfo::joinTime) + sizeof(PlayerInfo::profession) +
                                                  sizeof(PlayerInfo::elite) + sizeof(PlayerInfo::role) +
                                                  sizeof(PlayerInfo::subgroup) + (3 * sizeof(uint8_t));

    [[nodiscard]] inline std::size_t SerialSize(const PlayerInfo& player) noexcept
    {
        const std::size_t acc_str_count{1 + player.accountName.size()};
        const std::size_t char_str_count{1 + player.characterName.size()};

        return acc_str_count + char_str_count + PlayerInfoPartialSize;
    }

    [[nodiscard]] inline std::size_t SerialSize(const PlayerInfoEntry& entry) noexcept
    {
        return sizeof(entry.validator) + SerialSize(entry.player);
    }

    void ToSerial(const PlayerInfo& player, uint8_t* storage, std::size_t count);
    void ToSerial(const PlayerInfoEntry& entry, uint8_t* storage, std::size_t count);

    [[nodiscard]] nlohmann::json ToJSON(const PlayerInfo& player);
    [[nodiscard]] nlohmann::json ToJSON(const PlayerInfoEntry& entry);

    template <MessageType Type>
    [[nodiscard]] Message PlayerEntryMessageGenerator(uint64_t id, uint64_t timestamp, const PlayerInfoEntry& entry,
                                                      std::underlying_type_t<MessageProtocol> protocols)
    {
        static_assert(Type == MessageType::SquadAdd || Type == MessageType::SquadRemove ||
                          Type == MessageType::SquadUpdate,
                      "Type is not a Squad message");

        const auto protocolSerial = static_cast<std::underlying_type_t<MessageProtocol>>(MessageProtocol::Serial);
        const auto protocolJSON = static_cast<std::underlying_type_t<MessageProtocol>>(MessageProtocol::JSON);

        SerialData serial{};

        if (protocols & protocolSerial)
        {
            const std::size_t playerentry_size = SerialSize(entry);
            serial = CreateSerialData(playerentry_size);
            ToSerial(entry, &serial.ptr[Message::DataOffset()], playerentry_size);

            if (protocols == protocolSerial)
                return SquadMessage<Type>(id, timestamp, serial);
        }

        nlohmann::json json{};

        if (protocols & protocolJSON)
        {
            json = ToJSON(entry);

            if (protocols == protocolJSON)
                return SquadMessage<Type>(id, timestamp, json);
        }

        return SquadMessage<Type>(id, timestamp, serial, json);
    }
} // namespace Squad

bool operator==(const Squad::PlayerInfo& lhs, const Squad::PlayerInfo& rhs);
bool operator!=(const Squad::PlayerInfo& lhs, const Squad::PlayerInfo& rhs);

#endif // BRIDGE_PLAYERCONTAINER_HPP