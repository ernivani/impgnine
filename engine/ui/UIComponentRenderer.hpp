#pragma once

#include "UIComponents.hpp"
#include "UIRenderer.hpp"
#include <vulkan/vulkan.h>

namespace impgine {

    class UIComponentRenderer {
    public:
        static void renderButton(UIRenderer& renderer, UIButton& button, const glm::vec2& windowContentPos);
        static void renderInputField(UIRenderer& renderer, UIInputField& input, const glm::vec2& windowContentPos);
        static void renderLabel(UIRenderer& renderer, UILabel& label, const glm::vec2& windowContentPos);
        static void renderCheckbox(UIRenderer& renderer, UICheckbox& checkbox, const glm::vec2& windowContentPos);
        static void renderSlider(UIRenderer& renderer, UISlider& slider, const glm::vec2& windowContentPos);

        static void renderComponent(UIRenderer& renderer, UIComponent& component, const glm::vec2& windowContentPos);
    };

} // namespace impgine
