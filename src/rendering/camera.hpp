/**
 * @file camera.hpp
 * @brief Камера для 3D визуализации
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace incline::rendering {

/**
 * @brief Камера для 3D аксонометрии
 *
 * Поддерживает вращение, панорамирование и масштабирование.
 * Координатная система: X=Север, Y=Восток, Z=Вниз
 */
class Camera {
public:
    Camera() = default;

    /**
     * @brief Установить углы вращения (градусы)
     * @param x Наклон (вокруг оси X)
     * @param y Поворот (вокруг оси Y)
     * @param z Вращение (вокруг оси Z)
     */
    void setRotation(float x, float y, float z) noexcept;

    /**
     * @brief Получить углы вращения
     */
    [[nodiscard]] glm::vec3 getRotation() const noexcept { return rotation_; }

    /**
     * @brief Установить смещение камеры (панорамирование)
     */
    void setPan(float x, float y) noexcept;

    /**
     * @brief Получить смещение
     */
    [[nodiscard]] glm::vec2 getPan() const noexcept { return pan_; }

    /**
     * @brief Установить масштаб
     */
    void setZoom(float zoom) noexcept;

    /**
     * @brief Получить масштаб
     */
    [[nodiscard]] float getZoom() const noexcept { return zoom_; }

    /**
     * @brief Вращение относительно текущего положения
     * @param dx Изменение угла X (градусы)
     * @param dy Изменение угла Y (градусы)
     */
    void rotate(float dx, float dy) noexcept;

    /**
     * @brief Панорамирование относительно текущего положения
     */
    void pan(float dx, float dy) noexcept;

    /**
     * @brief Масштабирование (умножение на коэффициент)
     */
    void zoom(float factor) noexcept;

    /**
     * @brief Сброс в начальное положение
     */
    void reset() noexcept;

    /**
     * @brief Установить центр сцены (вокруг чего вращаемся)
     */
    void setSceneCenter(const glm::vec3& center) noexcept;

    /**
     * @brief Установить размер области просмотра
     */
    void setViewportSize(int width, int height) noexcept;

    /**
     * @brief Получить матрицу вида (View)
     */
    [[nodiscard]] glm::mat4 getViewMatrix() const noexcept;

    /**
     * @brief Получить матрицу проекции (ортографическая)
     */
    [[nodiscard]] glm::mat4 getProjectionMatrix() const noexcept;

    /**
     * @brief Получить объединённую матрицу MVP
     */
    [[nodiscard]] glm::mat4 getMVPMatrix() const noexcept;

    /**
     * @brief Преобразование экранных координат в мировые
     */
    [[nodiscard]] glm::vec3 screenToWorld(float x, float y, float depth = 0.0f) const noexcept;

    /**
     * @brief Преобразование мировых координат в экранные
     */
    [[nodiscard]] glm::vec2 worldToScreen(const glm::vec3& world) const noexcept;

    // Начальные значения по умолчанию
    static constexpr float DEFAULT_ROTATION_X = 30.0f;
    static constexpr float DEFAULT_ROTATION_Y = 45.0f;
    static constexpr float DEFAULT_ROTATION_Z = 0.0f;
    static constexpr float DEFAULT_ZOOM = 1.0f;
    static constexpr float MIN_ZOOM = 0.1f;
    static constexpr float MAX_ZOOM = 100.0f;

private:
    void updateMatrices() noexcept;

    glm::vec3 rotation_{DEFAULT_ROTATION_X, DEFAULT_ROTATION_Y, DEFAULT_ROTATION_Z};
    glm::vec2 pan_{0.0f, 0.0f};
    float zoom_{DEFAULT_ZOOM};

    glm::vec3 scene_center_{0.0f, 0.0f, 0.0f};
    int viewport_width_{800};
    int viewport_height_{600};

    // Кэшированные матрицы
    mutable glm::mat4 view_matrix_{1.0f};
    mutable glm::mat4 projection_matrix_{1.0f};
    mutable bool matrices_dirty_{true};
};

} // namespace incline::rendering
