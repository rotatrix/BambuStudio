#pragma once
#include "Camera.hpp"
#include <cmath>
#include <optional>

namespace Slic3r::GUI {
// Keep this native-camera operation shared with the compiled regression tests.
inline void apply_openaxis_camera(Camera& c, const Vec3d& position, const Vec3d& forward,
                                  const Vec3d& up, const std::optional<Vec3d>& pivot,
                                  double fov, double ortho_extent)
{
    // Native orbit needs an on-axis target, at the pivot's positive camera-space
    // depth. Its Euclidean distance would change scale for an off-axis pivot.
    const double old_distance = c.get_distance();
    double distance = std::max(1e-6, old_distance);
    if (pivot) {
        const double depth = (*pivot - position).dot(forward);
        if (std::isfinite(depth) && depth > 1e-6)
            distance = depth;
    }
    // Projection is the user's native preference. On a mismatched pose retain
    // its scale within native limits; realized readback lets the SDK rebase.
    double zoom = c.get_zoom() * (c.get_type() == Camera::EType::Perspective ? old_distance / distance : 1.0);
    if (c.get_type() == Camera::EType::Perspective && fov > 0)
        zoom = c.get_viewport()[3] / (2 * distance * std::tan(fov / 2));
    else if (c.get_type() == Camera::EType::Ortho && ortho_extent > 0)
        zoom = c.get_viewport()[3] / ortho_extent;
    c.look_at(position, position + distance * forward, up);
    c.set_zoom(zoom); // Retain Bambu's native zoom limits and frustum adjustment.
}
} // namespace Slic3r::GUI
