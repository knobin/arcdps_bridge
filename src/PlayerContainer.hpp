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

        [[nodiscard]] MessageBuffer CreateMessageBuffer(const std::string& self) const;

    private:
        std::array<std::pair<bool, PlayerInfoEntry>, 65> m_squad{};
        mutable std::mutex m_mutex;
    };

    class PlayerInfoSerializer
    {
    public:
        PlayerInfoSerializer() = delete;
        explicit PlayerInfoSerializer(const PlayerInfo& info)
            : m_info{info}
        {}

        [[nodiscard]] inline std::size_t size() const noexcept
        {
            return fixedSize() + dynamicSize();
        }

        [[nodiscard]] static constexpr std::size_t fixedSize() noexcept
        {
            constexpr auto acc{2 * sizeof(uint16_t)}; // AccountName "pointer" and size.
            constexpr auto cha{2 * sizeof(uint16_t)}; // CharacterName "pointer" and size.
            return acc + cha + sizeof(PlayerInfo::joinTime) + sizeof(PlayerInfo::profession) +
                sizeof(PlayerInfo::elite) + sizeof(PlayerInfo::role) +
                sizeof(PlayerInfo::subgroup) + (3 * sizeof(uint8_t));
        }

        [[nodiscard]] std::size_t dynamicSize() const noexcept
        {
            const auto accSize{(!m_info.accountName.empty() ? m_info.accountName.size() + 1 : 0)};
            const auto charSize{(!m_info.characterName.empty() ? m_info.characterName.size() + 1 : 0)};
            return accSize + charSize;
        }

        [[nodiscard]] MessageBuffers writeToBuffers(MessageBuffers buffers) const
        {
            // AccountName.
            const auto acIndex = static_cast<uint16_t>(buffers.dynamic - buffers.fixed);
            buffers.dynamic = serial_w_string(buffers.dynamic, m_info.accountName.data(), m_info.accountName.size());
            buffers.fixed = serial_w_integral(buffers.fixed, acIndex);
            buffers.fixed = serial_w_integral(buffers.fixed, m_info.accountName.size() + 1);

            // CharacterName.
            const auto chIndex = static_cast<uint16_t>(buffers.dynamic - buffers.fixed);
            buffers.dynamic = serial_w_string(buffers.dynamic, m_info.characterName.data(), m_info.characterName.size());
            buffers.fixed = serial_w_integral(buffers.fixed, chIndex);
            buffers.fixed = serial_w_integral(buffers.fixed, m_info.characterName.size() + 1);

            // Fixed.
            buffers.fixed = serial_w_integral(buffers.fixed, m_info.joinTime);
            buffers.fixed = serial_w_integral(buffers.fixed, m_info.profession);
            buffers.fixed = serial_w_integral(buffers.fixed, m_info.elite);
            buffers.fixed = serial_w_integral(buffers.fixed, m_info.role);
            buffers.fixed = serial_w_integral(buffers.fixed, m_info.subgroup);

            buffers.fixed = serial_w_integral(buffers.fixed, static_cast<uint8_t>(m_info.inInstance));
            buffers.fixed = serial_w_integral(buffers.fixed, static_cast<uint8_t>(m_info.self));
            buffers.fixed = serial_w_integral(buffers.fixed, static_cast<uint8_t>(m_info.readyStatus));

            return buffers;
        }

    private:
        const PlayerInfo& m_info;
    };

    class PlayerInfoEntrySerializer
    {
    public:
        PlayerInfoEntrySerializer() = delete;
        explicit PlayerInfoEntrySerializer(const PlayerInfoEntry& info)
            : m_info{info}
        {}

        [[nodiscard]] inline std::size_t size() const noexcept
        {
            return fixedSize() + dynamicSize();
        }

        [[nodiscard]] static constexpr std::size_t fixedSize() noexcept
        {
            return sizeof(uint16_t) + sizeof(m_info.validator);
        }

        [[nodiscard]] std::size_t dynamicSize() const noexcept
        {
            const PlayerInfoSerializer serializer{m_info.player};
            return serializer.size();
        }

        [[nodiscard]] MessageBuffers writeToBuffers(MessageBuffers buffers) const
        {
            // Write whole PlayerInfo on dynamic.
            const auto playerInfoIndex = static_cast<uint16_t>(buffers.dynamic - buffers.fixed);
            MessageBuffers playerInfoBuffers{buffers.dynamic, buffers.dynamic + PlayerInfoSerializer::fixedSize()};
            const PlayerInfoSerializer serializer{m_info.player};
            playerInfoBuffers = serializer.writeToBuffers(playerInfoBuffers);
            buffers.dynamic = playerInfoBuffers.dynamic; // Update local dynamic location.
            buffers.fixed = serial_w_integral(buffers.fixed, playerInfoIndex);

            // Fixed.
            buffers.fixed = serial_w_integral(buffers.fixed, m_info.validator);

            return buffers;
        }

    private:
        const PlayerInfoEntry& m_info;
    };

} // namespace Squad

bool operator==(const Squad::PlayerInfo& lhs, const Squad::PlayerInfo& rhs);
bool operator!=(const Squad::PlayerInfo& lhs, const Squad::PlayerInfo& rhs);


#endif // BRIDGE_PLAYERCONTAINER_HPP