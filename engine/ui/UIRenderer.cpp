#include "UIRenderer.hpp"
#include "UIComponentRenderer.hpp"

#include <cstring>
#include <iostream>
#include "../ECS/components.hpp"

namespace impgine {

    UIRenderer::UIRenderer(VkDevice device, VkPhysicalDevice physicalDevice, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, VkCommandPool commandPool, VkQueue graphicsQueue, SwapChain& swapChain, VkSampleCountFlagBits samples, const std::string& vertSpvPath, const std::string& fragSpvPath)
        : device(device), physicalDevice(physicalDevice), pipelineLayout(pipelineLayout), swapChain(swapChain), samples(samples), vertPath(vertSpvPath), fragPath(fragSpvPath),
          layout(swapChain.getSwapChainExtent().width, swapChain.getSwapChainExtent().height) {

        textRenderer = std::make_unique<TextRenderer>(device, physicalDevice, commandPool, graphicsQueue);

        // Create UI-specific descriptor set layout and pipeline layout
        createDescriptorSetLayout();
        createDescriptorPool();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &uiPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create UI pipeline layout");
        }

        PipelineConfigInfo cfg{};
        Pipeline::defaultPipelineConfigInfo(cfg);
        Pipeline::enableAlphaBlending(cfg);
        cfg.renderPass = renderPass;
        cfg.pipelineLayout = uiPipelineLayout;
        cfg.multisampleInfo.rasterizationSamples = samples;
        cfg.depthStencilInfo.depthTestEnable = VK_FALSE;
        cfg.depthStencilInfo.depthWriteEnable = VK_FALSE;

        // Provide UI vertex format
        cfg.bindingDescriptions = { UIVertex::getBindingDescription() };
        auto attr = UIVertex::getAttributeDescriptions();
        cfg.attributeDescriptions.assign(attr.begin(), attr.end());

        // UI shaders (path relative to project config)
        pipeline = std::make_unique<Pipeline>(device, vertPath, fragPath, cfg);

