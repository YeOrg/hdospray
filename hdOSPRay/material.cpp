// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "material.h"
#include "texture.h"

#include <pxr/base/gf/vec3f.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/sdf/assetPath.h>

#include "config.h"
#include "context.h"

#include <OpenImageIO/imageio.h>
#include <pxr/imaging/hdSt/material.h>
#include <pxr/imaging/hdSt/resourceRegistry.h>
#include <pxr/imaging/hdSt/shaderCode.h>

#include <ospray/ospray_cpp.h>
#include <ospray/ospray_cpp/ext/rkcommon.h>

using namespace rkcommon::math;

OIIO_NAMESPACE_USING

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    HdOSPRayMaterialTokens,
    (UsdPreviewSurface)
    (HwPtexTexture_1)
    (UsdTransform2d)
    (sourceColorSpace)
    (diffuseColor)
    (specularColor)
    (emissiveColor)
    (metallic)
    (roughness)
    (clearcoat)
    (clearcoatRoughness)
    (ior)
    (color)
    (opacity)
    (UsdUVTexture)
    (normal)
    (displacement)
    (file)
    (filename)
    ( (infoFilename, "info:filename"))
    (scale)
    (wrapS)
    (wrapT)
    (repeat)
    (mirror)
    (rotation)
    (translation)
);

// clang-format on

HdOSPRayMaterial::HdOSPRayMaterial(SdfPath const& id)
    : HdMaterial(id)
{
    diffuseColor = GfVec3f(1, 1, 1);
}

void
HdOSPRayMaterial::Sync(HdSceneDelegate* sceneDelegate,
                       HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    TF_UNUSED(renderParam);

    // if material dirty, update
    if (*dirtyBits & HdMaterial::DirtyResource) {
        //  find material network
        VtValue networkMapResource
               = sceneDelegate->GetMaterialResource(GetId());
        HdMaterialNetworkMap networkMap
               = networkMapResource.Get<HdMaterialNetworkMap>();
        HdMaterialNetwork matNetwork;

        TF_FOR_ALL (itr, networkMap.map) {
            auto& network = itr->second;
            TF_FOR_ALL (node, network.nodes) {
                if (node->identifier
                    == HdOSPRayMaterialTokens->UsdPreviewSurface)
                    matNetwork = network;
            }
        }

        // process each material node based on type
        TF_FOR_ALL (node, matNetwork.nodes) {
            if (node->identifier == HdOSPRayMaterialTokens->UsdPreviewSurface)
                _ProcessUsdPreviewSurfaceNode(*node);
            else if (node->identifier == HdOSPRayMaterialTokens->UsdUVTexture
                     || node->identifier
                            == HdOSPRayMaterialTokens->HwPtexTexture_1) {
                // texture node found

                // determine if texture is actively used
                auto relationships = matNetwork.relationships;
                auto relationship = std::find_if(
                       relationships.begin(), relationships.end(),
                       [&node](HdMaterialRelationship const& rel) {
                           return rel.inputId == node->path;
                       });
                if (relationship == relationships.end()) {
                    continue; // ignore unused texture
                }

                TfToken inputNameToken = relationship->inputName;
                TfToken texNameToken = relationship->outputName;
                _ProcessTextureNode(*node, inputNameToken, texNameToken);
            } else if (node->identifier
                       == HdOSPRayMaterialTokens->UsdTransform2d) {
                // calculate transform2d to be used on a texture
                auto relationships = matNetwork.relationships;
                auto relationship = std::find_if(
                       relationships.begin(), relationships.end(),
                       [&node](HdMaterialRelationship const& rel) {
                           return rel.inputId == node->path;
                       });
                if (relationship == relationships.end()) {
                    continue; // skip unused nodes
                }

                TfToken texNameToken = relationship->outputName;

                // node is used, find what param it representds, ie diffuseColor
                auto relationship2 = std::find_if(
                       relationships.begin(), relationships.end(),
                       [&relationship](HdMaterialRelationship const& rel) {
                           return rel.inputId == relationship->outputId;
                       });
                if (relationship2 == relationships.end()) {
                    continue; // skip unused nodes
                }
                _ProcessTransform2dNode(*node, relationship2->outputName);
            }
        }

        _UpdateOSPRayMaterial();

        *dirtyBits = Clean;
    }
}

void
HdOSPRayMaterial::_UpdateOSPRayMaterial()
{
    std::string rendererType = HdOSPRayConfig::GetInstance().usePathTracing
           ? "pathtracer"
           : "scivis";

    if (rendererType == "pathtracer") {
        if (!HdOSPRayConfig::GetInstance().useSimpleMaterial) {
            _ospMaterial = CreatePrincipledMaterial(rendererType);
        } else {
            _ospMaterial = CreateSimpleMaterial(rendererType);
        }
    } else {
        _ospMaterial = CreateScivisMaterial(rendererType);
    }

    _ospMaterial.commit();
}

