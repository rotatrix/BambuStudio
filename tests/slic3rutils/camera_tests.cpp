#include <catch2/catch.hpp>
#include "slic3r/GUI/OpenAxisCamera.hpp"

using namespace Slic3r;
using Slic3r::GUI::Camera;
using Slic3r::GUI::apply_openaxis_camera;

namespace {
const BoundingBoxf3 scene(Vec3d(-10, -10, 0), Vec3d(10, 10, 20));
void prepare(Camera& camera, Camera::EType type)
{
    camera.set_scene_box(scene);
    // Exercise native projection math without requiring a GL context merely to
    // call glViewport. The viewport storage belongs to this mutable test camera.
    const_cast<std::array<int, 4>&>(camera.get_viewport()) = {0, 0, 1200, 800};
    camera.set_type(type);
}
double scale(const Camera& camera)
{
    const double yy = camera.get_projection_matrix()(1, 1);
    return camera.get_type() == Camera::EType::Perspective ? 2 * std::atan(1 / yy) : 2 / yy;
}
}

TEST_CASE("OpenAxis keeps native projection and uses axial depth for an off-axis pivot", "[Camera]")
{
    Camera camera;
    const auto type = GENERATE(Camera::EType::Perspective, Camera::EType::Ortho);
    prepare(camera, type);
    const Vec3d position(0, 0, 410), pivot(30, 40, 10), forward(0, 0, -1);
    apply_openaxis_camera(camera, position, forward, Vec3d::UnitY(), pivot,
                          type == Camera::EType::Perspective ? 0.2 : 0,
                          type == Camera::EType::Ortho ? 20 : 0);
    REQUIRE(camera.get_type() == type);
    REQUIRE(camera.get_distance() == Approx(400));
    REQUIRE(camera.get_target().isApprox(Vec3d(0, 0, 10)));
    camera.apply_projection(scene);
    const auto view = camera.get_view_matrix();
    const double original_scale = scale(camera);
    REQUIRE(original_scale == Approx(type == Camera::EType::Perspective ? 0.2 : 20));
    for (int redraw = 0; redraw < 3; ++redraw) {
        camera.apply_projection(scene);
        REQUIRE(camera.get_view_matrix().matrix().isApprox(view.matrix()));
        REQUIRE(scale(camera) == Approx(original_scale));
    }
    // Deliberately supply the opposite projection. The actual adapter operation
    // must retain both the native type and its scale as pivot depth changes.
    apply_openaxis_camera(camera, Vec3d(0, 0, 210), forward, Vec3d::UnitY(), pivot,
                          type == Camera::EType::Perspective ? 0 : 1.2,
                          type == Camera::EType::Ortho ? 0 : 100);
    camera.apply_projection(scene);
    REQUIRE(camera.get_type() == type);
    REQUIRE(camera.get_distance() == Approx(200));
    REQUIRE(scale(camera) == Approx(original_scale));
}

TEST_CASE("OpenAxis close poses retain native zoom and frustum limits", "[Camera]")
{
    Camera camera;
    const auto type = GENERATE(Camera::EType::Perspective, Camera::EType::Ortho);
    prepare(camera, type);
    for (double depth : {30.0, 2.0, 0.1}) {
        INFO("depth " << depth << " projection " << camera.get_type_as_string());
        const Vec3d position(0, 0, 10 + depth), pivot(3, 4, 10), forward(0, 0, -1);
        apply_openaxis_camera(camera, position, forward, Vec3d::UnitY(), pivot,
                              type == Camera::EType::Perspective ? 0.7 : 0,
                              type == Camera::EType::Ortho ? 0.5 : 0);
        REQUIRE(camera.get_distance() == Approx(depth));
        REQUIRE(camera.get_zoom() <= camera.max_zoom());
        REQUIRE(camera.get_target().isApprox(Vec3d(0, 0, 10)));
        camera.apply_projection(scene);
        REQUIRE(camera.get_near_z() >= Camera::FrustrumMinNearZ);
        REQUIRE_FALSE(camera.get_position().isApprox(position)); // Native limit remains active.
        const auto constrained_view = camera.get_view_matrix();
        const double constrained_scale = scale(camera);
        for (int redraw = 0; redraw < 3; ++redraw) {
            camera.apply_projection(scene);
            REQUIRE(camera.get_view_matrix().matrix().isApprox(constrained_view.matrix()));
            REQUIRE(scale(camera) == Approx(constrained_scale));
            REQUIRE(camera.get_projection_matrix().matrix().allFinite());
            REQUIRE(camera.get_type() == type);
        }
    }
}

TEST_CASE("OpenAxis camera remains compatible with ISO top front and zoom to fit", "[Camera]")
{
    Camera camera;
    const auto type = GENERATE(Camera::EType::Perspective, Camera::EType::Ortho);
    prepare(camera, type);
    apply_openaxis_camera(camera, Vec3d(0, 0, 410), Vec3d(0, 0, -1), Vec3d::UnitY(),
                          Vec3d(3, 4, 10),
                          type == Camera::EType::Perspective ? 0.7 : 0,
                          type == Camera::EType::Ortho ? 20 : 0);
    for (auto preset : {Camera::ViewAngleType::Iso, Camera::ViewAngleType::Top, Camera::ViewAngleType::Front}) {
        camera.select_view(preset);
        const auto view = camera.get_view_matrix();
        const Vec3d target = camera.get_target();
        const double distance = camera.get_distance();
        for (int redraw = 0; redraw < 3; ++redraw) {
            camera.apply_projection(scene);
            REQUIRE(camera.get_view_matrix().matrix().isApprox(view.matrix()));
            REQUIRE(camera.get_target().isApprox(target));
            REQUIRE(camera.get_distance() == Approx(distance));
            REQUIRE(camera.get_type() == type);
        }
        if (preset == Camera::ViewAngleType::Top)
            REQUIRE(camera.get_dir_forward().isApprox(-Vec3d::UnitZ()));
        if (preset == Camera::ViewAngleType::Front)
            REQUIRE(camera.get_dir_forward().isApprox(Vec3d::UnitY()));
    }
    const double native_distance = camera.get_distance();
    camera.zoom_to_box(scene);
    camera.apply_projection(scene);
    REQUIRE(camera.get_near_z() >= Camera::FrustrumMinNearZ);
    REQUIRE(camera.get_distance() == Approx(native_distance));
    REQUIRE(camera.get_target().isApprox(scene.center()));
}
