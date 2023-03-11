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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <type_traits>
#include <utility>

//
// MessageCategory.
// Main type of the message.
//

enum class MessageCategory : uint8_t
{
    Info = 1,
    Combat = 2,
    Extras = 4,
    Squad = 8,
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
    // 0 = None or Empty, should never be used.

    // Info types.
    ConnectionStatus = 1,
    BridgeInfo = 2,
    Status = 3,
    Closing = 4,

    // ArcDPS combat api types.
    CombatEvent = 5,

    // Extras event types.
    ExtrasSquadUpdate = 6,
    ExtrasLanguageChanged = 7,
    ExtrasKeyBindChanged = 8,
    ExtrasChatMessage = 9,

    // Squad event types.
    SquadStatus = 10,
    SquadAdd = 11,
    SquadUpdate = 12,
    SquadRemove = 13
};

constexpr std::size_t MessageTypeCount = 14; // Including first 0 value.

constexpr std::string_view MessageTypeToStr(MessageType type) noexcept
{
    switch (type)
    {
        case MessageType::ConnectionStatus:
            return "ConnectionStatus";
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
        case MessageType::ExtrasLanguageChanged:
            return "ExtrasLanguageChanged";
        case MessageType::ExtrasKeyBindChanged:
            return "ExtrasKeyBindChanged";
        case MessageType::ExtrasChatMessage:
            return "ExtrasChatMessage";
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

// All possible strings for MessageType.
constexpr std::array<std::string_view, MessageTypeCount - 1> MessageTypeStrings{
    MessageTypeToStr(MessageType::ConnectionStatus),
    MessageTypeToStr(MessageType::BridgeInfo),
    MessageTypeToStr(MessageType::Status),
    MessageTypeToStr(MessageType::Closing),
    MessageTypeToStr(MessageType::CombatEvent),
    MessageTypeToStr(MessageType::ExtrasSquadUpdate),
    MessageTypeToStr(MessageType::ExtrasLanguageChanged),
    MessageTypeToStr(MessageType::ExtrasKeyBindChanged),
    MessageTypeToStr(MessageType::ExtrasChatMessage),
    MessageTypeToStr(MessageType::SquadStatus),
    MessageTypeToStr(MessageType::SquadAdd),
    MessageTypeToStr(MessageType::SquadUpdate),
    MessageTypeToStr(MessageType::SquadRemove)};

//
// Implementation for matching MessageInfo types to MessageCategory.
//

template <MessageCategory Category>
struct MatchTypeToCategory
{
    template <MessageType Type>
    [[maybe_unused]] static constexpr bool Match() noexcept
    {
        return false;
    }
};

template <MessageType... Types>
struct [[maybe_unused]] MsgTypeMatcher
{
    using Matcher = MsgTypeMatcher<Types...>;

    template <MessageType Type>
    static constexpr bool Match() noexcept
    {
        return ((Types == Type) || ...);
    }
};

//
// Specialized Matchers.
//

template <>
struct MatchTypeToCategory<MessageCategory::Info>
    : MsgTypeMatcher<MessageType::ConnectionStatus, MessageType::BridgeInfo, MessageType::Status, MessageType::Closing>
{};

template <>
struct MatchTypeToCategory<MessageCategory::Combat> : MsgTypeMatcher<MessageType::CombatEvent>
{};

template <>
struct MatchTypeToCategory<MessageCategory::Extras>
    : MsgTypeMatcher<MessageType::ExtrasSquadUpdate, MessageType::ExtrasKeyBindChanged,
                     MessageType::ExtrasLanguageChanged, MessageType::ExtrasChatMessage>
{};

template <>
struct MatchTypeToCategory<MessageCategory::Squad> : MsgTypeMatcher<MessageType::SquadStatus, MessageType::SquadAdd,
                                                                    MessageType::SquadUpdate, MessageType::SquadRemove>
{};

//
// Matcher.
//

template <MessageCategory Category, MessageType Type>
struct MatchCategoryAndType
    : std::integral_constant<bool, MatchTypeToCategory<Category>::Matcher::template Match<Type>()>
{};

//
// MessageProtocol.
// Types of message protocols supported.
//

enum class MessageProtocol : uint8_t
{
    Serial = 1,
    JSON = 2
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

template <MessageProtocol Protocol>
constexpr bool IsProtocolBitSet(std::underlying_type_t<MessageProtocol> bits) noexcept
{
    const auto protocolBit = static_cast<std::underlying_type_t<MessageProtocol>>(Protocol);
    return bits & protocolBit;
}

//
// SerialData.
//

struct SerialData
{
    std::shared_ptr<uint8_t[]> ptr{nullptr};
    std::size_t count{0};
};

inline bool operator==(const SerialData& lhs, const SerialData& rhs)
{
    if ((lhs.ptr == rhs.ptr) && (lhs.count == rhs.count))
        return true;

    if ((lhs.count != rhs.count) || !lhs.ptr || !rhs.ptr)
        return false;

    const auto end = static_cast<std::ptrdiff_t>(lhs.count);
    for (std::ptrdiff_t i{0}; i < end; ++i)
        if (lhs.ptr[i] != rhs.ptr[i])
            return false;

    return true;
}

// Get ms since epoch.
inline uint64_t GetMillisecondsSinceEpoch()
{
    const auto tp = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

template <typename T>
inline uint8_t* serial_w_integral(uint8_t* storage, T val)
{
    static_assert(std::is_integral<T>::value, "Integral required.");
    constexpr std::size_t count{sizeof(T)};
    std::memcpy(storage, &val, count);
    return storage + count;
}

inline uint8_t* serial_w_string(uint8_t* storage, const char* data, std::size_t count)
{
    std::memcpy(storage, data, count);
    storage[count] = '\0';
    return storage + count + 1;
}

//
// Message class.
//

class Message
{
public:
    Message() = default;
    Message(MessageProtocol protocol, MessageCategory category, MessageType type, uint64_t id, uint64_t timestamp)
        : m_id{id},
          m_timestamp{timestamp},
          m_protocol{static_cast<std::underlying_type_t<MessageProtocol>>(protocol)},
          m_category{static_cast<std::underlying_type_t<MessageCategory>>(category)},
          m_type{static_cast<std::underlying_type_t<MessageType>>(type)}
    {}
    virtual ~Message() = default;

    // Does the message have a category and type? (is not related to if the msg has any data).
    [[nodiscard]] bool valid() const noexcept { return m_category && m_type; }

    [[nodiscard]] MessageProtocol protocol() const noexcept { return static_cast<MessageProtocol>(m_protocol); }
    [[nodiscard]] MessageCategory category() const noexcept { return static_cast<MessageCategory>(m_category); }
    [[nodiscard]] MessageType type() const noexcept { return static_cast<MessageType>(m_type); }

    [[nodiscard]] uint64_t id() const noexcept { return m_id; }
    [[nodiscard]] uint64_t timestamp() const noexcept { return m_timestamp; }

    // Number of bytes reserved for message header.
    [[nodiscard]] static constexpr std::ptrdiff_t HeaderByteCount() noexcept
    {
        return sizeof(m_id) + sizeof(m_timestamp) + sizeof(m_category) + sizeof(m_type);
    }

    // Data retriever to implement.
    [[nodiscard]] virtual const uint8_t* data() const noexcept { return nullptr; }

    // Data size retriever to implement.
    [[nodiscard]] virtual std::size_t count() const noexcept { return 0; }

private:
    uint64_t m_id{};
    uint64_t m_timestamp{};
    std::underlying_type_t<MessageProtocol> m_protocol{0};
    std::underlying_type_t<MessageCategory> m_category{0};
    std::underlying_type_t<MessageType> m_type{0};
};

//
// Serial Message class.
//

class MessageSerial : public Message
{
public:
    MessageSerial(MessageCategory mc, MessageType mt, uint64_t mid, uint64_t mts)
        : Message(MessageProtocol::Serial, mc, mt, mid, mts),
          m_serial{}
    {
        setNoDataSerial();
    }
    MessageSerial(MessageCategory mc, MessageType mt, uint64_t mid, uint64_t mts, SerialData serial)
        : Message(MessageProtocol::Serial, mc, mt, mid, mts),
          m_serial{std::move(serial)}
    {
        setSerialHeaders();
    }
    virtual ~MessageSerial() = default;

    // Data retriever to implement.
    [[nodiscard]] const uint8_t* data() const noexcept override { return m_serial.ptr.get(); }

    // Data size retriever to implement.
    [[nodiscard]] std::size_t count() const noexcept override { return m_serial.count; }

private:
    void setNoDataSerial()
    {
        m_serial.count = static_cast<std::size_t>(HeaderByteCount());
        m_serial.ptr = std::make_shared<uint8_t[]>(m_serial.count);
        setSerialHeaders();
    }

    void setSerialHeaders() const
    {
        // Set first two bytes in serial data.
        // Narrow cast is safe here. 0 <= DataOffset() <= max of std::size_t.
        if (m_serial.count >= static_cast<std::size_t>(HeaderByteCount()) && m_serial.ptr)
        {
            const auto c{static_cast<std::underlying_type_t<MessageCategory>>(category())};
            const auto t{static_cast<std::underlying_type_t<MessageType>>(type())};

            uint8_t* storage{serial_w_integral(&m_serial.ptr[0], c)};
            storage = serial_w_integral(storage, t);
            storage = serial_w_integral(storage, id());
            serial_w_integral(storage, timestamp());
        }
    }

private:
    SerialData m_serial{};
};

//
// JSON Message class.
//

class MessageJSON : public Message
{
public:
    MessageJSON(MessageCategory mc, MessageType mt, uint64_t mid, uint64_t mts)
        : Message(MessageProtocol::JSON, mc, mt, mid, mts)
    {
        setNoDataJSON();
    }
    MessageJSON(MessageCategory mc, MessageType mt, uint64_t mid, uint64_t mts, const nlohmann::json& data)
        : Message(MessageProtocol::JSON, mc, mt, mid, mts)
    {
        setJSON(data);
    }
    virtual ~MessageJSON() = default;

    // Data retriever to implement.
    [[nodiscard]] const uint8_t* data() const noexcept override
    {
        return reinterpret_cast<const uint8_t*>(m_json.data());
    }

    // Data size retriever to implement.
    [[nodiscard]] std::size_t count() const noexcept override { return m_json.size(); }

private:
    void setNoDataJSON()
    {
        // Generate json header for message.
        m_json = nlohmann::json{{"category", MessageCategoryToStr(category())},
                                {"type", MessageTypeToStr(type())},
                                {"id", id()},
                                {"timestamp", timestamp()}}
                     .dump();
    }

    void setJSON(const nlohmann::json& data)
    {
        // Generate json header for message.
        m_json = nlohmann::json{
            {"category", MessageCategoryToStr(category())},
            {"type", MessageTypeToStr(type())},
            {"id", id()},
            {"timestamp", timestamp()},
            {"data",
             data}}.dump();
    }

private:
    std::string m_json{};
};

// Creates a SerialData object to hold count bytes + the serial header bytes.
inline SerialData CreateSerialData(std::size_t count)
{
    const std::size_t byte_count = Message::HeaderByteCount() + count;
    return {std::make_shared<uint8_t[]>(byte_count), byte_count};
}

//
// Get Message class type depending on MessageProtocol.
//

template <MessageProtocol Protocol>
struct GetMessageClass : std::false_type
{};

template <>
struct GetMessageClass<MessageProtocol::Serial> : std::true_type
{
    using Type = MessageSerial;
};

template <>
struct GetMessageClass<MessageProtocol::JSON> : std::true_type
{
    using Type = MessageJSON;
};

//
// Message create functions.
//

template <MessageProtocol Protocol, MessageCategory Category, MessageType Type, typename... Args>
std::shared_ptr<Message> CreateMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<Category, Type>::value, "MessageCategory and MessageType does not have a match");
    static_assert(GetMessageClass<Protocol>::value, "Can not get MessageClass from provided protocol");

    return std::make_shared<typename GetMessageClass<Protocol>::Type>(Category, Type, std::forward<Args>(args)...);
}

template <MessageProtocol Protocol, MessageType Type, typename... Args>
std::shared_ptr<Message> InfoMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<MessageCategory::Info, Type>::value, "Type is not an Info message");

    return CreateMessage<Protocol, MessageCategory::Info, Type>(std::forward<Args>(args)...);
}

template <MessageProtocol Protocol, MessageType Type, typename... Args>
std::shared_ptr<Message> CombatMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<MessageCategory::Combat, Type>::value, "Type is not a Combat message");

    return CreateMessage<Protocol, MessageCategory::Combat, Type>(std::forward<Args>(args)...);
}

template <MessageProtocol Protocol, MessageType Type, typename... Args>
std::shared_ptr<Message> ExtrasMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<MessageCategory::Extras, Type>::value, "Type is not an Extras message");

    return CreateMessage<Protocol, MessageCategory::Extras, Type>(std::forward<Args>(args)...);
}

template <MessageProtocol Protocol, MessageType Type, typename... Args>
std::shared_ptr<Message> SquadMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<MessageCategory::Squad, Type>::value, "Type is not a Squad message");

    return CreateMessage<Protocol, MessageCategory::Squad, Type>(std::forward<Args>(args)...);
}

#endif // BRIDGE_MESSAGE_HPP
