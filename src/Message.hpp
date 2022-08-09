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
// MessageSource.
// Main type of the message.
//

enum class MessageSource : uint8_t
{
    Info    = 1,
    Combat  = 2,
    Extras  = 4,
    Squad   = 8,
};

constexpr std::string_view MessageSourceToStr(MessageSource source) noexcept
{
    switch (source)
    {
        case MessageSource::Info:
            return "Info";
        case MessageSource::Combat:
            return "Combat";
        case MessageSource::Extras:
            return "Extras";
        case MessageSource::Squad:
            return "Squad";
    }

    return "";
}

//
// MessageType.
// Sub type of MessageSource for the message.
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
// Implementation for matching MessageInfo types to MessageSource.
//

template<MessageSource source>
struct MatchTypeToSource
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
struct MatchTypeToSource<MessageSource::Info> 
    : MsgTypeMatcher<MessageType::BridgeInfo, MessageType::Status, MessageType::Closing>
{};

template<>
struct MatchTypeToSource<MessageSource::Combat> : MsgTypeMatcher<MessageType::CombatEvent>
{};

template<>
struct MatchTypeToSource<MessageSource::Extras> : MsgTypeMatcher<MessageType::ExtrasSquadUpdate>
{};

template<>
struct MatchTypeToSource<MessageSource::Squad> 
    : MsgTypeMatcher<MessageType::SquadStatus, MessageType::SquadAdd, MessageType::SquadUpdate, MessageType::SquadRemove>
{};

//
// Matcher.
//

template<MessageSource Source, MessageType Type>
struct MatchSourceAndType : std::integral_constant<bool, MatchTypeToSource<Source>::Match<Type>()>
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

// First two bytes are reserved for MessageSource and MessageType.
constexpr std::size_t SerialStartPadding = 2;

//
// Message class.
//

class Message
{
public:
    Message() = default;
    Message(MessageSource source, MessageType type)
        : m_source{static_cast<std::underlying_type_t<MessageSource>>(source)},
          m_type{static_cast<std::underlying_type_t<MessageType>>(type)}
    {
        generate();
    }
    Message(MessageSource source, MessageType type, const SerialData& serial, const nlohmann::json& jdata) 
        : m_source{static_cast<std::underlying_type_t<MessageSource>>(source)},
          m_type{static_cast<std::underlying_type_t<MessageType>>(type)},
          m_serial{serial}
    {
        generate(jdata);
    }
    virtual ~Message() = default;

    const SerialData& toSerial() const { return m_serial; }
    const std::string& toJSON() const { return m_json; }

    MessageSource source() const noexcept { return static_cast<MessageSource>(m_source); }
    MessageType type() const noexcept { return static_cast<MessageType>(m_type); }
    bool empty() const noexcept { return !m_source || !m_type; }

private:
    void generate()
    {
        m_json = nlohmann::json{
            {"source", MessageSourceToStr(source())},
            {"type", MessageTypeToStr(type())},
        }.dump();
    }

    void generate(const nlohmann::json& data)
    {
        // Set first two bytes in serial data.
        if (m_serial.count > 1 && m_serial.data)
        {
            m_serial.data[0] = m_source;
            m_serial.data[1] = m_type;
        }

        // Generate json header for message.
        m_json = nlohmann::json{
            {"source", MessageSourceToStr(source())}, 
            {"type", MessageTypeToStr(type())}, 
            {"data", data}
        }.dump();
    }

private:
    SerialData m_serial{};
    std::string m_json{};
    std::underlying_type_t<MessageSource> m_source{0};
    std::underlying_type_t<MessageType> m_type{0};
};

//
// Message create functions.
//

template<MessageSource Source, MessageType Type, typename... Args>
Message CreateMessage(Args&&... args)
{
    static_assert(MatchSourceAndType<Source, Type>::value, 
                  "MessageSource and MessageType does not match");

    return Message{Source, Type, std::forward<Args>(args)...};
}

template<MessageType Type, typename... Args>
Message InfoMessage(Args&&... args)
{
    static_assert(MatchSourceAndType<MessageSource::Info, Type>::value,
                  "Type is not an Info message");

    return CreateMessage<MessageSource::Info, Type>(std::forward<Args>(args)...);
}

template<MessageType Type, typename... Args>
Message CombatMessage(Args&&... args)
{
    static_assert(MatchSourceAndType<MessageSource::Combat, Type>::value,
                  "Type is not a Combat message");

    return CreateMessage<MessageSource::Combat, Type>(std::forward<Args>(args)...);
}

template<MessageType Type, typename... Args>
Message ExtrasMessage(Args&&... args)
{
    static_assert(MatchSourceAndType<MessageSource::Extras, Type>::value,
                  "Type is not an Extras message");

    return CreateMessage<MessageSource::Extras, Type>(std::forward<Args>(args)...);
}

template<MessageType Type, typename... Args>
Message SquadMessage(Args&&... args)
{
    static_assert(MatchSourceAndType<MessageSource::Squad, Type>::value,
                  "Type is not a Squad message");

    return CreateMessage<MessageSource::Squad, Type>(std::forward<Args>(args)...);
}

#endif // BRIDGE_MESSAGE_HPP
