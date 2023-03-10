//
//  tests/FuzzHelper.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-10.
//

#define NOMINMAX

// Catch2 Headers
#include "catch2/catch_test_macros.hpp"

// Bridge Headers
#include "Message.hpp"

// C++ Headers
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <random>

//
// Base Node to inherit from.
//

struct Node
{
    virtual ~Node() = default;
    virtual uint8_t* write(uint8_t* storage) = 0;
    virtual uint8_t* require(uint8_t* storage) = 0;
    virtual void json_require() {}
    [[nodiscard]] virtual std::size_t count() const = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

//
// Integral node and related random generator functions.
//

template <typename T, T Min = std::numeric_limits<T>::min(), T Max = std::numeric_limits<T>::max()>
inline T RandomIntegral()
{
    static std::random_device dev;
    static std::mt19937_64 engine(dev());
    static std::uniform_int_distribution<T> distr(Min, Max);
    return distr(engine);
}

template <>
inline int8_t RandomIntegral()
{
    return static_cast<int8_t>(RandomIntegral<int16_t>());
}

template <>
inline uint8_t RandomIntegral()
{
    return static_cast<int8_t>(RandomIntegral<uint16_t>());
}

template <typename T>
inline uint8_t* RequireAtLocation(uint8_t* storage, T val)
{
    static_assert(std::is_integral<T>::value, "Integral required.");
    T* location = reinterpret_cast<T*>(storage);
    REQUIRE(*location == val);
    return storage + sizeof(T);
}

template <typename T>
struct IntegralNode : Node
{
    explicit IntegralNode(T val)
        : value{val}
    {}

    T value{};

    uint8_t* write(uint8_t* storage) override { return serial_w_integral(storage, value); }
    uint8_t* require(uint8_t* storage) override { return RequireAtLocation(storage, value); }
    [[nodiscard]] std::size_t count() const override { return sizeof(T); }
};

static const std::array<std::function<std::unique_ptr<Node>()>, 8> IntegralCreators{
    []() {
    return std::make_unique<IntegralNode<int64_t>>(RandomIntegral<int64_t>());
    },
    []() {
    return std::make_unique<IntegralNode<uint64_t>>(RandomIntegral<uint64_t>());
},
    []() {
    return std::make_unique<IntegralNode<int32_t>>(RandomIntegral<int32_t>());
    },
    []() {
    return std::make_unique<IntegralNode<uint32_t>>(RandomIntegral<uint32_t>());
    },
    []() {
    return std::make_unique<IntegralNode<int16_t>>(RandomIntegral<int16_t>());
    },
    []() {
    return std::make_unique<IntegralNode<uint16_t>>(RandomIntegral<uint16_t>());
    },
    []() {
    return std::make_unique<IntegralNode<int8_t>>(static_cast<int8_t>(RandomIntegral<int16_t>()));
    },
    []() {
    return std::make_unique<IntegralNode<uint8_t>>(static_cast<uint8_t>(RandomIntegral<uint16_t>()));
    },
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

//
// String node and related random generator functions.
//

inline uint8_t* RequireStringAtLocation(uint8_t* storage, const char* str, std::size_t count)
{
    const char* location = reinterpret_cast<const char*>(storage);
    for (std::size_t i{0}; i < count; ++i)
        REQUIRE(location[i] == str[i]);
    REQUIRE(location[count] == '\0');
    return storage + count + 1;
}

struct StringNode : Node
{
    explicit StringNode(std::string str)
        : value{std::move(str)}
    {}

    std::string value{};

    uint8_t* write(uint8_t* storage) override { return serial_w_string(storage, value.c_str(), value.size()); }
    uint8_t* require(uint8_t* storage) override
    {
        return RequireStringAtLocation(storage, value.c_str(), value.size());
    }
    [[nodiscard]] std::size_t count() const override { return value.size() + 1; }
};

constexpr std::size_t MaxStringSize = 2048;

template <std::size_t Min = std::numeric_limits<std::size_t>::min(), std::size_t Max = MaxStringSize>
inline std::string RandomString()
{
    // Should extend the valid alpha set.
    constexpr std::string_view alpha = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz ()[]{}&%#";

    const auto count = RandomIntegral<std::size_t, Min, Max>();
    std::string str{};
    str.reserve(count);

    for (std::size_t i{0}; i < count; ++i)
        str += alpha[RandomIntegral<std::size_t>() % alpha.size()];

    return std::move(str);
}

template <std::size_t Min = std::numeric_limits<std::size_t>::min(), std::size_t Max = MaxStringSize>
inline std::optional<std::string> OptionalRandomString()
{
    if (RandomIntegral<uint64_t>() % 2)
        return RandomString<Min, Max>();

    return std::nullopt;
}

inline std::unique_ptr<StringNode> StringNodeCreator()
{
    return std::make_unique<StringNode>(RandomString());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

//
// Main general fuzzing function to run tests with.
//

template <std::size_t MaxTests, std::size_t MaxNodes, std::size_t MinScale, typename Creator>
inline void BudgetFuzzer(Creator func)
{
    static_assert(MinScale > 1, "MinScale cannot be under 1");
    const std::size_t tests = RandomIntegral<std::size_t, 0, MaxTests>();
    for (std::size_t i{0}; i < tests; ++i)
    {
        const std::size_t node_count = RandomIntegral<std::size_t, MaxNodes / MinScale, MaxNodes>();
        std::vector<std::unique_ptr<Node>> nodes{};
        nodes.reserve(node_count);
        std::size_t count = 0;

        // Create nodes.
        for (std::size_t j{0}; j < node_count; ++j)
        {
            std::unique_ptr<Node> node = func();
            count += node->count();
            nodes.push_back(std::move(node));
        }

        // Allocate buffer and write values.
        std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(count);
        uint8_t* location = &buffer[0];
        for (std::size_t j{0}; j < nodes.size(); ++j)
            location = nodes[j]->write(location);

        // Validate.
        location = &buffer[0];
        for (std::size_t j{0}; j < nodes.size(); ++j)
        {
            location = nodes[j]->require(location); // Serial.
            nodes[j]->json_require();
        }

        // Validate pointer location (== +1 of last storage position).
        REQUIRE(buffer.get() + count == location);
    }
}
