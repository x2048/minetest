/*
Minetest
Copyright (C) 2021 Liso <anlismon@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once
#include "irrlichttypes_extrabloated.h"
#include <IMaterialRendererServices.h>
#include <IShaderConstantSetCallBack.h>
#include "client/shader.h"

class ShadowDepthShaderCB : public video::IShaderConstantSetCallBack
{
public:
	ShadowDepthShaderCB() :
			m_light_mvp_setting("LightMVP"),
			m_map_resolution_setting("MapResolution"),
			m_max_far_setting("MaxFar"),
			m_color_map_sampler_setting("ColorMapSampler"),
			m_perspective_zbias("zPerspectiveBias"),
			m_cascade_setting("Cascade")
	{}

	void OnSetMaterial(const video::SMaterial &material) override {}

	void OnSetConstants(video::IMaterialRendererServices *services,
			s32 userData) override;

	f32 MaxFar{2048.0f}, MapRes{1024.0f};
	f32 PerspectiveBiasZ {0.5f};
	s32 Cascade;

private:
	CachedVertexShaderSetting<f32, 16> m_light_mvp_setting;
	CachedVertexShaderSetting<f32> m_map_resolution_setting;
	CachedVertexShaderSetting<f32> m_max_far_setting;
	CachedPixelShaderSetting<s32> m_color_map_sampler_setting;
	CachedVertexShaderSetting<f32> m_perspective_zbias;
	CachedPixelShaderSetting<s32> m_cascade_setting;
};
