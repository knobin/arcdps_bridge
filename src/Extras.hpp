//
//  src/Extras.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-08.
//

#ifndef BRIDGE_EXTRAS_HPP
#define BRIDGE_EXTRAS_HPP

// Local Headers
#include "Message.hpp"

// ArcDPS Unofficial Extras Header
#include "Definitions.h"

namespace Extras
{
    //
    // Extras Squad Callback.
    //

    // __time64_t is treated as int64_t here.
    // bool is treated as 1 byte.
    class UserInfoSerializer
    {
    public:
        UserInfoSerializer() = delete;
        explicit UserInfoSerializer(const UserInfo& user)
            : m_user{user}
        {
            if (m_user.AccountName != nullptr)
                m_strLength = static_cast<uint16_t>(std::strlen(m_user.AccountName)) + 1;
        }

        [[nodiscard]] inline std::size_t size() const noexcept
        {
            return fixedSize() + dynamicSize();
        }

        [[nodiscard]] static constexpr std::size_t fixedSize() noexcept
        {
            constexpr auto str{2 * sizeof(uint16_t)}; // AccountName "pointer" and size.
            return str + sizeof(int64_t) + sizeof(UserInfo::Role) + sizeof(UserInfo::Subgroup) + sizeof(uint8_t);
        }

        [[nodiscard]] std::size_t dynamicSize() const noexcept
        {
            return m_strLength;
        }

        [[nodiscard]] MessageBuffers writeToBuffers(MessageBuffers buffers) const
        {
            // AccountName.
            const auto acIndex = static_cast<uint16_t>(buffers.dynamic - buffers.fixed);
            buffers.dynamic = serial_w_string(buffers.dynamic, m_user.AccountName, m_strLength - 1);
            buffers.fixed = serial_w_integral(buffers.fixed, acIndex);

            // Fixed.
            buffers.fixed = serial_w_integral(buffers.fixed, static_cast<int64_t>(m_user.JoinTime));
            buffers.fixed = serial_w_integral(buffers.fixed, static_cast<std::underlying_type_t<UserRole>>(m_user.Role));
            buffers.fixed = serial_w_integral(buffers.fixed, m_user.Subgroup);
            buffers.fixed = serial_w_integral(buffers.fixed, static_cast<uint8_t>(m_user.ReadyStatus));

            return buffers;
        }

    private:
        const UserInfo& m_user;
        uint16_t m_strLength{0};
    };

    //
    // Extras Language Callback.
    //

    class LanguageSerializer
    {
    public:
        LanguageSerializer() = delete;
        explicit LanguageSerializer(Language language)
            : m_language{language}
        {}

        [[nodiscard]] static constexpr std::size_t size() noexcept
        {
            return fixedSize() + dynamicSize();
        }

        [[nodiscard]] static constexpr std::size_t fixedSize() noexcept
        {
            return sizeof(std::underlying_type_t<Language>);
        }

        [[nodiscard]] static constexpr std::size_t dynamicSize() noexcept
        {
            return 0;
        }

        [[nodiscard]] MessageBuffers writeToBuffers(MessageBuffers buffers) const
        {
            buffers.fixed = serial_w_integral(buffers.fixed, static_cast<std::underlying_type_t<Language>>(m_language));
            return buffers;
        }

    private:
        Language m_language;
    };

    //
    // Extras KeyBind Callback.
    //

    class KeyBindSerializer
    {
    public:
        KeyBindSerializer() = delete;
        explicit KeyBindSerializer(const KeyBinds::KeyBindChanged& bindChanged)
            : m_bindChanged{bindChanged}
        {}

        [[nodiscard]] static constexpr std::size_t size() noexcept
        {
            return fixedSize() + dynamicSize();
        }

        [[nodiscard]] static constexpr std::size_t fixedSize() noexcept
        {
            constexpr std::size_t key_control_size = sizeof(std::underlying_type_t<KeyBinds::KeyControl>);
            constexpr std::size_t key_index_size = sizeof(uint32_t);
            constexpr std::size_t key_size =
                sizeof(std::underlying_type_t<KeyBinds::DeviceType>) + sizeof(int32_t) + sizeof(KeyBinds::Modifier);

            return key_control_size + key_index_size + key_size;
        }

        [[nodiscard]] static constexpr std::size_t dynamicSize() noexcept
        {
            return 0;
        }

        [[nodiscard]] MessageBuffers writeToBuffers(MessageBuffers buffers) const
        {
            const auto keyControl = static_cast<std::underlying_type_t<KeyBinds::KeyControl>>(m_bindChanged.KeyControl);
            buffers.fixed = serial_w_integral(buffers.fixed, keyControl);
            buffers.fixed = serial_w_integral(buffers.fixed, m_bindChanged.KeyIndex);

            const auto deviceType =
                static_cast<std::underlying_type_t<KeyBinds::DeviceType>>(m_bindChanged.SingleKey.DeviceType);
            buffers.fixed = serial_w_integral(buffers.fixed, deviceType);
            buffers.fixed = serial_w_integral(buffers.fixed, m_bindChanged.SingleKey.Code);
            buffers.fixed = serial_w_integral(buffers.fixed, m_bindChanged.SingleKey.Modifier);

            return buffers;
        }