void
HdOSPRayMaterial::_ProcessUsdPreviewSurfaceNode(HdMaterialNode node)
{
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayMaterialTokens->diffuseColor) {
            diffuseColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->specularColor) {
            specularColor = value.Get<GfVec3f>();
        } else if (name == HdOSPRayMaterialTokens->metallic) {
            metallic = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->roughness) {
            roughness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->ior) {
            ior = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->clearcoatRoughness) {
            coatRoughness = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->clearcoat) {
            coat = value.Get<float>();
        } else if (name == HdOSPRayMaterialTokens->opacity) {
            opacity = value.Get<float>();
        }
    }
}

void
HdOSPRayMaterial::_ProcessTextureNode(HdMaterialNode node, TfToken inputName,
                                      TfToken outputName)
{
    bool isPtex = node.identifier == HdOSPRayMaterialTokens->HwPtexTexture_1;
    bool isUdim = false;

    if (_textures.find(outputName) == _textures.end())
        _textures[outputName] = HdOSPRayTexture();
    HdOSPRayTexture& texture = _textures[outputName];
    std::string filename = "";
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayMaterialTokens->file
            || name == HdOSPRayMaterialTokens->filename
            || name == HdOSPRayMaterialTokens->infoFilename) {
            SdfAssetPath const& path = value.Get<SdfAssetPath>();
            filename = path.GetResolvedPath();
        } else if (name == HdOSPRayMaterialTokens->scale) {
            texture.scale = value.Get<GfVec4f>();
        } else if (name == HdOSPRayMaterialTokens->wrapS) {
        } else if (name == HdOSPRayMaterialTokens->wrapT) {
        } else if (name == HdOSPRayMaterialTokens->sourceColorSpace) {
        } else {
            TF_CODING_ERROR("unhandled token: %s\n", name.GetString().c_str());
        }
    }

    if (filename != "") {
        texture.file = filename;
        isUdim = TfStringContains(texture.file, "<UDIM>");
        if (isPtex) {
            hasPtex = true;
            texture.isPtex = true;
#ifdef HDOSPRAY_PLUGIN_PTEX
            texture.ospTexture = LoadPtexTexture(texture.file);
#endif
        } else if (isUdim) {
            int numX, numY;
            const auto& result = LoadUDIMTexture2D(
                   texture.file, numX, numY, false,
                   (outputName == HdOSPRayMaterialTokens->opacity));
            texture.ospTexture = result.first;
            texture.hasXfm = true;
            texture.xfm_scale = { 1.f / float(numX), 1.f / float(numY) };
            // OSPRay scales around the center (0.5, 0.5).  translate
            // texture from (0.5, 0.5) to (0,0)
            texture.xfm_translation
                   = { -(.5f - .5f / float(numX)), -(.5f - .5f / float(numY)) };
        } else {
            const auto& result = LoadOIIOTexture2D(
                   texture.file, inputName.GetString(), false,
                   (outputName == HdOSPRayMaterialTokens->opacity));
            texture.ospTexture = result.first;
        }
    }

    if (texture.ospTexture)
        texture.ospTexture.commit();
}

void
HdOSPRayMaterial::_ProcessTransform2dNode(HdMaterialNode node,
                                          TfToken textureName)
{
    if (_textures.find(textureName) == _textures.end())
        _textures[textureName] = HdOSPRayTexture();
    HdOSPRayTexture& texture = _textures[textureName];
    TF_FOR_ALL (param, node.parameters) {
        const auto& name = param->first;
        const auto& value = param->second;
        if (name == HdOSPRayMaterialTokens->scale) {
            texture.xfm_scale = value.Get<GfVec2f>();
            texture.hasXfm = true;
        } else if (name == HdOSPRayMaterialTokens->rotation) {
            texture.xfm_rotation = value.Get<float>();
            texture.hasXfm = true;
        } else if (name == HdOSPRayMaterialTokens->translation) {
            texture.xfm_translation = value.Get<GfVec2f>();
            texture.hasXfm = true;
        }
    }
}

void
HdOSPRayMaterial::SetDisplayColor(GfVec4f color)
{
    diffuseColor = GfVec3f(color[0], color[1], color[2]);
    opacity = color[3];
    _UpdateOSPRayMaterial();
}

opp::Material
HdOSPRayMaterial::CreateDefaultMaterial(GfVec4f color)
{
    std::string rendererType = HdOSPRayConfig::GetInstance().usePathTracing
           ? "pathtracer"
           : "scivis";
    opp::Material ospMaterial;
    if (rendererType == "pathtracer"
        && !HdOSPRayConfig::GetInstance().useSimpleMaterial) {
        ospMaterial = opp::Material(rendererType.c_str(), "principled");
        ospMaterial.setParam("baseColor", vec3f(color[0], color[1], color[2]));
        ospMaterial.setParam("ior", 1.5f);
        ospMaterial.setParam("metallic", 0.0f);
        ospMaterial.setParam("roughness", 0.25f);
    } else {
        ospMaterial = opp::Material(rendererType.c_str(), "obj");
        ospMaterial.setParam("ns", 10.f);
        ospMaterial.setParam("ks", vec3f(0.2f, 0.2f, 0.2f));
        ospMaterial.setParam("kd", vec3f(color[0], color[1], color[2]));
        ospMaterial.setParam("d", color.data()[3]);
    }
    ospMaterial.commit();
    return ospMaterial;
}

