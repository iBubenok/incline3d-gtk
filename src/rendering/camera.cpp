/**
 * @file camera.cpp
 * @brief Реализация камеры для 3D визуализации
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace incline::rendering {

void Camera::setRotation(float x, float y, float z) noexcept {
    rotation_.x = std::clamp(x, -89.0f, 89.0f);
    rotation_.y = y;
    rotation_.z = z;
    matrices_dirty_ = true;
}

void Camera::setPan(float x, float y) noexcept {
    pan_.x = x;
    pan_.y = y;
    matrices_dirty_ = true;
}

void Camera::setZoom(float zoom) noexcept {
    zoom_ = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
    matrices_dirty_ = true;
}

void Camera::rotate(float dx, float dy) noexcept {
    rotation_.x = std::clamp(rotation_.x + dy, -89.0f, 89.0f);
    rotation_.y = std::fmod(rotation_.y + dx, 360.0f);
    if (rotation_.y < 0.0f) rotation_.y += 360.0f;
    matrices_dirty_ = true;
}

void Camera::pan(float dx, float dy) noexcept {
    // Учитываем масштаб при панорамировании
    pan_.x += dx / zoom_;
    pan_.y += dy / zoom_;
    matrices_dirty_ = true;
}

void Camera::zoom(float factor) noexcept {
    zoom_ = std::clamp(zoom_ * factor, MIN_ZOOM, MAX_ZOOM);
    matrices_dirty_ = true;
}

void Camera::reset() noexcept {
    rotation_ = glm::vec3(DEFAULT_ROTATION_X, DEFAULT_ROTATION_Y, DEFAULT_ROTATION_Z);
    pan_ = glm::vec2(0.0f, 0.0f);
    zoom_ = DEFAULT_ZOOM;
    matrices_dirty_ = true;
}

void Camera::setSceneCenter(const glm::vec3& center) noexcept {
    scene_center_ = center;
    matrices_dirty_ = true;
}

void Camera::setViewportSize(int width, int height) noexcept {
    viewport_width_ = std::max(1, width);
    viewport_height_ = std::max(1, height);
    matrices_dirty_ = true;
}

void Camera::updateMatrices() noexcept {
    if (!matrices_dirty_) return;

    // Преобразование углов в радианы
    float rx = glm::radians(rotation_.x);
    float ry = glm::radians(rotation_.y);
    float rz = glm::radians(rotation_.z);

    // Матрица вида
    view_matrix_ = glm::mat4(1.0f);

    // Смещение для панорамирования
    view_matrix_ = glm::translate(view_matrix_, glm::vec3(pan_.x, pan_.y, 0.0f));

    // Вращение
    view_matrix_ = glm::rotate(view_matrix_, rx, glm::vec3(1.0f, 0.0f, 0.0f));
    view_matrix_ = glm::rotate(view_matrix_, ry, glm::vec3(0.0f, 1.0f, 0.0f));
    view_matrix_ = glm::rotate(view_matrix_, rz, glm::vec3(0.0f, 0.0f, 1.0f));

    // Смещение к центру сцены
    view_matrix_ = glm::translate(view_matrix_, -scene_center_);

    // Ортографическая проекция
    float aspect = static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_);
    float size = 1000.0f / zoom_;  // Размер видимой области в метрах

    float half_width = size * aspect * 0.5f;
    float half_height = size * 0.5f;

    projection_matrix_ = glm::ortho(
        -half_width, half_width,
        -half_height, half_height,
        -10000.0f, 10000.0f  // Near/Far для ортографии
    );

    matrices_dirty_ = false;
}

glm::mat4 Camera::getViewMatrix() const noexcept {
    const_cast<Camera*>(this)->updateMatrices();
    return view_matrix_;
}

glm::mat4 Camera::getProjectionMatrix() const noexcept {
    const_cast<Camera*>(this)->updateMatrices();
    return projection_matrix_;
}

glm::mat4 Camera::getMVPMatrix() const noexcept {
    const_cast<Camera*>(this)->updateMatrices();
    return projection_matrix_ * view_matrix_;
}

glm::vec3 Camera::screenToWorld(float x, float y, float depth) const noexcept {
    const_cast<Camera*>(this)->updateMatrices();

    // Нормализуем координаты экрана в [-1, 1]
    float nx = (2.0f * x / viewport_width_) - 1.0f;
    float ny = 1.0f - (2.0f * y / viewport_height_);

    // Обратное преобразование
    glm::mat4 inv_mvp = glm::inverse(projection_matrix_ * view_matrix_);
    glm::vec4 world_pos = inv_mvp * glm::vec4(nx, ny, depth, 1.0f);

    return glm::vec3(world_pos) / world_pos.w;
}

glm::vec2 Camera::worldToScreen(const glm::vec3& world) const noexcept {
    const_cast<Camera*>(this)->updateMatrices();

    glm::vec4 clip = projection_matrix_ * view_matrix_ * glm::vec4(world, 1.0f);
    glm::vec3 ndc = glm::vec3(clip) / clip.w;

    float sx = (ndc.x + 1.0f) * 0.5f * viewport_width_;
    float sy = (1.0f - ndc.y) * 0.5f * viewport_height_;

    return glm::vec2(sx, sy);
}

} // namespace incline::rendering
