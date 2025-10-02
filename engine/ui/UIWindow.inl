#pragma once

#include "UIWindow.hpp"
#include "UIComponents.hpp"

namespace impgine {

    template<typename T>
    std::shared_ptr<T> UIWindow::findComponent(const glm::vec2& localPoint) {
        for (auto& comp : components) {
            if (comp->isPointInComponent(localPoint)) {
                return std::dynamic_pointer_cast<T>(comp);
            }
        }
        return nullptr;
    }

} // namespace impgine
