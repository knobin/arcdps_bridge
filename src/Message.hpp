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
    BridgeInfo = 1,
    Status = 2,
    Closing = 3,

    // ArcDPS combat api types.
    CombatEvent = 4,

    // Extras event types.
    ExtrasSquadUpdate = 5,
    ExtrasLanguageChanged = 6,
    ExtrasKeyBindChanged = 7,
    ExtrasChatMessage = 8,

    // Squad event types.
    SquadStatus = 9,
    SquadAdd = 10,
    SquadUpdate = 11,
    SquadRemove = 12
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
    : MsgTypeMatcher<MessageType::BridgeInfo, MessageType::Status, MessageType::Closing>
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
    Message(MessageCategory category, MessageType type, uint64_t id, uint64_t timestamp, bool serial = true,
            bool json = true)
        : m_id{id},
          m_timestamp{timestamp},
          m_category{static_cast<std::underlying_type_t<MessageCategory>>(category)},
          m_type{static_cast<std::underlying_type_t<MessageType>>(type)}
    {
        // Message does not contain any data.
        // Generate headers if needed.

        if (serial)
            setNoDataSerial();
        if (json)
            setNoDataJSON();
    }
    Message(MessageCategory category, MessageType type, uint64_t id, uint64_t timestamp, const SerialData& serial)
        : m_id{id},
          m_timestamp{timestamp},
          m_category{static_cast<std::underlying_type_t<MessageCategory>>(category)},
          m_type{static_cast<std::underlying_type_t<MessageType>>(type)},
          m_serial{serial}
    {
        setSerialHeaders();
    }
    Message(MessageCategory category, MessageType type, uint64_t id, uint64_t timestamp, const nlohmann::json& jdata)
        : m_id{id},
          m_timestamp{timestamp},
          m_category{static_cast<std::underlying_type_t<MessageCategory>>(category)},
          m_type{static_cast<std::underlying_type_t<MessageType>>(type)}
    {
        setJSON(jdata);
    }
    Message(MessageCategory category, MessageType type, uint64_t id, uint64_t timestamp, const SerialData& serial,
            const nlohmann::json& jdata)
        : m_id{id},
          m_timestamp{timestamp},
          m_category{static_cast<std::underlying_type_t<MessageCategory>>(category)},
          m_type{static_cast<std::underlying_type_t<MessageType>>(type)},
          m_serial{serial}
    {
        setJSON(jdata);
        setSerialHeaders();
    }
    virtual ~Message() = default;

    [[nodiscard]] const SerialData& toSerial() const { return m_serial; }
    [[nodiscard]] const std::string& toJSON() const { return m_json; }

    [[nodiscard]] bool hasSerial() const noexcept { return static_cast<bool>(m_serial.count); }
    [[nodiscard]] bool hasJSON() const noexcept { return !m_json.empty(); }

    [[nodiscard]] bool empty() const noexcept { return !m_category || !m_type; }

    [[nodiscard]] MessageCategory category() const noexcept { return static_cast<MessageCategory>(m_category); }
    [[nodiscard]] MessageType type() const noexcept { return static_cast<MessageType>(m_type); }

    [[nodiscard]] uint64_t id() const noexcept { return m_id; }
    [[nodiscard]] uint64_t timestamp() const noexcept { return m_timestamp; }

    // Number of bytes reserved for message header.
    [[nodiscard]] static constexpr std::ptrdiff_t DataOffset() noexcept
    {
        return sizeof(m_id) + sizeof(m_timestamp) + sizeof(m_category) + sizeof(m_type);
    }

private:
    void setNoDataSerial()
    {
        m_serial.count = static_cast<std::size_t>(DataOffset());
        m_serial.ptr = std::make_shared<uint8_t[]>(m_serial.count);
        setSerialHeaders();
    }

    void setNoDataJSON()
    {
        // Generate json header for message.
        m_json = nlohmann::json{{"category", MessageCategoryToStr(category())},
                                {"type", MessageTypeToStr(type())},
                                {"id", m_id},
                                {"timestamp", m_timestamp}}
                     .dump();
    }

    void setJSON(const nlohmann::json& data)
    {
        // Generate json header for message.
        m_json = nlohmann::json{
            {"category", MessageCategoryToStr(category())},
            {"type", MessageTypeToStr(type())},
            {"id", m_id},
            {"timestamp", m_timestamp},
            {"data",
             data}}.dump();
    }

    void setSerialHeaders() const
    {
        // Set first two bytes in serial data.
        if (m_serial.count >= DataOffset() && m_serial.ptr)
        {
            uint8_t* storage{serial_w_integral(&m_serial.ptr[0], m_category)};
            storage = serial_w_integral(storage, m_type);
            storage = serial_w_integral(storage, m_id);
            serial_w_integral(storage, m_timestamp);
        }
    }

private:
    SerialData m_serial{};
    std::string m_json{};
    uint64_t m_id{};
    uint64_t m_timestamp{};
    std::underlying_type_t<MessageCategory> m_category{0};
    std::underlying_type_t<MessageType> m_type{0};
};

// Creates a SerialData object to hold count bytes + the serial header bytes.
inline SerialData CreateSerialData(std::size_t count)
{
    const std::size_t byte_count = Message::DataOffset() + count;
    return {std::make_shared<uint8_t[]>(byte_count), byte_count};
}

//
// Message create functions.
//

template <MessageCategory Category, MessageType Type, typename... Args>
Message CreateMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<Category, Type>::value, "MessageCategory and MessageType does not have a match");

    return Message{Category, Type, std::forward<Args>(args)...};
}

template <MessageType Type, typename... Args>
Message InfoMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<MessageCategory::Info, Type>::value, "Type is not an Info message");

    return CreateMessage<MessageCategory::Info, Type>(std::forward<Args>(args)...);
}

template <MessageType Type, typename... Args>
Message CombatMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<MessageCategory::Combat, Type>::value, "Type is not a Combat message");

    return CreateMessage<MessageCategory::Combat, Type>(std::forward<Args>(args)...);
}

template <MessageType Type, typename... Args>
Message ExtrasMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<MessageCategory::Extras, Type>::value, "Type is not an Extras message");

    return CreateMessage<MessageCategory::Extras, Type>(std::forward<Args>(args)...);
}

template <MessageType Type, typename... Args>
Message SquadMessage(Args&&... args)
{
    static_assert(MatchCategoryAndType<MessageCategory::Squad, Type>::value, "Type is not a Squad message");

    return CreateMessage<MessageCategory::Squad, Type>(std::forward<Args>(args)...);
}

#endif // BRIDGE_MESSAGE_HPP
