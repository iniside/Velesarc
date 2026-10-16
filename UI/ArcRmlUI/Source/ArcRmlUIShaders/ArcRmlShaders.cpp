// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRmlShaders.h"

IMPLEMENT_GLOBAL_SHADER(FArcRmlGeometryVS, "/Plugin/ArcRmlUI/ArcRmlUI.usf", "GeometryVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FArcRmlColorPS, "/Plugin/ArcRmlUI/ArcRmlUI.usf", "ColorPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArcRmlTexturePS, "/Plugin/ArcRmlUI/ArcRmlUI.usf", "TexturePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArcRmlGradientPS, "/Plugin/ArcRmlUI/ArcRmlUI.usf", "GradientPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArcRmlCopyPS, "/Plugin/ArcRmlUI/ArcRmlUI.usf", "CopyPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArcRmlColorMatrixPS, "/Plugin/ArcRmlUI/ArcRmlUI.usf", "ColorMatrixPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArcRmlBlendMaskPS, "/Plugin/ArcRmlUI/ArcRmlUI.usf", "BlendMaskPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArcRmlBlurPS, "/Plugin/ArcRmlUI/ArcRmlUI.usf", "BlurPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArcRmlDropShadowPS, "/Plugin/ArcRmlUI/ArcRmlUI.usf", "DropShadowPS", SF_Pixel);
