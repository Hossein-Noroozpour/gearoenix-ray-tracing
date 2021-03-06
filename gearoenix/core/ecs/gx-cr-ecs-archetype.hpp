#ifndef GEAROENIX_CORE_ECS_ARCHETYPE_HPP
#define GEAROENIX_CORE_ECS_ARCHETYPE_HPP
#include "../../platform/gx-plt-log.hpp"
#include "../gx-cr-build-configuration.hpp"
#include "../gx-cr-range.hpp"
#include "../sync/gx-cr-sync-parallel-for.hpp"
#include "gx-cr-ecs-component.hpp"
#include "gx-cr-ecs-entity.hpp"
#include "gx-cr-ecs-not.hpp"
#include "gx-cr-ecs-types.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <type_traits>
#include <vector>

namespace gearoenix::core::ecs {
struct World;
struct Archetype final {
    friend struct World;

private:
    typedef std::vector<std::type_index> id_t;
    typedef std::pair<std::type_index, std::size_t> component_index_t;
    typedef std::vector<component_index_t> components_indices_t;

    constexpr static std::size_t header_size = sizeof(Entity::id_t);

    const components_indices_t components_indices;
    const std::size_t entity_size;
    std::vector<std::uint8_t> data;

    constexpr static auto component_index_less = +[](const component_index_t& l, const component_index_t& r) constexpr noexcept
    {
        return std::less()(l.first, r.first);
    };

    template <typename... ComponentsTypes>
    [[nodiscard]] static id_t create_id() noexcept
    {
        Component::types_check<ComponentsTypes...>();
        id_t id {
            Component::create_type_index<ComponentsTypes>()...,
        };
        std::sort(id.begin(), id.end());
#ifdef GX_DEBUG_MODE
        for (std::size_t i = 0, j = 1; j < sizeof...(ComponentsTypes); ++i, ++j)
            if (id[i] == id[j])
                GX_LOG_F("Duplicated components are not allowed.")
#endif
        return id;
    }

    template <typename... ComponentsTypes>
    [[nodiscard]] bool satisfy() noexcept
    {
        Component::query_types_check<ComponentsTypes...>();
        bool result = true;
        (([&]<typename T>(T*) constexpr noexcept {
            if (result) {
                result &= (is_not<T> != std::binary_search(components_indices.begin(), components_indices.end(), std::make_pair(Component::create_type_index<T>(), 0), component_index_less));
            }
        }(reinterpret_cast<ComponentsTypes*>(0))),
            ...);
        return result;
    }

    template <typename... ComponentsTypes>
    [[nodiscard]] constexpr static std::size_t get_components_size() noexcept
    {
        Component::types_check<ComponentsTypes...>();
        std::size_t size = 0;
        ((size += sizeof(ComponentsTypes)), ...);
        return size;
    }

    [[nodiscard]] static std::size_t get_components_size(const EntityBuilder::components_t&) noexcept;

    template <typename... ComponentsTypes>
    [[nodiscard]] static components_indices_t get_components_indices() noexcept
    {
        Component::types_check<ComponentsTypes...>();
        std::size_t index = 0;
        components_indices_t indices {
            { Component::create_type_index<ComponentsTypes>(), sizeof(ComponentsTypes) }...,
        };
        std::sort(indices.begin(), indices.end(), component_index_less);
        for (auto& i : indices) {
            auto s = i.second;
            i.second = index;
            index += s;
        }
        return indices;
    }

    [[nodiscard]] static components_indices_t get_components_indices(const EntityBuilder::components_t&) noexcept;

    explicit Archetype(const EntityBuilder::components_t&) noexcept;
    Archetype(std::size_t, components_indices_t&&) noexcept;

    template <typename... ComponentsTypes>
    [[nodiscard]] static Archetype create() noexcept
    {
        Component::types_check<ComponentsTypes...>();
        return Archetype(get_components_size<ComponentsTypes...>(), get_components_indices<ComponentsTypes...>());
    }

    [[nodiscard]] std::uint8_t* allocate_size(std::size_t) noexcept;

    template <typename T>
    T& allocate() noexcept
    {
        return *reinterpret_cast<T*>(allocate_size(sizeof(T)));
    }

    void allocate_entity(Entity::id_t) noexcept;

    [[nodiscard]] std::size_t allocate_entity(Entity::id_t, const EntityBuilder::components_t&) noexcept;

    template <typename... ComponentsTypes>
    [[nodiscard]] std::size_t allocate(const Entity::id_t id, ComponentsTypes&&... components) noexcept
    {
        Component::types_check<ComponentsTypes...>();
        allocate_entity(id);
        const auto result = data.size();
        if constexpr (sizeof...(ComponentsTypes) > 0) {
            auto* const ptr = allocate_size(get_components_size<ComponentsTypes...>());
            ((new (&ptr[get_component_index<ComponentsTypes>()]) ComponentsTypes(std::move(components))), ...);
        }
        return result;
    }

    std::optional<std::pair<Entity::id_t, std::size_t>> remove_entity(std::size_t index) noexcept;

    template <typename T>
    [[nodiscard]] std::size_t get_component_index() noexcept
    {
        Component::query_type_check<T>();
        const auto search = std::lower_bound(
            components_indices.begin(),
            components_indices.end(),
            std::make_pair(Component::create_type_index<T>(), 0),
            component_index_less);
#ifdef GX_DEBUG_MODE
        if (components_indices.end() == search || search->first != Component::create_type_index<T>())
            GX_LOG_F("Component '" << typeid(T).name() << "' not found in this archetype.")
#endif
        return search->second;
    }

    template <typename ComponentType>
    [[nodiscard]] ComponentType& get_component(const std::size_t index) noexcept
    {
        Component::type_check<ComponentType>();
        return *reinterpret_cast<ComponentType*>(&data[index + get_component_index<ComponentType>()]);
    }

    void move_out_entity(std::size_t index, EntityBuilder::components_t& components) noexcept;

    [[nodiscard]] std::optional<std::pair<Entity::id_t, std::size_t>> move_from_back(std::size_t index) noexcept;

    template <typename... C, std::size_t... I, std::size_t N, typename F>
    static inline void call_function(
        std::index_sequence<I...> const&,
        const Entity::id_t id,
        std::uint8_t* const ptr,
        const std::size_t (&indices)[N],
        F f,
        const unsigned int kernel_index) noexcept
    {
        f(id, *reinterpret_cast<C*>(&ptr[indices[I]])..., kernel_index);
    }

    template <typename... ComponentsTypes, typename F>
    void parallel_system(F fun) noexcept
    {
        const std::size_t indices[] = {
            (is_not<ComponentsTypes> ? 0 : (sizeof(Entity::id_t) + get_component_index<ComponentsTypes>()))...,
        };
        auto range = PtrRange(data.data(), data.size(), entity_size);
        sync::ParallelFor::exec(range.begin(), range.end(), [&](std::uint8_t* ptr, const unsigned int kernel_index) noexcept {
            const auto id = *reinterpret_cast<const Entity::id_t*>(ptr);
            call_function<ComponentsTypes...>(
                std::make_index_sequence<sizeof...(ComponentsTypes)> {}, id, ptr, indices, fun, kernel_index);
        });
    }

public:
    Archetype(Archetype&&) noexcept;
    Archetype(const Archetype&) = delete;
    ~Archetype() noexcept;
};
}

#endif