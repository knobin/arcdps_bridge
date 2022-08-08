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
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <sstream>

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

enum class MessageType : uint8_t
{
    // Info types.
    BridgeInfo = 1,
    Status,

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

class Message
{
public:
    Message() = default;
    Message(MessageSource source, MessageType type, const SerialData& serial, const nlohmann::json& jdata)
        : m_serial{serial},
          m_source{static_cast<std::underlying_type_t<MessageSource>>(source)},
          m_type{static_cast<std::underlying_type_t<MessageType>>(type)}
    {
        generate(jdata);
    }
    virtual ~Message() = default;

    const SerialData& toSerial() const { return m_serial; }
    std::string toJSON() const { return m_json; }

    MessageSource source() const noexcept { return static_cast<MessageSource>(m_source); }
    MessageType type() const noexcept { return static_cast<MessageType>(m_type); }
    bool empty() const noexcept { return !m_source || !m_type; }

private:
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
            {"origin", MessageSourceToStr(source())}, 
            {"type", MessageTypeToStr(type())}, 
            {"data", data}
        };
    }

private:
    SerialData m_serial;
    std::string m_json{};
    std::underlying_type_t<MessageSource> m_source{0};
    std::underlying_type_t<MessageType> m_type{0};
};

template<MessageType Type>
Message InfoMessage(const SerialData& serial, const nlohmann::json& jdata)
{
    static_assert(Type == MessageType::BridgeInfo || 
                  Type == MessageType::Status,
                  "Type is not an Info message");

    return Message(MessageSource::Info, Type, serial, jdata);
}

template<MessageType Type>
Message CombatMessage(const SerialData& serial, const nlohmann::json& jdata)
{
    static_assert(Type == MessageType::CombatEvent,
                  "Type is not a Combat message");

    return Message(MessageSource::Combat, Type, serial, jdata);
}

template<MessageType Type>
Message ExtrasMessage(const SerialData& serial, const nlohmann::json& jdata)
{
    static_assert(Type == MessageType::ExtrasSquadUpdate,
                  "Type is not an Extras message");

    return Message(MessageSource::Extras, Type, serial, jdata);
}

template<MessageType Type>
Message SquadMessage(const SerialData& serial, const nlohmann::json& jdata)
{
    static_assert(Type == MessageType::SquadAdd || 
                  Type == MessageType::SquadRemove || 
                  Type == MessageType::SquadStatus || 
                  Type == MessageType::SquadUpdate,
                  "Type is not a Squad message");

    return Message(MessageSource::Squad, Type, serial, jdata);
}

#endif // BRIDGE_MESSAGE_HPP
