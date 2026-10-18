#include <GL/glew.h>
#include "GCodeViewer.hpp"
#include "slic3r/GUI/GCodeRenderer/AdvancedRenderer.hpp"
#include "slic3r/GUI/GCodeRenderer/LegacyRenderer.hpp"
#include "slic3r/GUI/OpenGLManager.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "libslic3r/BoundingBox.hpp"
#include "Camera.hpp"
#include <igl/unproject.h>
#include <cmath>
namespace Slic3r {
    namespace GUI {
        namespace gcode
        {
            bool GCodeViewer::pick_extrusion(const Vec2d& pixel, const Camera& camera, Vec3d& point)
            {
                const auto& renderer = get_renderer();
                const auto& viewport = camera.get_viewport();
                if (!renderer || !renderer->has_data() || !OpenGLManager::are_framebuffers_supported() ||
                    !pixel.allFinite() || pixel.x() < 0 || pixel.y() < 0 ||
                    pixel.x() >= viewport[2] || pixel.y() >= viewport[3])
                    return false;

                const int x = int(pixel.x()), y = viewport[3] - 1 - int(pixel.y());
                auto& manager = *wxGetApp().get_opengl_manager();
                // Reuse the renderer, not a second approximation of its segments.
                // A translated viewport renders just the requested pixel into a
                // single-sample 1x1 target, independent of display MSAA/FXAA.
                GLint old_viewport[4], old_fbo, old_depth_func, old_active_texture;
                GLboolean old_depth_mask, old_color_mask[4];
                GLdouble old_clear_depth;
                glGetIntegerv(GL_VIEWPORT, old_viewport);
                glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);
                glGetIntegerv(GL_DEPTH_FUNC, &old_depth_func);
                glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active_texture);
                glGetBooleanv(GL_DEPTH_WRITEMASK, &old_depth_mask);
                glGetBooleanv(GL_COLOR_WRITEMASK, old_color_mask);
                glGetDoublev(GL_DEPTH_CLEAR_VALUE, &old_clear_depth);
                const bool depth_enabled = glIsEnabled(GL_DEPTH_TEST);
                const bool blend_enabled = glIsEnabled(GL_BLEND);
                const bool scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
                const bool stencil_enabled = glIsEnabled(GL_STENCIL_TEST);
                const auto shader = manager.get_current_shader();
                float depth = 1.0f;
                {
                    FrameBuffer frame({1, 1, 0});
                    frame.bind();
                    GLint pick_fbo = 0;
                    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &pick_fbo);
                    // A failed FBO allocation must never clear the visible framebuffer.
                    const bool complete = pick_fbo != 0 &&
                        (OpenGLManager::get_framebuffers_type() == OpenGLManager::EFramebufferType::Ext ?
                         glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT :
                         glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
                    if (complete) {
                        manager.bind_vao();
                        glViewport(-x, -y, viewport[2], viewport[3]);
                        glDisable(GL_SCISSOR_TEST);
                        glDisable(GL_BLEND);
                        glDisable(GL_STENCIL_TEST);
                        glEnable(GL_DEPTH_TEST);
                        glDepthFunc(GL_LESS);
                        glDepthMask(GL_TRUE);
                        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                        glClearDepth(1.0);
                        glClear(GL_DEPTH_BUFFER_BIT);
                        renderer->render_extrusions_for_picking();
                        glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
                    }
                }
                if (OpenGLManager::get_framebuffers_type() == OpenGLManager::EFramebufferType::Ext)
                    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, old_fbo);
                else
                    glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);
                glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);
                glDepthFunc(old_depth_func);
                glDepthMask(old_depth_mask);
                glColorMask(old_color_mask[0], old_color_mask[1], old_color_mask[2], old_color_mask[3]);
                glClearDepth(old_clear_depth);
                (depth_enabled ? glEnable : glDisable)(GL_DEPTH_TEST);
                (blend_enabled ? glEnable : glDisable)(GL_BLEND);
                (scissor_enabled ? glEnable : glDisable)(GL_SCISSOR_TEST);
                (stencil_enabled ? glEnable : glDisable)(GL_STENCIL_TEST);
                glActiveTexture(old_active_texture);
                if (shader) manager.bind_shader(shader);
                else manager.unbind_shader();

                if (!std::isfinite(depth) || depth < 0.0f || depth >= 1.0f)
                    return false;
                igl::unproject(Vec3d(viewport[0] + x + 0.5, viewport[1] + y + 0.5, depth),
                    camera.get_view_matrix().matrix(), camera.get_projection_matrix().matrix(),
                    Vec4i(viewport.data()), point);
                return point.allFinite();
            }

            GCodeViewer::GCodeViewer()
            {
            }
            GCodeViewer::~GCodeViewer()
            {
            }
            float GCodeViewer::get_legend_height() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_legend_height();
                }
                return 0.0f;
            }
            IMSlider* GCodeViewer::get_moves_slider()
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_moves_slider();
                }
                return nullptr;
            }
            IMSlider* GCodeViewer::get_layers_slider()
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_layers_slider();
                }
                return nullptr;
            }
            EViewType GCodeViewer::get_view_type() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_view_type();
                }
                return EViewType::Count;
            }
            void GCodeViewer::on_change_color_mode(bool is_dark)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->on_change_color_mode(is_dark);
                }
            }
            void GCodeViewer::init(ConfigOptionMode mode, Slic3r::PresetBundle* preset_bundle)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->init(mode, preset_bundle);
                }
            }
            void GCodeViewer::reset()
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->reset();
                }
            }
            void GCodeViewer::update_sequential_view_current(unsigned int first, unsigned int last)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->update_sequential_view_current(first, last);
                }
            }
            void GCodeViewer::enable_legend(bool enable)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->enable_legend(enable);
                }
            }
            void GCodeViewer::render_calibration_thumbnail(ThumbnailData& thumbnail_data, unsigned int w, unsigned int h, const ThumbnailsParams& thumbnail_params, PartPlateList& partplate_list, OpenGLManager& opengl_manager)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->render_calibration_thumbnail(thumbnail_data, w, h, thumbnail_params, partplate_list, opengl_manager);
                }
            }
            bool GCodeViewer::is_legend_enabled()
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->is_legend_enabled();
                }
                return false;
            }
            std::vector<double> GCodeViewer::get_layers_zs() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_layers_zs();
                }
                static std::vector<double> s_empty_list{};
                return s_empty_list;
            }
            size_t GCodeViewer::get_extruders_count() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_extruders_count();
                }
                return 0;
            }
            std::vector<CustomGCode::Item>& GCodeViewer::get_custom_gcode_per_print_z() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_custom_gcode_per_print_z();
                }
                static std::vector<CustomGCode::Item> s_empty_list{};
                return s_empty_list;
            }
            const BoundingBoxf3& GCodeViewer::get_paths_bounding_box() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_paths_bounding_box();
                }
                static BoundingBoxf3 s_empty;
                return s_empty;
            }
            const BoundingBoxf3& GCodeViewer::get_max_bounding_box() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_max_bounding_box();
                }
                static BoundingBoxf3 s_empty;
                return s_empty;
            }
            const BoundingBoxf3& GCodeViewer::get_shell_bounding_box() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_shell_bounding_box();
                }
                static BoundingBoxf3 s_empty;
                return s_empty;
            }
            int GCodeViewer::get_options_visibility_flags() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_options_visibility_flags();
                }
                return 0;
            }
            void GCodeViewer::set_options_visibility_from_flags(unsigned int flags)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->set_options_visibility_from_flags(flags);
                }
            }
            unsigned int GCodeViewer::get_toolpath_role_visibility_flags() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_extrusion_role_visibility_flags();
                }
                return 0;
            }
            void GCodeViewer::update_shells_color_by_extruder(const DynamicPrintConfig* config)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->update_shells_color_by_extruder(config);
                }
            }
            void GCodeViewer::reset_shell()
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->reset_shell();
                }
            }
            void GCodeViewer::load_shells(const Print& print, bool initialized, bool force_previewing)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->load_shells(print, initialized, force_previewing);
                }
            }
            void GCodeViewer::set_shells_on_preview(bool is_previewing)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->set_shells_on_preview(is_previewing);
                }
            }
            void GCodeViewer::load(const GCodeProcessorResult& gcode_result, const Print& print, const BuildVolume& build_volume, const std::vector<BoundingBoxf3>& exclude_bounding_box, bool initialized, ConfigOptionMode mode, bool only_gcode)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->load(gcode_result, print, build_volume, exclude_bounding_box, initialized, mode, only_gcode);
                }
            }
            void GCodeViewer::refresh(const GCodeProcessorResult& gcode_result, const std::vector<std::string>& str_tool_colors)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->refresh(gcode_result, str_tool_colors);
                }
            }
            void GCodeViewer::refresh_render_paths()
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->refresh_render_paths();
                }
            }
            void GCodeViewer::toggle_gcode_window_visibility()
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->toggle_gcode_window_visibility();
                }
            }
            bool GCodeViewer::can_export_toolpaths() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->can_export_toolpaths();
                }
                return false;
            }
            void GCodeViewer::export_toolpaths_to_obj(const char* filename) const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->export_toolpaths_to_obj(filename);
                }
            }
            bool GCodeViewer::has_data() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->has_data();
                }
                return false;
            }
            void GCodeViewer::set_layers_z_range(const std::array<unsigned int, 2>& layers_z_range)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->set_layers_z_range(layers_z_range);
                }
            }
            void GCodeViewer::update_marker_curr_move()
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->update_marker_curr_move();
                }
            }
            void GCodeViewer::render_all_plates_stats(const std::vector<const GCodeProcessorResult*>& gcode_result_list, bool show) const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->render_all_plates_stats(gcode_result_list, show);
                }
            }
            bool GCodeViewer::is_contained_in_bed() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->is_contained_in_bed();
                }
                return false;
            }
            const std::array<unsigned int, 2>& GCodeViewer::get_layers_z_range() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_layers_z_range();
                }
                static std::array<unsigned int, 2> s_empty_array{};
                return s_empty_array;
            }
            const float GCodeViewer::get_max_print_height() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_max_print_height();
                }
                return 0.0f;
            }
            const GCodeCheckResult& GCodeViewer::get_gcode_check_result() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_gcode_check_result();
                }
                static GCodeCheckResult temp;
                return temp;
            }
            const FilamentPrintableResult& GCodeViewer::get_filament_printable_result() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_filament_printable_result();
                }
                static FilamentPrintableResult temp;
                return temp;
            }
            const ConflictResultOpt& GCodeViewer::get_conflict_result() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_conflict_result();
                }
                static ConflictResultOpt temp;
                return temp;
            }
            void GCodeViewer::render(int canvas_width, int canvas_height, int right_margin)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->render(canvas_width, canvas_height, right_margin);
                }
            }
            const Shells& GCodeViewer::get_shells() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_shells();
                }
                static Shells s_empty;
                return s_empty;
            }
            void GCodeViewer::set_scale(float scale)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->set_scale(scale);
                }
            }
            // helio
            bool GCodeViewer::is_show_horizontal_slider() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->is_show_horizontal_slider();
                }
                return false;
            }

            void GCodeViewer::set_show_horizontal_slider(bool flag)
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->set_show_horizontal_slider(flag);
                }
            }

            bool GCodeViewer::is_helio_option() const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->is_helio_option();
                }
                return false;
            }

            bool GCodeViewer::curr_plate_has_ok_helio_slice(int plate_idx) const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->curr_plate_has_ok_helio_slice(plate_idx);
                }
                return false;
            }

            void GCodeViewer::reset_curr_plate_thermal_options()
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->reset_curr_plate_thermal_options();
                }
            }

            void GCodeViewer::record_record_gcodeviewer_option_item()
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    p_renderer->record_gcodeviewer_option_item();
                }
            }
            bool GCodeViewer::get_min_max_value_of_option(int index, float& _min, float& _max) const
            {
                const auto& p_renderer = get_renderer();
                if (p_renderer) {
                    return p_renderer->get_min_max_value_of_option(index, _min, _max);
                }
                return false;
            }
            // end helio

            const std::shared_ptr<gcode::BaseRenderer>& GCodeViewer::get_renderer() const
            {
                const auto& p_ogl_manager = wxGetApp().get_opengl_manager();
                const bool b_advanced_gcode_viewer_enabled = p_ogl_manager->is_advanced_gcode_viewer_enabled();
                const bool b_dirty = m_b_advanced_gcode_viewer_enabled != b_advanced_gcode_viewer_enabled;
                if (!m_p_renderer || b_dirty) {
                    if (p_ogl_manager) {
                        if (p_ogl_manager->init_gl()) {
                            const auto& gl_version = p_ogl_manager->get_gl_info().get_formated_gl_version();
                            if (b_advanced_gcode_viewer_enabled && gl_version >= 31) {
                                m_p_renderer = std::make_shared<gcode::AdvancedRenderer>();
                            }
                            else {
                                m_p_renderer = std::make_shared<gcode::LegacyRenderer>();
                            }
                            if (m_p_renderer) {
                                m_p_renderer->reset();
                            }
                        }
                    }
                    m_b_advanced_gcode_viewer_enabled = b_advanced_gcode_viewer_enabled;
                }
                return m_p_renderer;
            }
        } // namespace gcode
    } // namespace GUI
} // namespace Slic3r
