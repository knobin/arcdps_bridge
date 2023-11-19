//
//  src/Combat.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-08.
//

#ifndef BRIDGE_COMBAT_HPP
#define BRIDGE_COMBAT_HPP

// Local Headers
#include "Message.hpp"

// C++ Headers
#include <cstdint>
#include <cstring>

/* arcdps export table */
struct arcdps_exports
{
    uintptr_t size;        /* size of exports table */
    uint32_t sig;          /* pick a number between 0 and uint32_t max that isn't used by other modules */
    uint32_t imguivers;    /* set this to IMGUI_VERSION_NUM. if you don't use imgui, 18000 (as of 2021-02-02) */
    const char* out_name;  /* name string */
    const char* out_build; /* build string */
    void* wnd_nofilter; /* wndproc callback, fn(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam), return assigned to
                           umsg */
    void* combat; /* combat event callback, fn(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t
                     revision) */
    void* imgui;  /* ::present callback, before imgui::render, fn(uint32_t not_charsel_or_loading) */
    void* options_end;  /* ::present callback, appending to the end of options window in arcdps, fn() */
    void* combat_local; /* combat event callback like area but from chat log, fn(cbtevent* ev, ag* src, ag* dst, char*
                           skillname, uint64_t id, uint64_t revision) */
    void* wnd_filter;   /* wndproc callback like wnd_nofilter above, input filered using modifiers */
    void* options_windows; /* called once per 'window' option checkbox, with null at the end, non-zero return disables
                              arcdps drawing that checkbox, fn(char* windowname) */
};

/* combat event - see evtc docs for details, revision param in combat cb is equivalent of revision byte header */
struct cbtevent
{
    uint64_t time;
    uint64_t src_agent;
    uint64_t dst_agent;
    int32_t value;
    int32_t buff_dmg;
    uint32_t overstack_value;
    uint32_t skillid;
    uint16_t src_instid;
    uint16_t dst_instid;
    uint16_t src_master_instid;
    uint16_t dst_master_instid;
    uint8_t iff;
    uint8_t buff;
    uint8_t result;
    uint8_t is_activation;
    uint8_t is_buffremove;
    uint8_t is_ninety;
    uint8_t is_fifty;
    uint8_t is_moving;
    uint8_t is_statechange;
    uint8_t is_flanking;
    uint8_t is_shields;
    uint8_t is_offcycle;
    uint8_t pad61;
    uint8_t pad62;
    uint8_t pad63;
    uint8_t pad64;
};

/* agent short */
struct ag
{
    char* name;     /* agent name. may be null. valid only at time of event. utf8 */
    uintptr_t id;   /* agent unique identifier */
    uint32_t prof;  /* profession at time of event. refer to evtc notes for identification */
    uint32_t elite; /* elite spec at time of event. refer to evtc notes for identification */
    uint32_t self;  /* 1 if self, 0 if not */
    uint16_t team;  /* sep21+ */
};

namespace Combat
{
    class AgentSerializer
    {
    public:
        AgentSerializer() = delete;
        explicit AgentSerializer(ag *agent)
            : m_agent{agent}
        {
            if (agent->name != nullptr)
                m_strLength = static_cast<uint16_t>(std::strlen(agent->name)) + 1;
        }

        [[nodiscard]] inline std::size_t size() const noexcept
        {
            if (m_agent == nullptr)
                return 0;

            return fixedSize() + dynamicSize();
        }

        [[nodiscard]] static constexpr std::size_t fixedSize() noexcept
        {
            constexpr auto str{2 * sizeof(uint16_t)}; // Str "pointer" and size.
            return str + sizeof(ag::id) + sizeof(ag::prof) + sizeof(ag::elite) + sizeof(ag::self) + sizeof(ag::team);
        }

        [[nodiscard]] std::size_t dynamicSize() const noexcept
        {
            if (m_agent == nullptr)
                return 0;

            return m_strLength;
        }

        [[nodiscard]] MessageBuffers writeToBuffers(MessageBuffers buffers) const
        {
            if (m_agent != nullptr)
            {
                // Dynamic.
                const auto strIndex = static_cast<uint16_t>(buffers.dynamic - buffers.fixed);
                buffers.dynamic = serial_w_string(buffers.dynamic, m_agent->name, m_strLength - 1);

                // String.
                buffers.fixed = serial_w_integral(buffers.fixed, strIndex);
                buffers.fixed = serial_w_integral(buffers.fixed, m_strLength);

                // Fixed.
                buffers.fixed = serial_w_integral(buffers.fixed, static_cast<uint64_t>(m_agent->id));
                buffers.fixed = serial_w_integral(buffers.fixed, m_agent->prof);
                buffers.fixed = serial_w_integral(buffers.fixed, m_agent->elite);
                buffers.fixed = serial_w_integral(buffers.fixed, m_agent->self);
                buffers.fixed = serial_w_integral(buffers.fixed, m_agent->team);
            }
            return buffers;
        }