opp::Material
HdOSPRayMaterial::CreatePrincipledMaterial(std::string rendererType)
{
    opp::Material ospMaterial
           = opp::Material(rendererType.c_str(), "principled");
    ospMaterial.setParam(
           "baseColor",
           vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2]));
    bool hasMetallicTex = false;
    bool hasRoughnessTex = false;
    bool hasOpacityTex = false;
    // set texture maps
    for (const auto& [key, value] : _textures) {
        if (!value.ospTexture)
            continue;
        std::string name = "";
        if (key == HdOSPRayMaterialTokens->diffuseColor)
            name = "map_baseColor";
        else if (key == HdOSPRayMaterialTokens->metallic) {
            name = "map_metallic";
            hasMetallicTex = true;
        } else if (key == HdOSPRayMaterialTokens->roughness) {
            name = "map_roughness";
            hasRoughnessTex = true;
        } else if (key == HdOSPRayMaterialTokens->opacity) {
            name = "map_transmission";
            hasOpacityTex = true;
        }

        if (name != "") {
            ospMaterial.setParam(name.c_str(), value.ospTexture);
            if (value.hasXfm) {
                std::string st_translation = name + ".translation";
                std::string st_scale = name + ".scale";
                std::string st_rotation = name + ".rotation";
                ospMaterial.setParam(st_translation.c_str(),
                                     vec2f(-value.xfm_translation[0],
                                           -value.xfm_translation[1]));
                ospMaterial.setParam(st_scale.c_str(),
                                     vec2f(1.0f / value.xfm_scale[0],
                                           1.0f / value.xfm_scale[1]));
                ospMaterial.setParam(st_rotation.c_str(), -value.xfm_rotation);
            }
        }
    }

    // set material params
    ospMaterial.setParam("metallic", (hasMetallicTex ? 1.0f : metallic));
    ospMaterial.setParam("roughness", (hasRoughnessTex ? 1.0f : roughness));
    ospMaterial.setParam("coat", coat);
    ospMaterial.setParam("coatRoughness", coatRoughness);
    ospMaterial.setParam("ior", ior);
    ospMaterial.setParam("transmission",
                         (hasOpacityTex ? 1.0f : 1.0f - opacity));
    ospMaterial.setParam("thin", true);
    ospMaterial.commit();

    return ospMaterial;
}

opp::Material
HdOSPRayMaterial::CreateSimpleMaterial(std::string rendererType)
{
    opp::Material ospMaterial = opp::Material(rendererType.c_str(), "obj");
    float avgFresnel = EvalAvgFresnel(ior);

    if (metallic == 0.f) {
        ospMaterial.setParam(
               "kd",
               vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2])
                      * (1.0f - avgFresnel));
        ospMaterial.setParam(
               "ks",
               vec3f(specularColor[0], specularColor[1], specularColor[2])
                      * avgFresnel);
        ospMaterial.setParam("ns",
                             RoughnesToPhongExponent(std::sqrt(roughness)));
    } else {
        ospMaterial.setParam("kd", vec3f(0.0f, 0.0f, 0.0f));
        ospMaterial.setParam(
               "ks",
               vec3f(specularColor[0], specularColor[1], specularColor[2]));
        ospMaterial.setParam("ns",
                             RoughnesToPhongExponent(std::sqrt(roughness)));
    }

    if (opacity < 1.0f) {
        float tf = 1.0f - opacity;
        ospMaterial.setParam("kd", vec3f(0.0f, 0.0f, 0.0f));
        ospMaterial.setParam(
               "tf",
               vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2]) * tf);
    }
    if (_textures.find(HdOSPRayMaterialTokens->diffuseColor)
        != _textures.end()) {
        opp::Texture ospMapDiffuseTex
               = _textures[HdOSPRayMaterialTokens->diffuseColor].ospTexture;
        if (ospMapDiffuseTex) {
            ospMaterial.setParam("map_kd", ospMapDiffuseTex);
            diffuseColor = GfVec3f(1.0);
        }
    }
    ospMaterial.commit();
    return ospMaterial;
}

opp::Material
HdOSPRayMaterial::CreateScivisMaterial(std::string rendererType)
{
    opp::Material ospMaterial = opp::Material(rendererType.c_str(), "obj");
    ospMaterial.setParam("ns", 10.f);
    ospMaterial.setParam("ks", vec3f(0.2f, 0.2f, 0.2f));
    ospMaterial.setParam(
           "kd", vec3f(diffuseColor[0], diffuseColor[1], diffuseColor[2]));
    if (_textures.find(HdOSPRayMaterialTokens->diffuseColor)
        != _textures.end()) {
        opp::Texture ospMapDiffuseTex
               = _textures[HdOSPRayMaterialTokens->diffuseColor].ospTexture;
        if (ospMapDiffuseTex) {
            ospMaterial.setParam("map_kd", ospMapDiffuseTex);
            diffuseColor = GfVec3f(1.0);
        }
    }
    ospMaterial.setParam("d", opacity);
    ospMaterial.commit();
    return ospMaterial;
}