        createVertexBuffer(1024 * sizeof(UIVertex));
    }

    UIRenderer::~UIRenderer() {
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        }
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        }
        if (uiPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, uiPipelineLayout, nullptr);
        }
        if (vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, vertexBuffer, nullptr);
        }
        if (vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, vertexMemory, nullptr);
        }
    }

    void UIRenderer::createVertexBuffer(VkDeviceSize size) {
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = size;
        info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device, &info, nullptr, &vertexBuffer);

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, vertexBuffer, &req);

        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        // Host visible for simplicity
        // Find memory type  - using a basic local function (engine already has one, but not accessible here)
        // We'll choose the first HOST_VISIBLE | HOST_COHERENT type.
        VkPhysicalDeviceMemoryProperties props{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);
        uint32_t typeIndex = 0;
        for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
            if ((req.memoryTypeBits & (1u << i)) &&
                (props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                typeIndex = i; break;
            }
        }
        alloc.memoryTypeIndex = typeIndex;
        vkAllocateMemory(device, &alloc, nullptr, &vertexMemory);
        vkBindBufferMemory(device, vertexBuffer, vertexMemory, 0);
    }

    void UIRenderer::record(VkCommandBuffer cmd, const Window& window, std::vector<UIWindow>& windows, uint32_t imageIndex) {
        // Update layout dimensions if window was resized
        int ww = 0, wh = 0;
        window.getFramebufferSize(&ww, &wh);
        layout.setWindowSize(ww, wh);

        // Compute layout for docked windows
        layout.computeLayout(windows);

        // Build quads for each window with title bars and borders
        std::vector<UIVertex> verts;
        verts.reserve(windows.size() * 30);  // More vertices for detailed UI

        auto toNDC = [&](glm::vec2 px) {
            float nx = (px.x / static_cast<float>(swapChain.getSwapChainExtent().width)) * 2.0f - 1.0f;
            float ny = (px.y / static_cast<float>(swapChain.getSwapChainExtent().height)) * 2.0f - 1.0f;
            return glm::vec2(nx, -ny);
        };

        auto addQuad = [&](glm::vec2 p, glm::vec2 s, glm::vec4 col) {
            glm::vec2 p0 = toNDC(p);
            glm::vec2 p1 = toNDC({ p.x + s.x, p.y });
            glm::vec2 p2 = toNDC({ p.x + s.x, p.y + s.y });
            glm::vec2 p3 = toNDC({ p.x, p.y + s.y });

            verts.push_back({ p0, col, {0.0f, 0.0f} });
            verts.push_back({ p1, col, {0.0f, 0.0f} });
            verts.push_back({ p2, col, {0.0f, 0.0f} });
            verts.push_back({ p0, col, {0.0f, 0.0f} });
            verts.push_back({ p2, col, {0.0f, 0.0f} });
            verts.push_back({ p3, col, {0.0f, 0.0f} });
        };

        for (const auto& w : windows) {
            if (!w.isVisible) continue;

            glm::vec2 p = w.position;
            glm::vec2 s = w.size;

            // Draw border
            if (w.borderWidth > 0.0f) {
                addQuad(p, { s.x, w.borderWidth }, w.borderColor);  // Top
                addQuad({ p.x, p.y + s.y - w.borderWidth }, { s.x, w.borderWidth }, w.borderColor);  // Bottom
                addQuad(p, { w.borderWidth, s.y }, w.borderColor);  // Left
                addQuad({ p.x + s.x - w.borderWidth, p.y }, { w.borderWidth, s.y }, w.borderColor);  // Right
            }

            // Draw title bar
            glm::vec2 titleBarPos = { p.x + w.borderWidth, p.y + w.borderWidth };
            glm::vec2 titleBarSize = { s.x - 2.0f * w.borderWidth, w.titleBarHeight };
            if (w.titleBarHeight > 0.0f) {
                addQuad(titleBarPos, titleBarSize, w.titleBarColor);

                // Render title bar text
                glm::vec2 titleTextPos = { p.x + w.borderWidth + 10.0f, p.y + w.borderWidth + 8.0f };
                renderText(w.title, titleTextPos, 0.4f, {1.0f, 1.0f, 1.0f, 1.0f}, verts);
            }

            // Draw content area background
            glm::vec2 contentPos = { p.x + w.borderWidth, p.y + w.borderWidth + w.titleBarHeight };
            glm::vec2 contentSize = { s.x - 2.0f * w.borderWidth, s.y - w.titleBarHeight - 2.0f * w.borderWidth };
            addQuad(contentPos, contentSize, w.backgroundColor);

            // Render UI components in this window
            currentVertices.clear();
            for (const auto& comp : w.components) {
                UIComponentRenderer::renderComponent(*this, *comp, contentPos);
            }
            // Convert currentVertices to NDC and add to verts
            if (!currentVertices.empty()) {
                std::cout << "Rendering " << currentVertices.size() << " component vertices in window '" << w.title << "'" << std::endl;
                std::cout << "Content pos: (" << contentPos.x << ", " << contentPos.y << ")" << std::endl;
            }
            for (const auto& v : currentVertices) {
                verts.push_back({ toNDC(v.pos), v.color, v.uv });
            }

            // Render content based on window type
            if (w.type == UIWindowType::Hierarchy) {
                auto& registry = ECSRegistry::getRegistry();
                const auto& entities = registry.getEntities();

                float yOffset = contentPos.y + 10.0f;
                float lineHeight = 25.0f;

                for (const auto& entity : entities) {
                    // Get Tag component to display entity name
                    std::string entityName = "Entity " + std::to_string(entity);

                    try {
                        const auto& tag = registry.getComponent<Tag>(entity);
                        if (!tag.tag.empty()) {
                            entityName = tag.tag;
                        }
                    } catch (...) {
                        // Entity doesn't have Tag component, use default name
                    }

                    // Highlight selected entity
                    glm::vec4 textColor = (entity == selectedEntity) ? glm::vec4(0.3f, 0.6f, 1.0f, 1.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

                    glm::vec2 textPos = { contentPos.x + 10.0f, yOffset };
                    renderText(entityName, textPos, 0.4f, textColor, verts);
                    yOffset += lineHeight;
                }
            } else if (w.type == UIWindowType::Inspector) {
                // Display selected entity details
                if (selectedEntity != INVALID_ENTITY) {
                    auto& registry = ECSRegistry::getRegistry();

                    float yOffset = contentPos.y + 10.0f;
                    float lineHeight = 25.0f;

                    // Entity name
                    std::string entityName = "Entity " + std::to_string(selectedEntity);
                    try {
                        const auto& tag = registry.getComponent<Tag>(selectedEntity);
                        if (!tag.tag.empty()) {
                            entityName = tag.tag;
                        }
                    } catch (...) {}

                    glm::vec2 textPos = { contentPos.x + 10.0f, yOffset };
                    renderText(entityName, textPos, 0.5f, {1.0f, 1.0f, 1.0f, 1.0f}, verts);
                    yOffset += lineHeight * 1.5f;

                    // Transform component
                    try {
                        const auto& transform = registry.getComponent<Transform>(selectedEntity);

                        renderText("Transform:", {contentPos.x + 10.0f, yOffset}, 0.4f, {0.8f, 0.8f, 0.8f, 1.0f}, verts);
                        yOffset += lineHeight;

                        std::string posStr = "Pos: " + std::to_string((int)transform.position.x) + ", " +
                                            std::to_string((int)transform.position.y) + ", " +
                                            std::to_string((int)transform.position.z);
                        renderText(posStr, {contentPos.x + 20.0f, yOffset}, 0.35f, {0.7f, 0.7f, 0.7f, 1.0f}, verts);
                        yOffset += lineHeight;

                        std::string rotStr = "Rot: " + std::to_string((int)transform.rotation.x) + ", " +
                                            std::to_string((int)transform.rotation.y) + ", " +
                                            std::to_string((int)transform.rotation.z);
                        renderText(rotStr, {contentPos.x + 20.0f, yOffset}, 0.35f, {0.7f, 0.7f, 0.7f, 1.0f}, verts);
                        yOffset += lineHeight;

                        std::string scaleStr = "Scale: " + std::to_string(transform.scale.x).substr(0, 4) + ", " +
                                              std::to_string(transform.scale.y).substr(0, 4) + ", " +
                                              std::to_string(transform.scale.z).substr(0, 4);
                        renderText(scaleStr, {contentPos.x + 20.0f, yOffset}, 0.35f, {0.7f, 0.7f, 0.7f, 1.0f}, verts);
                        yOffset += lineHeight * 1.5f;
                    } catch (...) {}

                    // MeshRenderer3D component
                    try {
                        const auto& meshRenderer = registry.getComponent<MeshRenderer3D>(selectedEntity);

                        renderText("MeshRenderer3D:", {contentPos.x + 10.0f, yOffset}, 0.4f, {0.8f, 0.8f, 0.8f, 1.0f}, verts);
                        yOffset += lineHeight;

                        if (meshRenderer.mesh) {
                            std::string meshPath = meshRenderer.mesh->modelPath;
                            // Show only filename
                            size_t lastSlash = meshPath.find_last_of("/\\");
                            if (lastSlash != std::string::npos) {
                                meshPath = meshPath.substr(lastSlash + 1);
                            }
                            renderText("Mesh: " + meshPath, {contentPos.x + 20.0f, yOffset}, 0.35f, {0.7f, 0.7f, 0.7f, 1.0f}, verts);
                        }
                        yOffset += lineHeight;
                    } catch (...) {}
                }
            }
        }

        if (verts.empty()) return;

        // Upload to vertex buffer
        void* data = nullptr;
        vkMapMemory(device, vertexMemory, 0, verts.size() * sizeof(UIVertex), 0, &data);
        std::memcpy(data, verts.data(), verts.size() * sizeof(UIVertex));
        vkUnmapMemory(device, vertexMemory);

        pipeline->bind(cmd);

        // Bind descriptor set for font texture - must always bind to match pipeline layout
        if (!descriptorSets.empty() && imageIndex < descriptorSets.size()) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, nullptr);
        } else {
            std::cerr << "Warning: UI descriptor sets not available! descriptorSets.size()=" << descriptorSets.size() << ", imageIndex=" << imageIndex << std::endl;
        }

        // Set viewport/scissor to full framebuffer
        VkViewport vp{}; vp.x = 0; vp.y = 0; vp.width = static_cast<float>(swapChain.getSwapChainExtent().width); vp.height = static_cast<float>(swapChain.getSwapChainExtent().height); vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);
        VkRect2D sc{}; sc.offset = {0,0}; sc.extent = swapChain.getSwapChainExtent();
        vkCmdSetScissor(cmd, 0, 1, &sc);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
        vkCmdDraw(cmd, static_cast<uint32_t>(verts.size()), 1, 0, 0);
    }

    void UIRenderer::renderText(const std::string& text, glm::vec2 position, float scale, glm::vec4 color, std::vector<UIVertex>& verts) {
        if (!textRenderer) return;

        auto toNDC = [&](glm::vec2 px) {
            float nx = (px.x / static_cast<float>(swapChain.getSwapChainExtent().width)) * 2.0f - 1.0f;
            float ny = (px.y / static_cast<float>(swapChain.getSwapChainExtent().height)) * 2.0f - 1.0f;
            return glm::vec2(nx, -ny);
        };

        float xpos = position.x;
        float ypos = position.y;

        for (char c : text) {
            const Character* ch = textRenderer->getCharacter(c);
            if (!ch) continue;

            float w = ch->size.x * scale;
            float h = ch->size.y * scale;
            float x = xpos + ch->bearing.x * scale;
            float y = ypos - (ch->size.y - ch->bearing.y) * scale;

            glm::vec2 p0 = toNDC({ x, y });
            glm::vec2 p1 = toNDC({ x + w, y });
            glm::vec2 p2 = toNDC({ x + w, y + h });
            glm::vec2 p3 = toNDC({ x, y + h });

            verts.push_back({ p0, color, { ch->texCoordMin.x, ch->texCoordMax.y } });
            verts.push_back({ p1, color, { ch->texCoordMax.x, ch->texCoordMax.y } });
            verts.push_back({ p2, color, { ch->texCoordMax.x, ch->texCoordMin.y } });
            verts.push_back({ p0, color, { ch->texCoordMin.x, ch->texCoordMax.y } });
            verts.push_back({ p2, color, { ch->texCoordMax.x, ch->texCoordMin.y } });
            verts.push_back({ p3, color, { ch->texCoordMin.x, ch->texCoordMin.y } });

            xpos += (ch->advance >> 6) * scale;
        }
    }

    void UIRenderer::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 0;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &samplerLayoutBinding;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create UI descriptor set layout");
        }
    }

    void UIRenderer::createDescriptorPool() {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = static_cast<uint32_t>(swapChain.imageCount());

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(swapChain.imageCount());

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create UI descriptor pool");
        }
    }

    void UIRenderer::createDescriptorSets() {
        if (!textRenderer) {
            std::cerr << "TextRenderer is null in createDescriptorSets!" << std::endl;
            return;
        }

        if (!textRenderer->getTextureView() || !textRenderer->getSampler()) {
            std::cerr << "Font texture not loaded yet!" << std::endl;
            return;
        }

        std::vector<VkDescriptorSetLayout> layouts(swapChain.imageCount(), descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChain.imageCount());
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets.resize(swapChain.imageCount());
        VkResult result = vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data());
        if (result != VK_SUCCESS) {
            std::cerr << "Failed to allocate UI descriptor sets! VkResult: " << result << std::endl;
            throw std::runtime_error("Failed to allocate UI descriptor sets");
        }

        std::cout << "Created " << descriptorSets.size() << " UI descriptor sets" << std::endl;

        for (size_t i = 0; i < swapChain.imageCount(); i++) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = textRenderer->getTextureView();
            imageInfo.sampler = textRenderer->getSampler();

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        }
    }

    void UIRenderer::handleMouseClick(double xpos, double ypos, std::vector<UIWindow>& windows) {
        auto& registry = ECSRegistry::getRegistry();
        const auto& entities = registry.getEntities();

        for (const auto& w : windows) {
            if (!w.isVisible || w.type != UIWindowType::Hierarchy) continue;

            // Check if click is in hierarchy window content area
            glm::vec2 contentPos = { w.position.x + w.borderWidth, w.position.y + w.borderWidth + w.titleBarHeight };
            glm::vec2 contentSize = { w.size.x - 2.0f * w.borderWidth, w.size.y - w.titleBarHeight - 2.0f * w.borderWidth };

            if (xpos >= contentPos.x && xpos <= contentPos.x + contentSize.x &&
                ypos >= contentPos.y && ypos <= contentPos.y + contentSize.y) {

                // Calculate which entity was clicked
                float yOffset = contentPos.y + 10.0f;
                float lineHeight = 25.0f;
                int index = 0;

                for (const auto& entity : entities) {
                    float itemTop = yOffset;
                    float itemBottom = yOffset + lineHeight;

                    if (ypos >= itemTop && ypos < itemBottom) {
                        selectedEntity = entity;
                        std::cout << "Selected entity: " << entity << std::endl;
                        return;
                    }

                    yOffset += lineHeight;
                    index++;
                }
            }
        }
    }

    void UIRenderer::drawRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color) {
        // Create 6 vertices for 2 triangles (a quad)
        // Use UV (0,0) for all vertices to match existing UI rendering
        UIVertex v0, v1, v2, v3;

        v0.pos = glm::vec2(position.x, position.y);
        v0.uv = glm::vec2(0.0f, 0.0f);
        v0.color = color;

        v1.pos = glm::vec2(position.x + size.x, position.y);
        v1.uv = glm::vec2(0.0f, 0.0f);
        v1.color = color;

        v2.pos = glm::vec2(position.x + size.x, position.y + size.y);
        v2.uv = glm::vec2(0.0f, 0.0f);
        v2.color = color;

        v3.pos = glm::vec2(position.x, position.y + size.y);
        v3.uv = glm::vec2(0.0f, 0.0f);
        v3.color = color;

        // First triangle
        currentVertices.push_back(v0);
        currentVertices.push_back(v1);
        currentVertices.push_back(v2);

        // Second triangle
        currentVertices.push_back(v0);
        currentVertices.push_back(v2);
        currentVertices.push_back(v3);
    }

    void UIRenderer::drawText(const std::string& text, const glm::vec2& position, const glm::vec4& color) {
        renderText(text, position, 0.4f, color, currentVertices);
    }

} // namespace impgine