    private:
        ag *m_agent{nullptr};
        uint16_t m_strLength{0};
    };

    class EventSerializer
    {
    public:
        EventSerializer() = delete;
        EventSerializer(cbtevent* ev, ag* src, ag* dst, char* sn, uint64_t id, uint64_t revision)
            : m_ev{ev}, m_src{src}, m_dst{dst}, m_sn{sn}, m_id{id}, m_revision{revision}
        {
            if (m_sn)
                m_snLength = static_cast<uint16_t>(std::strlen(m_sn)) + 1;
        }

        [[nodiscard]] inline std::size_t size() const noexcept
        {
            return fixedSize() + dynamicSize();
        }

        [[nodiscard]] static constexpr std::size_t fixedSize() noexcept
        {
            constexpr auto sn{2 * sizeof(uint16_t)}; // Str "pointer" and size.
            constexpr auto p{3 * sizeof(uint16_t)}; // "pointer" for ev, src and dst.

            return sn + p + sizeof(m_id) + sizeof(m_revision);
        }

        [[nodiscard]] std::size_t dynamicSize() const noexcept
        {
            // All pointer based variables will be stored on dynamic.
            return m_snLength + ((m_ev) ? sizeof(cbtevent) : 0) + m_src.size() + m_dst.size();
        }

        [[nodiscard]] MessageBuffers writeToBuffers(MessageBuffers buffers) const
        {
            uint16_t evIndex{0};
            uint16_t srcIndex{0};
            uint16_t dstIndex{0};
            uint16_t snIndex{0};

            // Dynamic.
            if (m_ev)
            {
                evIndex = static_cast<uint16_t>(buffers.dynamic - buffers.fixed);
                std::memcpy(buffers.dynamic, m_ev, sizeof(cbtevent));
                buffers.dynamic += sizeof(cbtevent);
            }
            if (m_src.size())
            {
                // Write whole Agent on dynamic.
                srcIndex = static_cast<uint16_t>(buffers.dynamic - buffers.fixed);
                MessageBuffers srcBuffers{buffers.dynamic, buffers.dynamic + AgentSerializer::fixedSize()};
                srcBuffers = m_src.writeToBuffers(srcBuffers);
                buffers.dynamic = srcBuffers.dynamic; // Update local dynamic location.
            }
            if (m_src.size())
            {
                // Write whole Agent on dynamic.
                dstIndex = static_cast<uint16_t>(buffers.dynamic - buffers.fixed);
                MessageBuffers dstBuffers{buffers.dynamic, buffers.dynamic + AgentSerializer::fixedSize()};
                dstBuffers = m_src.writeToBuffers(dstBuffers);
                buffers.dynamic = dstBuffers.dynamic; // Update local dynamic location.
            }
            if (m_sn)
            {
                snIndex = static_cast<uint16_t>(buffers.dynamic - buffers.fixed);
                buffers.dynamic = serial_w_string(buffers.dynamic, m_sn, m_snLength - 1);
            }

            // Fixed.
            buffers.fixed = serial_w_integral(buffers.fixed, evIndex);
            buffers.fixed = serial_w_integral(buffers.fixed, srcIndex);
            buffers.fixed = serial_w_integral(buffers.fixed, dstIndex);
            buffers.fixed = serial_w_integral(buffers.fixed, snIndex);
            buffers.fixed = serial_w_integral(buffers.fixed, m_snLength);
            buffers.fixed = serial_w_integral(buffers.fixed, m_id);
            buffers.fixed = serial_w_integral(buffers.fixed, m_revision);

            return buffers;
        }
    private:
        cbtevent *m_ev{nullptr};
        AgentSerializer m_src;
        AgentSerializer m_dst;
        char *m_sn{nullptr};
        uint16_t m_snLength{0};
        uint64_t m_id{0};
        uint64_t m_revision{0};
    };

} // namespace Combat

#endif // BRIDGE_COMBAT_HPP
