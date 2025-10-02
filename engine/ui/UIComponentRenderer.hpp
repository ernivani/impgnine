#pragma once

#include "UIComponents.hpp"
#include "UIRenderer.hpp"
#include <vulkan/vulkan.h>

namespace impgine {

    class UIComponentRenderer {
    public:
        static void renderButton(UIRenderer& renderer, UIButton& button, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts);
        static void renderInputField(UIRenderer& renderer, UIInputField& input, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts);
        static void renderLabel(UIRenderer& renderer, UILabel& label, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts);
        static void renderCheckbox(UIRenderer& renderer, UICheckbox& checkbox, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts);
        static void renderSlider(UIRenderer& renderer, UISlider& slider, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts);

        static void renderComponent(UIRenderer& renderer, UIComponent& component, const glm::vec2& windowContentPos, std::vector<UIVertex>& verts);
    };

} // namespace impgine
