#pragma once
#include "Camera.hpp"
#include "OpenAxisCameraMath.hpp"
#include <optional>

namespace Slic3r::GUI {
// Keep this native-camera operation shared with the compiled regression tests.
inline void apply_openaxis_camera(Camera& c, const Vec3d& position, const Vec3d& forward,
                                  const Vec3d& up, const std::optional<Vec3d>& pivot,
                                  double fov, double ortho_extent)
{
    // Native orbit needs an on-axis target, at the pivot's positive camera-space
    // depth. Its Euclidean distance would change scale for an off-axis pivot.
    const auto scale = OpenAxisCameraMath::view_scale(
        c.get_distance(), c.get_zoom(), c.get_viewport()[3],
        c.get_type() == Camera::EType::Perspective,
        pivot ? std::optional<double>((*pivot - position).dot(forward)) : std::nullopt,
        fov, ortho_extent);
    c.look_at(position, position + scale.distance * forward, up);
    c.set_zoom(scale.zoom); // Retain Bambu's native zoom limits and frustum adjustment.
}
} // namespace Slic3r::GUI