    private:
        const KeyBinds::KeyBindChanged& m_bindChanged;
    };

    //
    // Extras Chat Message Callback.
    //

    class ChatMessageSerializer
    {
    public:
        ChatMessageSerializer() = delete;
        explicit ChatMessageSerializer(const ChatMessageInfo& chatMsgInfo)
            : m_chatMsgInfo{chatMsgInfo}
        {}

        [[nodiscard]] inline std::size_t size() const noexcept
        {
            return fixedSize() + dynamicSize();
        }

        [[nodiscard]] static constexpr std::size_t fixedSize() noexcept
        {
            constexpr auto stamp{2 * sizeof(uint16_t)}; // Timestamp "pointer" and size.
            constexpr auto acc{2 * sizeof(uint16_t)}; // AccountName "pointer" and size.
            constexpr auto cha{2 * sizeof(uint16_t)}; // CharacterName "pointer" and size.
            constexpr auto txt{2 * sizeof(uint16_t)}; // Text "pointer" and size.
            return stamp + acc+ cha + txt + sizeof(ChatMessageInfo::ChannelId) + sizeof(std::underlying_type_t<ChannelType>) + sizeof(uint8_t) + sizeof(uint8_t);
        }

        [[nodiscard]] std::size_t dynamicSize() const noexcept
        {
            const std::size_t timestampCount{m_chatMsgInfo.TimestampLength + 1};
            const std::size_t accNameCount{m_chatMsgInfo.AccountNameLength + 1};
            const std::size_t charNameCount{m_chatMsgInfo.CharacterNameLength + 1};
            const std::size_t textCount{m_chatMsgInfo.TextLength + 1};
            return timestampCount + accNameCount + charNameCount + textCount;
        }

        [[nodiscard]] MessageBuffers writeToBuffers(MessageBuffers buffers) const
        {
            // Timestamp.
            {
                uint64_t stampIndex{0};
                uint32_t stampLength{0};
                if (m_chatMsgInfo.Timestamp)
                {
                    stampIndex = static_cast<uint64_t>(buffers.dynamic - buffers.fixed);
                    stampLength = static_cast<uint32_t>(m_chatMsgInfo.TimestampLength);
                    buffers.dynamic = serial_w_string(buffers.dynamic, m_chatMsgInfo.Timestamp, stampLength);
                    ++stampLength;
                }
                buffers.fixed = serial_w_integral(buffers.fixed, stampIndex);
                buffers.fixed = serial_w_integral(buffers.fixed, stampLength);
            }

            // AccountName.
            {
                uint64_t accIndex{0};
                uint32_t accLength{0};
                if (m_chatMsgInfo.AccountName)
                {
                    accIndex = static_cast<uint64_t>(buffers.dynamic - buffers.fixed);
                    accLength = static_cast<uint32_t>(m_chatMsgInfo.AccountNameLength);
                    buffers.dynamic = serial_w_string(buffers.dynamic, m_chatMsgInfo.AccountName, accLength);
                    ++accLength;
                }
                buffers.fixed = serial_w_integral(buffers.fixed, accIndex);
                buffers.fixed = serial_w_integral(buffers.fixed, accLength);
            }

            // CharacterName.
            {
                uint64_t chIndex{0};
                uint32_t chLength{0};
                if (m_chatMsgInfo.CharacterName)
                {
                    chIndex = static_cast<uint64_t>(buffers.dynamic - buffers.fixed);
                    chLength = static_cast<uint32_t>(m_chatMsgInfo.CharacterNameLength);
                    buffers.dynamic = serial_w_string(buffers.dynamic, m_chatMsgInfo.CharacterName, chLength);
                    ++chLength;
                }
                buffers.fixed = serial_w_integral(buffers.fixed, chIndex);
                buffers.fixed = serial_w_integral(buffers.fixed, chLength);
            }

            // ChatMessage.
            {
                uint64_t cmIndex{0};
                uint32_t cmLength{0};
                if (m_chatMsgInfo.CharacterName)
                {
                    cmIndex = static_cast<uint64_t>(buffers.dynamic - buffers.fixed);
                    cmLength = static_cast<uint32_t>(m_chatMsgInfo.TextLength);
                    buffers.dynamic = serial_w_string(buffers.dynamic, m_chatMsgInfo.Text, cmLength);
                    ++cmLength;
                }
                buffers.fixed = serial_w_integral(buffers.fixed, cmIndex);
                buffers.fixed = serial_w_integral(buffers.fixed, cmLength);
            }

            // Fixed.
            buffers.fixed = serial_w_integral(buffers.fixed, m_chatMsgInfo.ChannelId);
            buffers.fixed = serial_w_integral(buffers.fixed, static_cast<std::underlying_type_t<ChannelType>>(m_chatMsgInfo.Type));
            buffers.fixed = serial_w_integral(buffers.fixed, m_chatMsgInfo.Subgroup);
            buffers.fixed = serial_w_integral(buffers.fixed, m_chatMsgInfo.IsBroadcast);

            return buffers;
        }

    private:
        const ChatMessageInfo& m_chatMsgInfo;
    };

} // namespace Extras

#endif // BRIDGE_EXTRAS_HPP
