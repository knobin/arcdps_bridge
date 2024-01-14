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
// Helpers.
//

// Get ms since epoch.
inline uint64_t GetMillisecondsSinceEpoch()
{
    const auto tp = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

template <typename T>
constexpr uint8_t* serial_w_integral(uint8_t* storage, T val)
{
    static_assert(std::is_integral<T>::value, "Integral required.");
    constexpr std::size_t count{sizeof(T)};
    std::memcpy(storage, &val, count);
    return storage + count;
}

// String can only be 4294967295 characters long (including null terminator).
inline uint8_t* serial_w_string(uint8_t* storage, const char* data, uint32_t count)
{
    std::memcpy(storage, data, count);
    storage[count] = '\0';
    return storage + count + 1;
}

//
// Message Header.
//

struct MessageHeader
{
    std::underlying_type_t<MessageCategory> category{0};
    std::underlying_type_t<MessageType> type{0};
    uint64_t id{0};
    uint64_t timestamp{0};
    uint64_t length{0};
};

inline bool operator==(const MessageHeader& lhs, const MessageHeader& rhs) noexcept
{
    return (lhs.category == rhs.category) && (lhs.type == rhs.type) && (lhs.id == rhs.id) && (lhs.timestamp == rhs.timestamp);
}

// Number of bytes reserved for message header.
[[nodiscard]] static constexpr std::ptrdiff_t MessageHeaderByteCount() noexcept
{
    return sizeof(MessageCategory) +    // Category
           sizeof(MessageType) +        // Type
           sizeof(uint64_t) +           // id
           sizeof(uint64_t) +           // timestamp
           sizeof(uint64_t);            // length
}

//
// Message Data.
//

class MemoryLocation
{
public:
    MemoryLocation() = default;
    explicit constexpr MemoryLocation(uint8_t *buffer)
        : m_start{buffer}, m_buffer{buffer}
    {}
    ~MemoryLocation() = default;

    template <typename T>
    constexpr uint8_t* writeIntegral(T value)
    {
        m_buffer = serial_w_integral(m_buffer, value);
        return m_buffer;
    }

    uint8_t* writeString(const char* data, uint32_t count)
    {
        m_buffer = serial_w_string(m_buffer, data, count);
        return m_buffer;
    }

    uint8_t* writeBuffer(const void *data, uint32_t count)
    {
        std::memcpy(m_buffer, data, count);
        m_buffer += count;
        return m_buffer;
    }

    [[nodiscard]] constexpr uint8_t* start() const noexcept { return m_start; }
    [[nodiscard]] constexpr uint8_t* head() const noexcept { return m_buffer; }

private:
    uint8_t *m_start{nullptr};
    uint8_t *m_buffer{nullptr};
};

[[nodiscard]] constexpr uint16_t MemoryHeadOffset(const MemoryLocation& fixed, const MemoryLocation& dynamic) noexcept
{
    return static_cast<uint16_t>(dynamic.head() - fixed.head());
}

struct MessageBuffer
{
    std::shared_ptr<uint8_t[]> ptr{nullptr};
    std::size_t count{0};
    uint8_t* start{nullptr};

    static MessageBuffer Create(std::size_t count)
    {
        const std::size_t byte_count{static_cast<std::size_t>(MessageHeaderByteCount()) + count};
        std::shared_ptr<uint8_t[]> ptr{std::make_shared<uint8_t[]>(byte_count)};
        return {ptr, byte_count, ptr.get() + MessageHeaderByteCount()};
    }
};

inline bool operator==(const MessageBuffer& lhs, const MessageBuffer& rhs)
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

//
// Message class.
//

class Message
{
public:
    Message() = default;
    Message(MessageHeader header, MessageBuffer buffer)
        : m_header{header},
            m_buffer{std::move(buffer)}
    {
        if (m_buffer.ptr == nullptr)
        {
            // Allocate header for empty message.
            m_buffer = MessageBuffer::Create(0);
        }

        // Set Header data in message.
        // Narrow cast is safe here. 0 <= DataOffset() <= max of std::size_t.
        if (m_buffer.count >= static_cast<std::size_t>(MessageHeaderByteCount()) && m_buffer.ptr)
        {
            const auto c{static_cast<std::underlying_type_t<MessageCategory>>(category())};
            const auto t{static_cast<std::underlying_type_t<MessageType>>(type())};

            uint8_t* storage{serial_w_integral(&m_buffer.ptr[0], c)};
            storage = serial_w_integral(storage, t);
            storage = serial_w_integral(storage, id());
            storage = serial_w_integral(storage, timestamp());
            serial_w_integral(storage, static_cast<uint64_t>(m_buffer.count));
        }
    }
    virtual ~Message() = default;

    // Can the message be sent? (valid header + buffer > 0).
    [[nodiscard]] bool valid() const noexcept
    {
        return m_header.category && m_header.type && m_buffer.ptr && m_buffer.count;
    }

    [[nodiscard]] MessageCategory category() const noexcept { return static_cast<MessageCategory>(m_header.category); }
    [[nodiscard]] MessageType type() const noexcept { return static_cast<MessageType>(m_header.type); }

    [[nodiscard]] uint64_t id() const noexcept { return m_header.id; }
    [[nodiscard]] uint64_t timestamp() const noexcept { return m_header.timestamp; }

    // Data retriever.
    [[nodiscard]] const uint8_t* data() const noexcept { return m_buffer.ptr.get(); }

    // Data size retriever.
    [[nodiscard]] std::size_t count() const noexcept { return m_buffer.count; }

private:
    MessageHeader m_header{};
    MessageBuffer m_buffer{};
};

//
// Message create functions.
//

template <MessageCategory Category, MessageType Type>
Message CreateMessage(uint64_t id, uint64_t timestamp, const MessageBuffer& buffer)
{
    static_assert(MatchCategoryAndType<Category, Type>::value, "MessageCategory and MessageType does not have a match");

    constexpr auto c{static_cast<std::underlying_type_t<MessageCategory>>(Category)};
    constexpr auto t{static_cast<std::underlying_type_t<MessageType>>(Type)};

    const MessageHeader header{c, t, id, timestamp};
    return Message{header, buffer};
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
