#pragma once

#include <type_traits>
#include <utility>

#include "graphics/Passes.hpp"
#include "graphics/RenderScene.hpp"
#include "graphics/Scene.hpp"

/**
 * Pipeline composition.
 *
 * operator| applies the pass on the right to the scene on the left so a
 * pipeline reads in data-flow order:
 *
 *     auto scene = build_scene()
 *         | transform(model)
 *         | view(camera)
 *         | project(camera)
 *         | cull()
 *         | depth_sort()
 *         | viewport(width, height);
 *
 * It is only syntax: no pass gains behaviour from being composed, and the
 * types still have to line up, so applying a pass to the wrong space is a
 * compile error rather than a silently wrong picture.
 */
namespace rubiks::graphics {

/**
 * Opt-in marker for the types allowed on the left of operator|.
 *
 * Without it the overload would be found by argument-dependent lookup for
 * every pipe expression involving anything in this namespace.
 */
template <typename T>
struct is_pipeline_value : std::false_type {};

template <>
struct is_pipeline_value<WorldScene> : std::true_type {};

template <>
struct is_pipeline_value<ViewScene> : std::true_type {};

template <>
struct is_pipeline_value<ClipScene> : std::true_type {};

template <>
struct is_pipeline_value<RenderScene> : std::true_type {};

template <typename Scene, typename Pass,
          typename = std::enable_if_t<
              is_pipeline_value<std::decay_t<Scene>>::value>>
[[nodiscard]] auto operator|(Scene&& scene, Pass&& pass)
{
    return std::forward<Pass>(pass)(std::forward<Scene>(scene));
}

}  // namespace rubiks::graphics
