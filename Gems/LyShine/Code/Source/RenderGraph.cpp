/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "LyShine_precompiled.h"

#include "RenderGraph.h"
#include "UiRenderer.h"

#include <Atom/RPI.Public/Image/ImageSystemInterface.h>

#ifndef _RELEASE
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzFramework/IO/LocalFileIO.h>
#endif

namespace LyShine
{
    static const char* s_maskIncrProfileMarker = "UI_MASK_STENCIL_INCR";
    static const char* s_maskDecrProfileMarker = "UI_MASK_STENCIL_DECR";

    enum UiColorOp
    {
        ColorOp_Unused = 0,             // reusing shader flag value, FixedPipelineEmu shader uses 0 to mean eCO_NOSET
        ColorOp_Normal = 1,             // reusing shader flag value, FixedPipelineEmu shader uses 1 to mean eCO_DISABLE
        ColorOp_PreMultiplyAlpha = 2    // reusing shader flag value, FixedPipelineEmu shader uses 2 to mean eCO_REPLACE
    };

    enum UiAlphaOp
    {
        AlphaOp_Unused = 0,                 // reusing shader flag value, FixedPipelineEmu shader uses 0 to mean eCO_NOSET
        AlphaOp_Normal = 1,                 // reusing shader flag value, FixedPipelineEmu shader uses 1 to mean eCO_DISABLE
        AlphaOp_ModulateAlpha = 2,          // reusing shader flag value, FixedPipelineEmu shader uses 2 to mean eCO_REPLACE
        AlphaOp_ModulateAlphaAndColor = 3   // reusing shader flag value, FixedPipelineEmu shader uses 3 to mean eCO_DECAL
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    PrimitiveListRenderNode::PrimitiveListRenderNode(const AZ::Data::Instance<AZ::RPI::Image>& texture,
        bool isClampTextureMode, bool isTextureSRGB, bool preMultiplyAlpha, int blendModeState)
        : RenderNode(RenderNodeType::PrimitiveList)
        , m_numTextures(1)
        , m_isTextureSRGB(isTextureSRGB)
        , m_preMultiplyAlpha(preMultiplyAlpha)
        , m_alphaMaskType(AlphaMaskType::None)
        , m_blendModeState(blendModeState)
        , m_totalNumVertices(0)
        , m_totalNumIndices(0)
    {
        m_textures[0].m_texture = texture;
        m_textures[0].m_isClampTextureMode = isClampTextureMode;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    PrimitiveListRenderNode::PrimitiveListRenderNode(const AZ::Data::Instance<AZ::RPI::Image>& texture,
        const AZ::Data::Instance<AZ::RPI::Image>& maskTexture, bool isClampTextureMode, bool isTextureSRGB,
        bool preMultiplyAlpha, AlphaMaskType alphaMaskType, int blendModeState)
        : RenderNode(RenderNodeType::PrimitiveList)
        , m_numTextures(2)
        , m_isTextureSRGB(isTextureSRGB)
        , m_preMultiplyAlpha(preMultiplyAlpha)
        , m_alphaMaskType(alphaMaskType)
        , m_blendModeState(blendModeState)
        , m_totalNumVertices(0)
        , m_totalNumIndices(0)
    {
        m_textures[0].m_texture = texture;
        m_textures[0].m_isClampTextureMode = isClampTextureMode;
        m_textures[1].m_texture = maskTexture;
        m_textures[1].m_isClampTextureMode = isClampTextureMode;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    PrimitiveListRenderNode::~PrimitiveListRenderNode()
    {
        m_primitives.clear();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void PrimitiveListRenderNode::Render(UiRenderer* uiRenderer)
    {
#ifdef LYSHINE_ATOM_TODO // keeping this code for reference for future phase (masks/render targets)
        for (int i = 0; i < m_numTextures; ++i)
        {
            uiRenderer->SetTexture(m_textures[i].m_texture, i, m_textures[i].m_isClampTextureMode);
        }

        int blendModeState = m_blendModeState;

        IRenderer* renderer = gEnv->pRenderer;
        renderer->SetState(blendModeState | uiRenderer->GetBaseState());

        if (m_isTextureSRGB)
        {
            renderer->SetSrgbWrite(false);
        }

        // We are using SetColorOp as a way to set flags for the ui.cfx shader by reusing flags
        // that the FixedPipelineEmu.cfx shader uses. So the names colorOp and alphaOp are used
        // just because this are the inputs to SetColorOp.
        uint8 colorOp = m_preMultiplyAlpha ? ColorOp_PreMultiplyAlpha : ColorOp_Normal;
        uint8 alphaOp = AlphaOp_Normal;
        switch (m_alphaMaskType)
        {
        case AlphaMaskType::None:
            alphaOp = AlphaOp_Normal;
            break;
        case AlphaMaskType::ModulateAlpha:
            alphaOp = AlphaOp_ModulateAlpha;
            break;
        case AlphaMaskType::ModulateAlphaAndColor:
            alphaOp = AlphaOp_ModulateAlphaAndColor;
            break;
        }
        
        renderer->SetColorOp(colorOp, alphaOp, DEF_TEXARG0, DEF_TEXARG0);

        renderer->DrawDynUiPrimitiveList(m_primitives, m_totalNumVertices, m_totalNumIndices);

        if (m_isTextureSRGB)
        {
            renderer->SetSrgbWrite(true);
        }
#endif
        if (!uiRenderer->IsReady())
        {
            return;
        }

        AZ::RHI::Ptr<AZ::RPI::DynamicDrawContext> dynamicDraw = uiRenderer->GetDynamicDrawContext();
        const UiRenderer::UiShaderData& uiShaderData = uiRenderer->GetUiShaderData();

        // Set render state
        dynamicDraw->SetStencilState(uiRenderer->GetBaseState().m_stencilState);
        dynamicDraw->SetTarget0BlendState(uiRenderer->GetBaseState().m_blendState);

        dynamicDraw->SetShaderVariant(uiRenderer->GetCurrentShaderVariant());

        // Set up per draw SRG
        AZ::Data::Instance<AZ::RPI::ShaderResourceGroup> drawSrg = dynamicDraw->NewDrawSrg();

        // Set textures
        uint32_t isClampTextureMode = 0;
        for (int i = 0; i < m_numTextures; ++i)
        {
            const AZ::RHI::ImageView* imageView = m_textures[i].m_texture ? m_textures[i].m_texture->GetImageView() : nullptr;

            if (!imageView)
            {
                // Default to white texture
                auto image = AZ::RPI::ImageSystemInterface::Get()->GetSystemImage(AZ::RPI::SystemImage::White);
                imageView = image->GetImageView();
            }

            if (imageView)
            {
                drawSrg->SetImageView(uiShaderData.m_imageInputIndex, imageView, i);
                if (m_textures[i].m_isClampTextureMode)
                {
                    isClampTextureMode |= (1 << i);
                }
            }
        }

        // Set sampler state per texture
        drawSrg->SetConstant(uiShaderData.m_isClampInputIndex, isClampTextureMode);

        // Set projection matrix
        drawSrg->SetConstant(uiShaderData.m_viewProjInputIndex, uiRenderer->GetModelViewProjectionMatrix());

        drawSrg->Compile();

        // Add the indexed primitives to the dynamic draw context for drawing
        //
        // [LYSHINE_ATOM_TODO][ATOM-15073] - need to combine into a single DrawIndexed call to take advantage of the draw call
        // optimization done by this RenderGraph. This option will be added to DynamicDrawContext. For
        // now we could combine the vertices ourselves
        for (const IRenderer::DynUiPrimitive& primitive : m_primitives)
        {
            dynamicDraw->DrawIndexed(primitive.m_vertices, primitive.m_numVertices, primitive.m_indices, primitive.m_numIndices, AZ::RHI::IndexFormat::Uint16, drawSrg);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void PrimitiveListRenderNode::AddPrimitive(IRenderer::DynUiPrimitive* primitive)
    {
        // always clear the next pointer before adding to list
        primitive->m_next = nullptr;
        m_primitives.push_back(*primitive);

        m_totalNumVertices += primitive->m_numVertices;
        m_totalNumIndices += primitive->m_numIndices;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    IRenderer::DynUiPrimitiveList& PrimitiveListRenderNode::GetPrimitives() const
    {
        return const_cast<IRenderer::DynUiPrimitiveList&>(m_primitives);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    int PrimitiveListRenderNode::GetOrAddTexture(const AZ::Data::Instance<AZ::RPI::Image>& texture, bool isClampTextureMode)
    {
        // Check if node is already using this texture
        int texUnit = FindTexture(texture, isClampTextureMode);

        // render node is not already using this texture, if there is space to add a texture do so
        if (texUnit == -1 && m_numTextures < PrimitiveListRenderNode::MaxTextures)
        {
            texUnit = m_numTextures;
            m_textures[texUnit].m_texture = texture;
            m_textures[texUnit].m_isClampTextureMode = isClampTextureMode;
            m_numTextures++;
        }

        return texUnit;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    bool PrimitiveListRenderNode::HasSpaceToAddPrimitive(IRenderer::DynUiPrimitive* primitive) const
    {
        return primitive->m_numVertices + m_totalNumVertices < std::numeric_limits<uint16>::max();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    int PrimitiveListRenderNode::FindTexture(const AZ::Data::Instance<AZ::RPI::Image>& texture, bool isClampTextureMode) const
    {
        for (int i = 0; i < m_numTextures; ++i)
        {
            if (m_textures[i].m_texture == texture && m_textures[i].m_isClampTextureMode == isClampTextureMode)
            {
                return i;    // texture is already in the list
            }
        }
        return -1;
    }

#ifndef _RELEASE
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void PrimitiveListRenderNode::ValidateNode()
    {
        size_t numPrims = m_primitives.size();
        size_t primCount = 0;
        const IRenderer::DynUiPrimitive* lastPrim = nullptr;
        int highestTexUnit = 0;
        for (const IRenderer::DynUiPrimitive& primitive : m_primitives)
        {
            if (primCount > numPrims)
            {
                AZ_Error("UI", false, "There are more primitives in the m_primitives slist than m_primitives.size (%d)", numPrims)
            }
            primCount++;
            lastPrim = &primitive;

            if (primitive.m_vertices[0].texIndex > highestTexUnit)
            {
                highestTexUnit = primitive.m_vertices[0].texIndex;
            }
        }

        if (m_numTextures != highestTexUnit+1)
        {
            AZ_Error("UI", false, "m_numTextures (%d) is not highestTexUnit+1 (%d)", m_numTextures, highestTexUnit+1)
        }

        if (numPrims > 0 && lastPrim != &*m_primitives.last())
        {
            AZ_Error("UI", false, "lastPrim is not the same as last node")
        }
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    MaskRenderNode::MaskRenderNode(MaskRenderNode* parentMask, bool isMaskingEnabled, bool useAlphaTest, bool drawBehind, bool drawInFront)
        : RenderNode(RenderNodeType::Mask)
        , m_parentMask(parentMask)
        , m_isMaskingEnabled(isMaskingEnabled)
        , m_useAlphaTest(useAlphaTest)
        , m_drawBehind(drawBehind)
        , m_drawInFront(drawInFront)
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    MaskRenderNode::~MaskRenderNode()
    {
        for (RenderNode* renderNode : m_contentRenderNodes)
        {
            delete renderNode;
        }

        m_contentRenderNodes.clear();

        for (RenderNode* renderNode : m_maskRenderNodes)
        {
            AZ_Assert(renderNode->GetType() != RenderNodeType::Mask, "There cannot be mask render nodes in the mask visual");

            delete renderNode;
        }

        m_maskRenderNodes.clear();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void MaskRenderNode::Render(UiRenderer* uiRenderer)
    {
        UiRenderer::BaseState priorBaseState = uiRenderer->GetBaseState();

        if (m_isMaskingEnabled || m_drawBehind)
        {
            SetupBeforeRenderingMask(uiRenderer, true, priorBaseState);
            for (RenderNode* renderNode : m_maskRenderNodes)
            {
                renderNode->Render(uiRenderer);
            }
            SetupAfterRenderingMask(uiRenderer, true, priorBaseState);
        }

        for (RenderNode* renderNode : m_contentRenderNodes)
        {
            renderNode->Render(uiRenderer);
        }

        if (m_isMaskingEnabled || m_drawInFront)
        {
            SetupBeforeRenderingMask(uiRenderer, false, priorBaseState);
            for (RenderNode* renderNode : m_maskRenderNodes)
            {
                renderNode->Render(uiRenderer);
            }
            SetupAfterRenderingMask(uiRenderer, false, priorBaseState);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    bool MaskRenderNode::IsMaskRedundant()
    {
        // if there are no content nodes then there is no point rendering anything for the mask primitives
        // unless the mask primitives are non-empty and we are visually drawing the mask primitives in front or
        // behind of the children.
        if (m_contentRenderNodes.empty() &&
            (m_maskRenderNodes.empty() || (!m_drawBehind && !m_drawInFront)))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

#ifndef _RELEASE
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void MaskRenderNode::ValidateNode()
    {
        for (RenderNode* renderNode : m_maskRenderNodes)
        {
            renderNode->ValidateNode();
        }

        for (RenderNode* renderNode : m_contentRenderNodes)
        {
            renderNode->ValidateNode();
        }
     }
#endif

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void MaskRenderNode::SetupBeforeRenderingMask(UiRenderer* uiRenderer, bool firstPass, UiRenderer::BaseState priorBaseState)
    {
        UiRenderer::BaseState curBaseState = priorBaseState;

        // If using alpha test for drawing the renderable components on this element then we turn on
        // alpha test as a pre-render step
        curBaseState.m_useAlphaTest = m_useAlphaTest;

        // if either of the draw flags are checked then we may want to draw the renderable component(s)
        // on this element, otherwise use the color mask to stop them rendering
        curBaseState.m_blendState.m_enable = false;
        curBaseState.m_blendState.m_writeMask = 0x0;
        if ((m_drawBehind && firstPass) ||
            (m_drawInFront && !firstPass))
        {
            curBaseState.m_blendState.m_enable = true;
            curBaseState.m_blendState.m_writeMask = 0xF;
        }

        if (m_isMaskingEnabled)
        {
            AZ::RHI::StencilOpState stencilOpState;
            stencilOpState.m_func = AZ::RHI::ComparisonFunc::Equal;

            // masking is enabled so we want to setup to increment (first pass) or decrement (second pass)
            // the stencil buff when rendering the renderable component(s) on this element
            if (firstPass)
            {
                stencilOpState.m_passOp = AZ::RHI::StencilOp::Increment;
            }
            else
            {
                stencilOpState.m_passOp = AZ::RHI::StencilOp::Decrement;
            }

            curBaseState.m_stencilState.m_frontFace = stencilOpState;
            curBaseState.m_stencilState.m_backFace = stencilOpState;

            // set up for stencil write
            AZ::RHI::Ptr<AZ::RPI::DynamicDrawContext> dynamicDraw = uiRenderer->GetDynamicDrawContext();
            dynamicDraw->SetStencilReference(uiRenderer->GetStencilRef());
            curBaseState.m_stencilState.m_enable = true;
            curBaseState.m_stencilState.m_writeMask = 0xFF;
        }
        else
        {
            // masking is not enabled
            curBaseState.m_stencilState.m_enable = false;
        }

        uiRenderer->SetBaseState(curBaseState);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void MaskRenderNode::SetupAfterRenderingMask(UiRenderer* uiRenderer, bool firstPass, UiRenderer::BaseState priorBaseState)
    {
        if (m_isMaskingEnabled)
        {
            // Masking is enabled so on the first pass we want to increment the stencil ref stored
            // in the UiRenderer and used by all normal rendering, this is so that it matches the
            // increments to the stencil buffer that we have just done by rendering the mask.
            // On the second pass we want to decrement the stencil ref so it is back to what it
            // was before rendering the normal children of this mask element.
            if (firstPass)
            {
                uiRenderer->IncrementStencilRef();
            }
            else
            {
                uiRenderer->DecrementStencilRef();
            }

            AZ::RHI::Ptr<AZ::RPI::DynamicDrawContext> dynamicDraw = uiRenderer->GetDynamicDrawContext();
            dynamicDraw->SetStencilReference(uiRenderer->GetStencilRef());

            if (firstPass)
            {
                UiRenderer::BaseState curBaseState = priorBaseState;

                // turn off stencil write and turn on stencil test
                curBaseState.m_stencilState.m_enable = true;
                curBaseState.m_stencilState.m_writeMask = 0x00;

                AZ::RHI::StencilOpState stencilOpState;
                stencilOpState.m_func = AZ::RHI::ComparisonFunc::Equal;
                curBaseState.m_stencilState.m_frontFace = stencilOpState;
                curBaseState.m_stencilState.m_backFace = stencilOpState;

                uiRenderer->SetBaseState(curBaseState);
            }
            else
            {
                // second pass, set base state back to what it was before rendering this element
                uiRenderer->SetBaseState(priorBaseState);
            }
        }
        else
        {
            // masking is not enabled
            // remove any color mask or alpha test that we set in pre-render
            uiRenderer->SetBaseState(priorBaseState);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    RenderTargetRenderNode::RenderTargetRenderNode(
        RenderTargetRenderNode* parentRenderTarget,
        int renderTargetHandle,
        SDepthTexture* renderTargetDepthSurface,
        const AZ::Vector2& viewportTopLeft,
        const AZ::Vector2& viewportSize,
        const AZ::Color& clearColor,
        int nestLevel)
        : RenderNode(RenderNodeType::RenderTarget)
        , m_parentRenderTarget(parentRenderTarget)
        , m_renderTargetHandle(renderTargetHandle)
        , m_renderTargetDepthSurface(renderTargetDepthSurface)
        , m_viewportX(viewportTopLeft.GetX())
        , m_viewportY(viewportTopLeft.GetY())
        , m_viewportWidth(viewportSize.GetX())
        , m_viewportHeight(viewportSize.GetY())
        , m_clearColor(clearColor)
        , m_nestLevel(nestLevel)
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    RenderTargetRenderNode::~RenderTargetRenderNode()
    {
        for (RenderNode* renderNode : m_childRenderNodes)
        {
            delete renderNode;
        }

        m_childRenderNodes.clear();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderTargetRenderNode::Render(UiRenderer* uiRenderer)
    {
        if (m_renderTargetHandle <= 0)
        {
            return;
        }

        ISystem* system = gEnv->pSystem;
        if (system && !gEnv->IsDedicated())
        {
            TransformationMatrices backupMatrices;
            gEnv->pRenderer->Set2DModeNonZeroTopLeft(m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, backupMatrices);
 
            // this will change the viewport
            gEnv->pRenderer->SetRenderTarget(m_renderTargetHandle, m_renderTargetDepthSurface);

            // clear the render target before rendering to it
            // NOTE: the FRT_CLEAR_IMMEDIATE is required since we will have already set the render target
            // In theory we could call this before setting the render target without the immediate flag
            // but that doesn't work. Perhaps because FX_Commit is not called.
            ColorF viewportBackgroundColor(m_clearColor.GetR(), m_clearColor.GetG(), m_clearColor.GetB(), m_clearColor.GetA());
            gEnv->pRenderer->ClearTargetsImmediately(FRT_CLEAR, viewportBackgroundColor);

            // we could use SetSrgbWrite to write to a linear texture here. But that gets complicated with
            // having to affect all decsendant element renders. So we just let it write srgb to the render target and
            // allow for that when we render using the render target as a source texture.

            for (RenderNode* renderNode : m_childRenderNodes)
            {
                renderNode->Render(uiRenderer);
            }

            gEnv->pRenderer->SetRenderTarget(0); // restore render target

            gEnv->pRenderer->Unset2DMode(backupMatrices);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    const char* RenderTargetRenderNode::GetRenderTargetName() const
    {
        ITexture* texture = gEnv->pRenderer->EF_GetTextureByID(m_renderTargetHandle);
        return texture->GetName();
    }

#ifndef _RELEASE
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderTargetRenderNode::ValidateNode()
    {
        for (RenderNode* renderNode : m_childRenderNodes)
        {
            renderNode->ValidateNode();
        }
     }
#endif

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    bool RenderTargetRenderNode::CompareNestLevelForSort(RenderTargetRenderNode* a, RenderTargetRenderNode*b)
    {
        // elements with higher nest levels should be rendered first so they should be considered "less than"
        // for the sort
        return a->m_nestLevel > b->m_nestLevel;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    RenderGraph::RenderGraph()
    {
        // we keep track of the list of render nodes that new nodes should be added to. Initially
        // it is the main, top-level list of nodes. If we start defining a mask or render to texture
        // then it becomes the node list for that render node.
        m_renderNodeListStack.push(&m_renderNodes);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    RenderGraph::~RenderGraph()
    {
        ResetGraph();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::ResetGraph()
    {
        // clear and delete the list of render target nodes
        for (RenderNode* renderNode : m_renderTargetRenderNodes)
        {
            delete renderNode;
        }
        m_renderTargetRenderNodes.clear();

        // clear and delete the list of render nodes
        for (RenderNode* renderNode : m_renderNodes)
        {
            delete renderNode;
        }
        m_renderNodes.clear();

        // clear and delete the dynamic quads
        for (DynamicQuad* quad : m_dynamicQuads)
        {
            delete quad;
        }
        m_dynamicQuads.clear();

        m_currentMask = nullptr;
        m_currentRenderTarget = nullptr;

        // clear the render node list stack and reset it to be the top level node list
        while (!m_renderNodeListStack.empty())
        {
            m_renderNodeListStack.pop();
        }
        m_renderNodeListStack.push(&m_renderNodes);

        m_isDirty = true;
        m_renderToRenderTargetCount = 0;

#ifndef _RELEASE  
        m_wasBuiltThisFrame = true;
        m_timeGraphLastBuiltMs = AZStd::GetTimeUTCMilliSecond();
#endif
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::BeginMask(bool isMaskingEnabled, bool useAlphaTest, bool drawBehind, bool drawInFront)
    {
        // this uses pool allocator
        MaskRenderNode* maskRenderNode = new MaskRenderNode(m_currentMask, isMaskingEnabled, useAlphaTest, drawBehind, drawInFront);

        m_currentMask = maskRenderNode;
        m_renderNodeListStack.push(&maskRenderNode->GetMaskRenderNodeList());
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::StartChildrenForMask()
    {
        AZ_Assert(m_currentMask, "Calling StartChildrenForMask while not defining a mask");
        m_renderNodeListStack.pop();
        m_renderNodeListStack.push(&m_currentMask->GetContentRenderNodeList());
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::EndMask()
    {
        AZ_Assert(m_currentMask, "Calling EndMask while not defining a mask");
        if (m_currentMask)
        {
            MaskRenderNode* newMaskRenderNode = m_currentMask;
            m_currentMask = m_currentMask->GetParentMask();

            // pop off the mask's content render node list
            m_renderNodeListStack.pop();

            if (newMaskRenderNode->IsMaskRedundant())
            {
                // We don't know the mask is redundant until we have created this node and found that it hasn't got
                // child nodes. This is not common but does happen sometimes when all the children are currently disabled.
                delete newMaskRenderNode;
            }
            else
            {
                m_renderNodeListStack.top()->push_back(newMaskRenderNode);
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::BeginRenderToTexture(int renderTargetHandle, SDepthTexture* renderTargetDepthSurface,
        const AZ::Vector2& viewportTopLeft, const AZ::Vector2& viewportSize, const AZ::Color& clearColor)
    {
#ifdef LYSHINE_ATOM_TODO // keeping this code for future phase (masks and render targets)
        // this uses pool allocator
        RenderTargetRenderNode* renderTargetRenderNode = new RenderTargetRenderNode(
            m_currentRenderTarget, renderTargetHandle, renderTargetDepthSurface,
            viewportTopLeft, viewportSize, clearColor, m_renderTargetNestLevel);

        m_currentRenderTarget = renderTargetRenderNode;
        m_renderNodeListStack.push(&m_currentRenderTarget->GetChildRenderNodeList());
        m_renderTargetNestLevel++;
#else
        AZ_UNUSED(clearColor);
        AZ_UNUSED(viewportSize);
        AZ_UNUSED(viewportTopLeft);
        AZ_UNUSED(renderTargetDepthSurface);
        AZ_UNUSED(renderTargetHandle);
#endif
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::EndRenderToTexture()
    {
#ifdef LYSHINE_ATOM_TODO // keeping this code for future phase (masks and render targets)
        AZ_Assert(m_currentRenderTarget, "Calling EndRenderToTexture while not defining a render target node");
        if (m_currentRenderTarget)
        {
            RenderTargetRenderNode* newRenderTargetRenderNode = m_currentRenderTarget;
            m_currentRenderTarget = m_currentRenderTarget->GetParentRenderTarget();

            // we don't add this node to the normal list of render nodes since it is rendered before
            // the main render for the render graph
            m_renderTargetRenderNodes.push_back(newRenderTargetRenderNode);

            m_renderNodeListStack.pop();
            m_renderTargetNestLevel--;
        }
#endif
    }

    void RenderGraph::AddPrimitive(
        IRenderer::DynUiPrimitive* primitive,
        ITexture* texture,
        bool isClampTextureMode,
        bool isTextureSRGB,
        bool isTexturePremultipliedAlpha,
        BlendMode blendMode)
    {
        // LYSHINE_ATOM_TODO - this function will be removed when all IRenderer references are gone from UI components
        AZ_UNUSED(primitive);
        AZ_UNUSED(texture);
        AZ_UNUSED(isClampTextureMode);
        AZ_UNUSED(isTextureSRGB);
        AZ_UNUSED(isTexturePremultipliedAlpha);
        AZ_UNUSED(blendMode);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::AddPrimitiveAtom(IRenderer::DynUiPrimitive* primitive, const AZ::Data::Instance<AZ::RPI::Image>& texture,
        bool isClampTextureMode, bool isTextureSRGB, bool isTexturePremultipliedAlpha, BlendMode blendMode)
    {
        AZStd::vector<RenderNode*>* renderNodeList = m_renderNodeListStack.top();

        int texUnit = -1;
        if (renderNodeList)
        {
            // we want to pre-multiply alpha if we are rendering to a render target AND we are not rendering from a render target
            bool isPreMultiplyAlpha = m_renderTargetNestLevel > 0 && !isTexturePremultipliedAlpha;

            // given the blend mode get the right state, the state depends on whether the shader is outputing premultiplied alpha.
            // The shader can be outputing premultiplied alpha EITHER if the input texture is premultiplied alpha OR if the
            // shader is doing the premultiply of the output color
            bool isShaderOutputPremultAlpha = isPreMultiplyAlpha || isTexturePremultipliedAlpha;
            int blendModeState = GetBlendModeState(blendMode, isShaderOutputPremultAlpha);

            PrimitiveListRenderNode* renderNodeToAddTo = nullptr;
            if (!renderNodeList->empty())
            {
                RenderNode* lastRenderNode = renderNodeList->back();

                if (lastRenderNode && lastRenderNode->GetType() == RenderNodeType::PrimitiveList)
                {
                    PrimitiveListRenderNode* primListRenderNode = static_cast<PrimitiveListRenderNode*>(lastRenderNode);

                    // compare render state
                    if (primListRenderNode->GetIsTextureSRGB() == isTextureSRGB &&
                        primListRenderNode->GetBlendModeState() == blendModeState &&
                        primListRenderNode->GetIsPremultiplyAlpha() == isPreMultiplyAlpha &&
                        primListRenderNode->GetAlphaMaskType() ==  AlphaMaskType::None &&
                        primListRenderNode->HasSpaceToAddPrimitive(primitive))
                    {
                        // render state is the same - we can add the primitive to this list if the texture is in
                        // the list or there is space for another texture
                        texUnit = primListRenderNode->GetOrAddTexture(texture, isClampTextureMode);

                        if (texUnit != -1)
                        {
                            renderNodeToAddTo = primListRenderNode;
                        }
                    }
                }
            }

            if (!renderNodeToAddTo)
            {
                // We can't add this primitive to the existing render node, we need to create a new render node
                // this uses a pool allocator for fast allocation
                renderNodeToAddTo = new PrimitiveListRenderNode(texture, isClampTextureMode, isTextureSRGB, isPreMultiplyAlpha, blendModeState);

                renderNodeList->push_back(renderNodeToAddTo);
                texUnit = 0;
            }

            // Ensure that the vertices are referencing the right texture unit
            // Because primitive verts are only created when a UI component changes, they have a longer
            // lifetime than the render graph. So if not much has changed since the render graph was last built
            // it is quite likely that the verts are already set to use the correct texture unit.
            if (primitive->m_vertices[0].texIndex != texUnit)
            {
                for (int i = 0; i < primitive->m_numVertices; ++i)
                {
                    primitive->m_vertices[i].texIndex = texUnit;
                }
            }

            // add this primitive to the render node
            renderNodeToAddTo->AddPrimitive(primitive);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::AddAlphaMaskPrimitive(IRenderer::DynUiPrimitive* primitive,
            ITexture* texture, ITexture* maskTexture,
            bool isClampTextureMode, bool isTextureSRGB, bool isTexturePremultipliedAlpha, BlendMode blendMode)
    {
#ifdef LYSHINE_ATOM_TODO // keeping this code for future phase (masks and render targets)
        AZStd::vector<RenderNode*>* renderNodeList = m_renderNodeListStack.top();

        int texUnit0 = -1;
        int texUnit1 = -1;
        if (renderNodeList)
        {
            // we want to pre-multiply alpha if we are rendering to a render target AND we are not rendering from a render target
            bool isPreMultiplyAlpha = m_renderTargetNestLevel > 0 && !isTexturePremultipliedAlpha;

            // given the blend mode get the right state, the state depends on whether the shader is outputing premultiplied alpha.
            // The shader can be outputing premultiplied alpha EITHER if the input texture is premultiplied alpha OR if the
            // shader is doing the premultiply of the output color
            bool isShaderOutputPremultAlpha = isPreMultiplyAlpha || isTexturePremultipliedAlpha;
            int blendModeState = GetBlendModeState(blendMode, isShaderOutputPremultAlpha);
            AlphaMaskType alphaMaskType = isShaderOutputPremultAlpha ? AlphaMaskType::ModulateAlphaAndColor : AlphaMaskType::ModulateAlpha;

            PrimitiveListRenderNode* renderNodeToAddTo = nullptr;
            if (!renderNodeList->empty())
            {
                RenderNode* lastRenderNode = renderNodeList->back();

                if (lastRenderNode && lastRenderNode->GetType() == RenderNodeType::PrimitiveList)
                {
                    PrimitiveListRenderNode* primListRenderNode = static_cast<PrimitiveListRenderNode*>(lastRenderNode);

                    // compare render state
                    if (primListRenderNode->GetIsTextureSRGB() == isTextureSRGB &&
                        primListRenderNode->GetBlendModeState() == blendModeState &&
                        primListRenderNode->GetIsPremultiplyAlpha() == isPreMultiplyAlpha &&
                        primListRenderNode->GetAlphaMaskType() == alphaMaskType &&
                        primListRenderNode->HasSpaceToAddPrimitive(primitive))
                    {
                        // render state is the same - we can add the primitive to this list if the texture is in
                        // the list or there is space for another texture
                        texUnit0 = primListRenderNode->GetOrAddTexture(texture, true);
                        texUnit1 = primListRenderNode->GetOrAddTexture(maskTexture, true);

                        if (texUnit0 != -1 && texUnit1 != -1)
                        {
                            renderNodeToAddTo = primListRenderNode;
                        }
                    }
                }
            }

            if (!renderNodeToAddTo)
            {
                // We can't add this primitive to the existing render node, we need to create a new render node
                // this uses a pool allocator for fast allocation
                renderNodeToAddTo = new PrimitiveListRenderNode(texture, maskTexture,
                    isClampTextureMode, isTextureSRGB, isPreMultiplyAlpha, alphaMaskType, blendModeState);

                renderNodeList->push_back(renderNodeToAddTo);
                texUnit0 = 0;
                texUnit1 = 1;
            }

            // Ensure that the vertices are referencing the right texture unit
            // Because primitive verts are only created when a UI component changes, they have a longer
            // lifetime than the render graph. So if not much has changed since the render graph was last built
            // it is quite likely that the verts are already set to use the correct texture unit.
            if (primitive->m_vertices[0].texIndex != texUnit0 || primitive->m_vertices[0].texIndex2 != texUnit1)
            {
                for (int i = 0; i < primitive->m_numVertices; ++i)
                {
                    primitive->m_vertices[i].texIndex = texUnit0;
                    primitive->m_vertices[i].texIndex2 = texUnit1;
                }
            }

            // add this primitive to the render node
            renderNodeToAddTo->AddPrimitive(primitive);
        }
#else
        AZ_UNUSED(primitive);
        AZ_UNUSED(texture);
        AZ_UNUSED(maskTexture);
        AZ_UNUSED(isClampTextureMode);
        AZ_UNUSED(isTextureSRGB);
        AZ_UNUSED(isTexturePremultipliedAlpha);
        AZ_UNUSED(blendMode);
#endif
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    IRenderer::DynUiPrimitive* RenderGraph::GetDynamicQuadPrimitive(const AZ::Vector2* positions, uint32 packedColor)
    {
        const int numVertsInQuad = 4;
        const int numIndicesInQuad = 6;

        // points are a clockwise quad
        static const Vec2 uvs[numVertsInQuad] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };

        static uint16 indices[numIndicesInQuad] = { 0, 1, 2, 2, 3, 0 };

        DynamicQuad* quad = new DynamicQuad;
        for (int i = 0; i < numVertsInQuad; ++i)
        {
            quad->m_quadVerts[i].xy = Vec2(positions[i].GetX(), positions[i].GetY());
            quad->m_quadVerts[i].color.dcolor = packedColor;
            quad->m_quadVerts[i].st = uvs[i];
            quad->m_quadVerts[i].texIndex = 0;
            quad->m_quadVerts[i].texHasColorChannel = 1;
            quad->m_quadVerts[i].texIndex2 = 0;
            quad->m_quadVerts[i].pad = 0;
        }

        quad->m_primitive.m_vertices = quad->m_quadVerts;
        quad->m_primitive.m_numVertices = numVertsInQuad;
        quad->m_primitive.m_indices = indices;
        quad->m_primitive.m_numIndices = numIndicesInQuad;

        m_dynamicQuads.push_back(quad);

        return &quad->m_primitive;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    bool RenderGraph::IsRenderingToMask() const
    {
        return m_isRenderingToMask;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::SetIsRenderingToMask(bool isRenderingToMask)
    {
        m_isRenderingToMask = isRenderingToMask;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::PushAlphaFade(float alphaFadeValue)
    {
        float currentAlphaFade = GetAlphaFade();
        m_alphaFadeStack.push(alphaFadeValue * currentAlphaFade);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::PushOverrideAlphaFade(float alphaFadeValue)
    {
        m_alphaFadeStack.push(alphaFadeValue);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::PopAlphaFade()
    {
        if (!m_alphaFadeStack.empty())
        {
            m_alphaFadeStack.pop();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    float RenderGraph::GetAlphaFade() const
    {
        float alphaFade = 1.0f; // by default nothing is faded

        if (!m_alphaFadeStack.empty())
        {
            alphaFade = m_alphaFadeStack.top();
        }
        return alphaFade;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::Render(UiRenderer* uiRenderer, const AZ::Vector2& viewportSize)
    {
        // LYSHINE_ATOM_TODO - will probably need to support this when converting UI Editor to use Atom
        AZ_UNUSED(viewportSize);

        AZ::RHI::Ptr<AZ::RPI::DynamicDrawContext> dynamicDraw = uiRenderer->GetDynamicDrawContext();

        // Disable stencil and enable blend/color write
        dynamicDraw->SetStencilState(uiRenderer->GetBaseState().m_stencilState);
        dynamicDraw->SetTarget0BlendState(uiRenderer->GetBaseState().m_blendState);

        // First render the render targets, they are sorted so that more deeply nested ones are rendered first.

#ifdef LYSHINE_ATOM_TODO // keeping this code for reference for future phase (render targets)
        // They only need to be rendered the first time that a render graph is rendered after it has been built.
        // Though there is a special case, if this is the first time a shader variant has been used it can miss
        // the first render. So to be safe we only stop rendering to render targets after we have rendered to
        // them twice with no shader compiles initiated.
        if (m_renderToRenderTargetCount < 2)
        {
            for (RenderNode* renderNode : m_renderTargetRenderNodes)
            {
                renderNode->Render(uiRenderer);
            }

            // if the render targets render OK we don't need to render them every frame. But if a new shader
            // variant needed to be compiled then they will not have rendered OK. So we check is there are
            // any shaders still in the process of compiling. Because they are compiled on the render
            // thread, we may not know until the next frame that a shader needed to be compiled. So we need
            // the counter.
            SShaderCacheStatistics stats;
            gEnv->pRenderer->EF_Query(EFQ_GetShaderCacheInfo, stats);
            bool waitingOnShadersToCompile = stats.m_nNumShaderAsyncCompiles > 0 ? true : false;
            if (!waitingOnShadersToCompile)
            {
                m_renderToRenderTargetCount++;
            }
            else
            {
                m_renderToRenderTargetCount = 0;
            }
        }
#else
        for (RenderNode* renderNode : m_renderTargetRenderNodes)
        {
            renderNode->Render(uiRenderer);
        }
#endif

#ifdef LYSHINE_ATOM_TODO // keeping this code for reference for future phase (UI Editor)
        // Set2DMode defines the viewport so we set it to canvas viewport here (the render target render nodes 
        // above will have set the viewport as they needed).
        TransformationMatrices backupMatrices;
        gEnv->pRenderer->Set2DMode(static_cast<uint32>(viewportSize.GetX()), static_cast<uint32>(viewportSize.GetY()), backupMatrices);
#endif
        for (RenderNode* renderNode : m_renderNodes)
        {
            renderNode->Render(uiRenderer);
        }
#ifdef LYSHINE_ATOM_TODO // keeping this code for reference for future phase (UI Editor)
        // end the 2D mode
        gEnv->pRenderer->Unset2DMode(backupMatrices);
#endif
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::SetDirtyFlag(bool isDirty)
    {
        if (m_isDirty != isDirty)
        {
            if (isDirty)
            {
                // when graph first becomes dirty it must be reset since an element may have been deleted
                // and the graph contains pointers to DynUiPrimitives owned by components on elements.
                ResetGraph();
            }
            m_isDirty = isDirty;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    bool RenderGraph::GetDirtyFlag()
    {
        return m_isDirty;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::FinalizeGraph()
    {
        // sort the render targets so that more deeply nested ones are rendered first
        std::sort(m_renderTargetRenderNodes.begin(), m_renderTargetRenderNodes.end(),
            RenderTargetRenderNode::CompareNestLevelForSort);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    bool RenderGraph::IsEmpty()
    {
        return m_renderNodes.empty();
    }

#ifndef _RELEASE
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::ValidateGraph()
    {
        for (RenderNode* renderNode : m_renderNodes)
        {
            renderNode->ValidateNode();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::GetDebugInfoRenderGraph(LyShineDebug::DebugInfoRenderGraph& info) const
    {
        info.m_numPrimitives = 0;
        info.m_numRenderNodes = 0;
        info.m_numTriangles = 0;
        info.m_numUniqueTextures = 0;
        info.m_numMasks = 0;
        info.m_numRTs = 0;
        info.m_numNodesDueToMask = 0;
        info.m_numNodesDueToRT = 0;
        info.m_numNodesDueToBlendMode = 0;
        info.m_numNodesDueToSrgb = 0;
        info.m_numNodesDueToMaxVerts = 0;
        info.m_numNodesDueToTextures = 0;
        info.m_wasBuiltThisFrame = m_wasBuiltThisFrame;
        info.m_timeGraphLastBuiltMs = m_timeGraphLastBuiltMs;
        info.m_isReusingRenderTargets = m_renderToRenderTargetCount >= 2 && !m_renderTargetRenderNodes.empty();

        m_wasBuiltThisFrame = false;

        AZStd::set<AZ::Data::Instance<AZ::RPI::Image>> uniqueTextures;

        // If we are rendering to the render targets this frame then record the stats for doing that
        if (m_renderToRenderTargetCount < 2)
        {
            for (RenderNode* renderNode : m_renderTargetRenderNodes)
            {
                const RenderTargetRenderNode* renderTargetRenderNode = static_cast<const RenderTargetRenderNode*>(renderNode);

                if (renderTargetRenderNode->GetChildRenderNodeList().size() > 0)
                {
                    info.m_numNodesDueToRT += 1; // there is an extra draw call because these are inside a render target (so can't be combined with those outside)
                }

                ++info.m_numRTs;
                const AZStd::vector<RenderNode*>& childNodeList = renderTargetRenderNode->GetChildRenderNodeList();

                // walk the rendertarget's graph recursively to add up all of the data
                GetDebugInfoRenderNodeList(childNodeList, info, uniqueTextures);
            }
        }
    
        // walk the graph recursively to add up all of the data
        GetDebugInfoRenderNodeList(m_renderNodes, info, uniqueTextures);

        info.m_numUniqueTextures = uniqueTextures.size();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::GetDebugInfoRenderNodeList(
        const AZStd::vector<RenderNode*>& renderNodeList,
        LyShineDebug::DebugInfoRenderGraph& info,
        AZStd::set<AZ::Data::Instance<AZ::RPI::Image>>& uniqueTextures) const
    {
        const PrimitiveListRenderNode* prevPrimListNode = nullptr;
        bool isFirstNode = true;
        bool wasLastNodeAMask = false;
        for (const RenderNode* renderNode : renderNodeList)
        {
            ++info.m_numRenderNodes;

            if (renderNode->GetType() == RenderNodeType::Mask)
            {
                const MaskRenderNode* maskRenderNode = static_cast<const MaskRenderNode*>(renderNode);

                if (maskRenderNode->GetMaskRenderNodeList().size() > 0)
                {
                    info.m_numNodesDueToMask += 1; // there are always 2 draw calls for a mask so the mask adds one even if it is the first element
                }
                if (maskRenderNode->GetContentRenderNodeList().size() > 0)
                {
                    info.m_numNodesDueToMask += 1; // there is an extra draw call because these are inside a mask (so can't be combined with those outside)
                }
                if (!isFirstNode)
                {
                    info.m_numNodesDueToMask += 1; // caused a break from the previous due to a mask
                }

                wasLastNodeAMask = true;
                ++info.m_numMasks;

                GetDebugInfoRenderNodeList(maskRenderNode->GetContentRenderNodeList(), info, uniqueTextures);
                if (maskRenderNode->GetIsMaskingEnabled())
                {
                    GetDebugInfoRenderNodeList(maskRenderNode->GetMaskRenderNodeList(), info, uniqueTextures);
                }

                prevPrimListNode = nullptr;
            }
            else if (renderNode->GetType() == RenderNodeType::PrimitiveList)
            {
                if (wasLastNodeAMask)
                {
                    info.m_numNodesDueToMask += 1; // this could not be combined with the render nodes before the mask
                    wasLastNodeAMask = false;
                }

                const PrimitiveListRenderNode* primListRenderNode = static_cast<const PrimitiveListRenderNode*>(renderNode);
                
                IRenderer::DynUiPrimitiveList& primitives = primListRenderNode->GetPrimitives();
                info.m_numPrimitives += primitives.size();
                {
                    for (const IRenderer::DynUiPrimitive& primitive : primitives)
                    {
                        info.m_numTriangles += primitive.m_numIndices / 3;
                    }
                }

                for (int i = 0; i < primListRenderNode->GetNumTextures(); ++i)
                {
                    uniqueTextures.insert(primListRenderNode->GetTexture(i));
                }

                if (prevPrimListNode)
                {
                    if (prevPrimListNode->GetBlendModeState() != primListRenderNode->GetBlendModeState())
                    {
                        ++info.m_numNodesDueToBlendMode;
                    }
                    else if (prevPrimListNode->GetIsTextureSRGB() != primListRenderNode->GetIsTextureSRGB())
                    {
                        ++info.m_numNodesDueToSrgb;
                    }
                    else if (!prevPrimListNode->HasSpaceToAddPrimitive(&primListRenderNode->GetPrimitives().front()))
                    {
                        ++info.m_numNodesDueToMaxVerts;
                    }
                    else if (prevPrimListNode->GetNumTextures() == PrimitiveListRenderNode::MaxTextures)
                    {
                        ++info.m_numNodesDueToTextures;
                    }
                }

                prevPrimListNode = primListRenderNode;
            }

            isFirstNode = false;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::DebugReportDrawCalls(AZ::IO::HandleType fileHandle, LyShineDebug::DebugInfoDrawCallReport& reportInfo, void* context) const
    {
        if (m_renderNodes.empty())
        {
            AZStd::string logLine = "Rendergraph is empty\r\n";
            AZ::IO::LocalFileIO::GetInstance()->Write(fileHandle, logLine.c_str(), logLine.size());
        }
        else
        {
            // first list the render nodes for creating render targets
            for (const RenderNode* renderNode : m_renderTargetRenderNodes)
            {
                const RenderTargetRenderNode* renderTargetRenderNode = static_cast<const RenderTargetRenderNode*>(renderNode);
                const char* renderTargetName = renderTargetRenderNode->GetRenderTargetName();

                AZ::Color clearColor = renderTargetRenderNode->GetClearColor();
                AZStd::string logLine = AZStd::string::format("RenderTarget %s (ClearColor=(%f,%f,%f), ClearAlpha=%f, Viewport=(%f,%f,%f,%f)) :\r\n",
                    renderTargetName, 
                    static_cast<float>(clearColor.GetR()), static_cast<float>(clearColor.GetG()), static_cast<float>(clearColor.GetB()), static_cast<float>(clearColor.GetA()),
                    renderTargetRenderNode->GetViewportX(),
                    renderTargetRenderNode->GetViewportY(),
                    renderTargetRenderNode->GetViewportWidth(),
                    renderTargetRenderNode->GetViewportHeight());
                AZ::IO::LocalFileIO::GetInstance()->Write(fileHandle, logLine.c_str(), logLine.size());

                const AZStd::vector<RenderNode*>& childNodeList = renderTargetRenderNode->GetChildRenderNodeList();
                AZStd::string indent = "  ";
                DebugReportDrawCallsRenderNodeList(childNodeList, fileHandle, reportInfo, context, indent);

                // write blank separator line
                logLine = "\r\n";
                AZ::IO::LocalFileIO::GetInstance()->Write(fileHandle, logLine.c_str(), logLine.size());
            }

            AZStd::string logLine = "Main render target:\r\n";
            AZ::IO::LocalFileIO::GetInstance()->Write(fileHandle, logLine.c_str(), logLine.size());

            // Recursively visit all the render nodes
            AZStd::string indent = "  ";
            DebugReportDrawCallsRenderNodeList(m_renderNodes, fileHandle, reportInfo, context, indent);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    void RenderGraph::DebugReportDrawCallsRenderNodeList(
        const AZStd::vector<RenderNode*>& renderNodeList,
        AZ::IO::HandleType fileHandle,
        LyShineDebug::DebugInfoDrawCallReport& reportInfo,
        void* context,
        const AZStd::string& indent) const
    {
        AZStd::string logLine;

        bool previousNodeAlreadyCounted = false;

        const PrimitiveListRenderNode* prevPrimListNode = nullptr;
        for (const RenderNode* renderNode : renderNodeList)
        {
            if (renderNode->GetType() == RenderNodeType::Mask)
            {
                const MaskRenderNode* maskRenderNode = static_cast<const MaskRenderNode*>(renderNode);

                AZStd::string newIndent = indent + "    ";

                logLine = AZStd::string::format("%sMask (MaskEnabled=%d, UseAlphaTest=%d, DrawBehind=%d, DrawInFront=%d) :\r\n",
                    indent.c_str(), 
                    static_cast<int>(maskRenderNode->GetIsMaskingEnabled()),
                    static_cast<int>(maskRenderNode->GetUseAlphaTest()),
                    static_cast<int>(maskRenderNode->GetDrawBehind()),
                    static_cast<int>(maskRenderNode->GetDrawInFront()));
                AZ::IO::LocalFileIO::GetInstance()->Write(fileHandle, logLine.c_str(), logLine.size());

                logLine = AZStd::string::format("%s  Mask shape render nodes:\r\n", indent.c_str());
                AZ::IO::LocalFileIO::GetInstance()->Write(fileHandle, logLine.c_str(), logLine.size());
                DebugReportDrawCallsRenderNodeList(maskRenderNode->GetMaskRenderNodeList(), fileHandle, reportInfo, context, newIndent);

                logLine = AZStd::string::format("%s  Mask content render nodes:\r\n", indent.c_str());
                AZ::IO::LocalFileIO::GetInstance()->Write(fileHandle, logLine.c_str(), logLine.size());
                DebugReportDrawCallsRenderNodeList(maskRenderNode->GetContentRenderNodeList(), fileHandle, reportInfo, context, newIndent);

                prevPrimListNode = nullptr;
            }
            else if (renderNode->GetType() == RenderNodeType::PrimitiveList)
            {
                const PrimitiveListRenderNode* primListRenderNode = static_cast<const PrimitiveListRenderNode*>(renderNode);
                
                bool nodeExistsBecauseOfExceedingMaxTextures = false;
                if (prevPrimListNode)
                {
                    if (prevPrimListNode->GetBlendModeState() == primListRenderNode->GetBlendModeState() &&
                        prevPrimListNode->GetIsTextureSRGB() == primListRenderNode->GetIsTextureSRGB() &&
                        prevPrimListNode->HasSpaceToAddPrimitive(&primListRenderNode->GetPrimitives().front()) &&
                        prevPrimListNode->GetNumTextures() == PrimitiveListRenderNode::MaxTextures)
                    {
                        // this node could have been combined with the previous node if less unique textures were used
                        // so this is an opportunity for texture atlases to reduce draw calls
                        nodeExistsBecauseOfExceedingMaxTextures = true;
                    }
                }

                // If this render node was created because the previous render node ran out of textures
                // then we need to record the previous render node's textures as contributing to exceeding
                // the max textures.
                if (nodeExistsBecauseOfExceedingMaxTextures)
                {
                    if (!previousNodeAlreadyCounted)
                    {
                        for (int i = 0; i < prevPrimListNode->GetNumTextures(); ++i)
                        {
                            AZ::Data::Instance<AZ::RPI::Image> texture = prevPrimListNode->GetTexture(i);
                            if (!texture)
                            {
                                texture = AZ::RPI::ImageSystemInterface::Get()->GetSystemImage(AZ::RPI::SystemImage::White);
                            }
                            bool isClampTextureUsage = prevPrimListNode->GetTextureIsClampMode(i);

                            LyShineDebug::DebugInfoTextureUsage* matchingTextureUsage = nullptr;
                            // The texture should already be in reportInfo because we have already visited the previous
                            // render node.
                            for (LyShineDebug::DebugInfoTextureUsage& reportTextureUsage : reportInfo.m_textures)
                            {
                                if (reportTextureUsage.m_texture == texture &&
                                    reportTextureUsage.m_isClampTextureUsage == isClampTextureUsage)
                                {
                                    matchingTextureUsage = &reportTextureUsage;
                                    break;
                                }
                            }

                            if (matchingTextureUsage)
                            {
                                matchingTextureUsage->m_numDrawCallsWhereExceedingMaxTextures++;
                            }
                        }
                        previousNodeAlreadyCounted = true;
                    }
                }
                else
                {
                    previousNodeAlreadyCounted = false;
                }

                IRenderer::DynUiPrimitiveList& primitives = primListRenderNode->GetPrimitives();
                int numPrimitives = primitives.size();
                int numTriangles = 0;
                for (const IRenderer::DynUiPrimitive& primitive : primitives)
                {
                    numTriangles += primitive.m_numIndices / 3;
                }

                // Write heading to logfile for this render node
                logLine = AZStd::string::format("%sPrimitive render node (Blend mode=%d, SRGB=%d). NumPrims=%d, NumTris=%d. Using textures:\r\n",
                    indent.c_str(), primListRenderNode->GetBlendModeState(),
                    static_cast<int>(primListRenderNode->GetIsTextureSRGB()),
                    numPrimitives, numTriangles);
                AZ::IO::LocalFileIO::GetInstance()->Write(fileHandle, logLine.c_str(), logLine.size());

                for (int i = 0; i < primListRenderNode->GetNumTextures(); ++i)
                {
                    AZ::Data::Instance<AZ::RPI::Image> texture = primListRenderNode->GetTexture(i);
                    if (!texture)
                    {
                        texture = AZ::RPI::ImageSystemInterface::Get()->GetSystemImage(AZ::RPI::SystemImage::White);
                    }
                    bool isClampTextureUsage = primListRenderNode->GetTextureIsClampMode(i);

                    LyShineDebug::DebugInfoTextureUsage* matchingTextureUsage = nullptr;

                    // Write line to logfile for this texture
                    AZStd::string textureName;
                    AZ::Data::AssetCatalogRequestBus::BroadcastResult(textureName, &AZ::Data::AssetCatalogRequests::GetAssetPathById, texture->GetAssetId());
                    logLine = AZStd::string::format("%s  %s\r\n", indent.c_str(), textureName.c_str());
                    AZ::IO::LocalFileIO::GetInstance()->Write(fileHandle, logLine.c_str(), logLine.size());

                    // see if texture is in reportInfo
                    for (LyShineDebug::DebugInfoTextureUsage& reportTextureUsage : reportInfo.m_textures)
                    {
                        if (reportTextureUsage.m_texture == texture &&
                            reportTextureUsage.m_isClampTextureUsage == isClampTextureUsage)
                        {
                            matchingTextureUsage = &reportTextureUsage;
                            break;
                        }
                    }

                    if (!matchingTextureUsage)
                    {
                        // Texture is not already in reportInfo so add it
                        LyShineDebug::DebugInfoTextureUsage newTextureUsage;
                        newTextureUsage.m_texture = texture;
                        newTextureUsage.m_isClampTextureUsage = isClampTextureUsage;
                        newTextureUsage.m_numCanvasesUsed = 0;
                        newTextureUsage.m_numDrawCallsUsed = 0;
                        newTextureUsage.m_numDrawCallsWhereExceedingMaxTextures = 0;
                        newTextureUsage.m_lastContextUsed = nullptr;
                        reportInfo.m_textures.push_back(newTextureUsage);
                        matchingTextureUsage = &reportInfo.m_textures.back();
                    }

                    matchingTextureUsage->m_numDrawCallsUsed++;
                    if (nodeExistsBecauseOfExceedingMaxTextures)
                    {
                        matchingTextureUsage->m_numDrawCallsWhereExceedingMaxTextures++;
                    }

                    if (matchingTextureUsage->m_lastContextUsed != context)
                    {
                        matchingTextureUsage->m_numCanvasesUsed++;
                        matchingTextureUsage->m_lastContextUsed = context;
                    }
                }

                prevPrimListNode = primListRenderNode;
            }
        }
    }

#endif

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    int RenderGraph::GetBlendModeState(LyShine::BlendMode blendMode, bool isShaderOutputPremultAlpha) const
    {   
        // Our blend modes are complicated by the fact we want to be able to render to a render target and then
        // render from that render target texture to the back buffer and get the same result as if we rendered
        // directly to the back buffer. This should be true even if the render target texture does not end up
        // fully opaque.
        // If the blend mode is LyShine::BlendMode::Normal and we just use GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA
        // then this doesn't work for render targets that end up with transparency. To make it work the alpha has to be
        // accumulated as we render it into the alpha channel of the render target. If we use:
        // GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA it gets used for both the color blend op and the alpha blend op
        // so we end up with:         dstAlpha = srcAlpha * srcAlpha + dstAlpha * (1-srcAlpha).
        // This does not accumulate properly.
        // What we actually want is:  dstAlpha = srcAlpha + dstAlpha * (1-srcAlpha)
        // So that would mean for alpha we want GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA
        // If the IRenderer::SetState allowed us to set the alpha and color blend op separately that would be pretty simple.
        // However, it does not. So we use a work around. We use a variant of the shader that premultiplies the output
        // color by the output alpha. So using that variant means that:
        // GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA
        // will give the same *color* result as GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA
        // while giving us the alpha result that we want.
        //
        // For blend modes other than LyShine::BlendMode::Normal we make similar adjustments. This works well for
        // LyShine::BlendMode::Add. For the other three blend modes we cannot get the same results - but the results
        // for those blend modes have always been inadequate. Until we get full control over the blend ops
        // we won't be able to properly support those blend modes by using blend states. Even then to do them
        // properly might require shader changes also. For the moment using the blend modes Screen, Darken, Lighten
        // is not encouraged, especially when rendering to a render target.

        int flags = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA; // the default
        switch (blendMode)
        {
        case LyShine::BlendMode::Normal:
            // This is the default mode that does an alpha blend by interpolating based on src alpha
            if (isShaderOutputPremultAlpha)
            {
                flags = GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCALPHA;
            }
            else
            {
                flags = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;
            }
            break;
        case LyShine::BlendMode::Add:
            // This works well, the amount of the src color added is controlled by src alpha
            if (isShaderOutputPremultAlpha)
            {
                flags = GS_BLSRC_ONE | GS_BLDST_ONE;
            }
            else
            {
                flags = GS_BLSRC_SRCALPHA | GS_BLDST_ONE;
            }
            break;
        case LyShine::BlendMode::Screen:
            // This is a poor approximation of the PhotoShop Screen mode but trying to take some account of src alpha
            // In Photoshop this would be 1 - ( (1-SrcColor) * (1-DstColor) )
            // So we should use a blend op of multiply but the IRenderer interface doesn't support that. We get some multiply
            // from GS_BLDST_ONEMINUSSRCCOL which multiplies the DstColor by (1-SrcColor)
            if (isShaderOutputPremultAlpha)
            {
                flags = GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCCOL;
            }
            else
            {
                flags = GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCCOL;
            }
            break;
        case LyShine::BlendMode::Darken:
            // This is a poor approximation of the PhotoShop Darken mode but trying to take some account of src alpha
            // In Photoshop Darken means min(SrcColor, DstColor)
            if (isShaderOutputPremultAlpha)
            {
                flags = GS_BLOP_MIN | GS_BLSRC_ONE | GS_BLDST_ONE | GS_BLALPHA_MAX;
            }
            else
            {
                flags = GS_BLOP_MIN | GS_BLSRC_ONEMINUSSRCALPHA | GS_BLDST_ONE;
            }
            break;
        case LyShine::BlendMode::Lighten:
            // This is a pretty good an approximation of the PhotoShop Lighten mode but trying to take some account of src alpha
            // In PhotoShop Lighten means max(SrcColor, DstColor)
            if (isShaderOutputPremultAlpha)
            {
                flags = GS_BLOP_MAX | GS_BLSRC_ONE| GS_BLDST_ONE;
            }
            else
            {
                flags = GS_BLOP_MAX | GS_BLSRC_SRCALPHA | GS_BLDST_ONE;
            }
            break;
        }

        return flags;
    }

}
