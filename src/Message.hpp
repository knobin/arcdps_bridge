//
//  src/Message.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-07-26.
//

#ifndef BRIDGE_MESSAGE_HPP
#define BRIDGE_MESSAGE_HPP

// nlohmann_json Headers
#include <nlohmann/json.hpp>

// C++ Headers
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <sstream>

//
// MessageCategory.
// Main type of the message.
//

enum class MessageCategory : uint8_t
{
    Info    = 1,
    Combat  = 2,
    Extras  = 4,
    Squad   = 8,
};

constexpr std::string_view MessageCategoryToStr(MessageCategory category) noexcept
{
    switch (category)
    {
        case MessageCategory::Info:
            return "Info";
        case MessageCategory::Combat:
            return "Combat";
        case MessageCategory::Extras:
            return "Extras";
        case MessageCategory::Squad:
            return "Squad";
    }

    return "";
}

//
// MessageType.
// Sub type of MessageCategory for the message.
//

enum class MessageType : uint8_t
{
    // Info types.
    BridgeInfo = 1,
    Status,
    Closing,

    // ArcDPS combat api types.
    CombatEvent,

    // Extras event types.
    ExtrasSquadUpdate,
    // ExtrasLanguageChanged,   // TODO
    // ExtrasKeyBindChanged,    // TODO
    // ExtrasChatMessage,       // TODO

    // Squad event types.
    SquadStatus,
    SquadAdd,
    SquadUpdate,
    SquadRemove
};

constexpr std::string_view MessageTypeToStr(MessageType type) noexcept
{
    switch (type)
    {
        case MessageType::BridgeInfo:
            return "BridgeInfo";
        case MessageType::Status:
            return "Status";
        case MessageType::Closing:
            return "Closing";
        case MessageType::CombatEvent:
            return "CombatEvent";
        case MessageType::ExtrasSquadUpdate:
            return "ExtrasSquadUpdate";
        case MessageType::SquadStatus:
            return "SquadStatus";
        case MessageType::SquadAdd:
            return "SquadAdd";
        case MessageType::SquadUpdate:
            return "SquadUpdate";
        case MessageType::SquadRemove:
            return "SquadRemove";
    }

    return "";
}

//
// Implementation for matching MessageInfo types to MessageCategory.
//

template<MessageCategory Category>
struct MatchTypeToCategory
{
    template<MessageType Type>
    static constexpr bool Match() noexcept
    {
        return false;
    }
};

template<MessageType...Types>
struct MsgTypeMatcher
{
    template<MessageType Type>
    static constexpr bool Match() noexcept
    {
        return ((Types == Type) || ...);
    }
};

//
// Specialized Matchers.
//

template<>
struct MatchTypeToCategory<MessageCategory::Info> 
    : MsgTypeMatcher<MessageType::BridgeInfo, MessageType::Status, MessageType::Closing>
{};

template<>
struct MatchTypeToCategory<MessageCategory::Combat> : MsgTypeMatcher<MessageType::CombatEvent>
{};

template<>
struct MatchTypeToCategory<MessageCategory::Extras> : MsgTypeMatcher<MessageType::ExtrasSquadUpdate>
{};

template<>
struct MatchTypeToCategory<MessageCategory::Squad> 
    : MsgTypeMatcher<MessageType::SquadStatus, MessageType::SquadAdd, MessageType::SquadUpdate, MessageType::SquadRemove>
{};

//
// Matcher.
//

template<MessageCategory Category, MessageType Type>
struct MatchCategoryAndType : std::integral_constant<bool, MatchTypeToCategory<Category>::Match<Type>()>
{};

//
// MessageProtocol.
// Types of message protocols supported.
//

enum class MessageProtocol : uint8_t
{
    Serial  = 1,
    JSON    = 2
};

constexpr std::string_view MessageProtocolToStr(MessageProtocol protocol) noexcept
{
    switch (protocol)
    {
        case MessageProtocol::Serial:
            return "Serial";
        case MessageProtocol::JSON:
            return "JSON";
    }

    return "";
}

struct SerialData
{
    std::shared_ptr<uint8_t[]> data{nullptr};
    std::size_t count{0};
};

// First two bytes are reserved for MessageCategory and MessageType.
constexpr std::size_t SerialStartPadding = 2;

//
// Message class.
//

class Message
{
public:
    Message() = default;
    Message(MessageCategory category, MessageType type)
        : m_category{static_cast<std::underlying_type_t<MessageCategory>>(category)},
          m_type{static_cast<std::underlying_type_t<MessageType>>(type)}
    {
        generate();
    }
    Message(MessageCategory category, MessageType type, const SerialData& serial, const nlohmann::json& jdata) 
        : m_category{static_cast<std::underlying_type_t<MessageCategory>>(category)},
          m_type{static_cast<std::underlying_type_t<MessageType>>(type)},
          m_serial{serial}
    {
        generate(jdata);
    }
    virtual ~Message() = default;

    const SerialData& toSerial() const { return m_serial; }
    const std::string& toJSON() const { return m_json; }

    MessageCategory category() const noexcept { return static_cast<MessageCategory>(m_category); }
    MessageType type() const noexcept { return static_cast<MessageType>(m_type); }
    bool empty() const noexcept { return !m_category || !m_type; }

private:
    void generate()
    {
        m_json = nlohmann::json{
            {"category", MessageCategoryToStr(category())},
            {"type", MessageTypeToStr(type())},
        }.dump();
    }

    void generate(const nlohmann::json& data)
    {
        // Set first two bytes in serial data.
        if (m_serial.count > 1 && m_serial.data)
        {
            m_serial.data[0] = m_category;
            m_serial.data[1] = m_type;
        }

        // Generate json header for message.
        m_json = nlohmann::json{
            {"category", MessageCategoryToStr(category())}, 
            {"type", MessageTypeToStr(type())}, 
            {"data", data}
        }.dump();
    }

private:
    SerialData m_serial{};
    std::string m_json{};
    std::underlying_type_t<MessageCategory> m_category{0};
    std::underlying_type_t<MessageType> m_type{0};
};

//
// Message create functions.
//

template<MessageCategory Category, MessageType Type, typename... Args>
Message CreateMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<Category, Type>::value, 
                  "MessageCategory and MessageType does not have a match");

    return Message{Category, Type, std::forward<Args>(args)...};
}

template<MessageType Type, typename... Args>
Message InfoMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<MessageCategory::Info, Type>::value,
                  "Type is not an Info message");

    return CreateMessage<MessageCategory::Info, Type>(std::forward<Args>(args)...);
}

template<MessageType Type, typename... Args>
Message CombatMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<MessageCategory::Combat, Type>::value,
                  "Type is not a Combat message");

    return CreateMessage<MessageCategory::Combat, Type>(std::forward<Args>(args)...);
}

template<MessageType Type, typename... Args>
Message ExtrasMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<MessageCategory::Extras, Type>::value,
                  "Type is not an Extras message");

    return CreateMessage<MessageCategory::Extras, Type>(std::forward<Args>(args)...);
}

template<MessageType Type, typename... Args>
Message SquadMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<MessageCategory::Squad, Type>::value,
                  "Type is not a Squad message");

    return CreateMessage<MessageCategory::Squad, Type>(std::forward<Args>(args)...);
}

#endif // BRIDGE_MESSAGE_HPP
