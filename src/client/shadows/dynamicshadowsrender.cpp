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

#include <cstring>
#include <cmath>
#include <irr_ptr.h>
#include "client/shadows/dynamicshadowsrender.h"
#include "client/shadows/shadowsScreenQuad.h"
#include "client/shadows/shadowsshadercallbacks.h"
#include "settings.h"
#include "filesys.h"
#include "util/string.h"
#include "client/shader.h"
#include "client/client.h"
#include "client/clientmap.h"
#include "profiler.h"

/*
 * Service class to integrate video::IShaderConstantSetCallBack with IWritableShaderSource.
 */
class ShaderConstantSetterToSetCallBack : public IShaderConstantSetter
{
public:
	ShaderConstantSetterToSetCallBack(video::IShaderConstantSetCallBack *cb) : m_cb(cb)
	{
	}

	virtual ~ShaderConstantSetterToSetCallBack() override
	{
	}

	virtual void onSetConstants(video::IMaterialRendererServices *services) override
	{
		m_cb->OnSetConstants(services, 0);
	}
private:
	video::IShaderConstantSetCallBack *m_cb;
};

/*
 * Service class to integrate video::IShaderConstantSetCallBack with IWritableShaderSource.
 */
class ShaderConstantSetterToSetCallBackFactory : public IShaderConstantSetterFactory
{
public:
	ShaderConstantSetterToSetCallBackFactory(video::IShaderConstantSetCallBack *cb) : m_cb(cb)
	{
	}

	virtual ~ShaderConstantSetterToSetCallBackFactory() override
	{
	}

	virtual IShaderConstantSetter* create() override
	{
		return new ShaderConstantSetterToSetCallBack(m_cb);
	}

private:
	video::IShaderConstantSetCallBack *m_cb;
};

ShadowRenderer::ShadowRenderer(IrrlichtDevice *device, Client *client) :
		m_smgr(device->getSceneManager()), m_driver(device->getVideoDriver()),
		m_client(client), m_current_frame(0),
		m_perspective_bias_xy(0.8), m_perspective_bias_z(0.5)
{
	(void) m_client;

	m_shadows_supported = true; // assume shadows supported. We will check actual support in initialize
	m_shadows_enabled = true;

	m_shadow_strength_gamma = g_settings->getFloat("shadow_strength_gamma");
	if (std::isnan(m_shadow_strength_gamma))
		m_shadow_strength_gamma = 1.0f;
	m_shadow_strength_gamma = core::clamp(m_shadow_strength_gamma, 0.1f, 10.0f);

	m_shadow_map_max_distance = g_settings->getFloat("shadow_map_max_distance");

	m_shadow_map_texture_size = g_settings->getFloat("shadow_map_texture_size");

	m_shadow_map_texture_32bit = g_settings->getBool("shadow_map_texture_32bit");
	m_shadow_map_colored = g_settings->getBool("shadow_map_color");
	m_shadow_samples = g_settings->getS32("shadow_filters");
	m_map_shadow_update_frames = g_settings->getS16("shadow_update_frames");
}

ShadowRenderer::~ShadowRenderer()
{
	// call to disable releases dynamically allocated resources
	disable();

	if (m_shadow_depth_cb)
		delete m_shadow_depth_cb;
	if (m_shadow_mix_cb)
		delete m_shadow_mix_cb;
	if (m_screen_quad)
		delete m_screen_quad;
	m_shadow_node_array.clear();
	m_light_list.clear();
}

void ShadowRenderer::disable()
{
	m_shadows_enabled = false;
	if (shadowMapTextureFinal) {
		m_driver->setRenderTarget(shadowMapTextureFinal, true, true,
			video::SColor(255, 255, 255, 255));
		m_driver->setRenderTarget(0, false, false);
	}

	if (shadowMapTextureDynamicObjects) {
		m_driver->removeTexture(shadowMapTextureDynamicObjects);
		shadowMapTextureDynamicObjects = nullptr;
	}

	if (shadowMapTextureFinal) {
		m_driver->removeTexture(shadowMapTextureFinal);
		shadowMapTextureFinal = nullptr;
	}

	if (shadowMapTextureColors) {
		m_driver->removeTexture(shadowMapTextureColors);
		shadowMapTextureColors = nullptr;
	}

	if (shadowMapClientMap) {
		m_driver->removeTexture(shadowMapClientMap);
		shadowMapClientMap = nullptr;
	}

	if (shadowMapClientMapFuture) {
		m_driver->removeTexture(shadowMapClientMapFuture);
		shadowMapClientMapFuture = nullptr;
	}

	for (auto node : m_shadow_node_array)
		if (node.shadowMode & E_SHADOW_MODE::ESM_RECEIVE)
			node.node->setMaterialTexture(TEXTURE_LAYER_SHADOW, nullptr);
}

void ShadowRenderer::initialize()
{
	auto *gpu = m_driver->getGPUProgrammingServices();

	// we need glsl
	if (m_shadows_supported && gpu && m_driver->queryFeature(video::EVDF_ARB_GLSL)) {
		createShaders();
	} else {
		m_shadows_supported = false;

		warningstream << "Shadows: GLSL Shader not supported on this system."
			<< std::endl;
		return;
	}

	m_texture_format = m_shadow_map_texture_32bit
					   ? video::ECOLOR_FORMAT::ECF_R32F
					   : video::ECOLOR_FORMAT::ECF_R16F;

	m_texture_format_color = m_shadow_map_texture_32bit
						 ? video::ECOLOR_FORMAT::ECF_G32R32F
						 : video::ECOLOR_FORMAT::ECF_G16R16F;
	
	m_shadows_enabled &= m_shadows_supported;
}


size_t ShadowRenderer::addDirectionalLight()
{
	m_light_list.emplace_back(m_shadow_map_texture_size,
			v3f(0.f, 0.f, 0.f),
			video::SColor(255, 255, 255, 255), m_shadow_map_max_distance);
	return m_light_list.size() - 1;
}

DirectionalLight &ShadowRenderer::getDirectionalLight(u32 index)
{
	return m_light_list[index];
}

size_t ShadowRenderer::getDirectionalLightCount() const
{
	return m_light_list.size();
}

f32 ShadowRenderer::getMaxShadowFar() const
{
	if (!m_light_list.empty()) {
		float zMax = m_light_list[0].getFarValue();
		return zMax;
	}
	return 0.0f;
}

void ShadowRenderer::setShadowIntensity(float shadow_intensity)
{
	m_shadow_strength = pow(shadow_intensity, 1.0f / m_shadow_strength_gamma);
	if (m_shadow_strength > 1E-2)
		enable();
	else
		disable();
}

void ShadowRenderer::addNodeToShadowList(
		scene::ISceneNode *node, E_SHADOW_MODE shadowMode)
{
	if (!node)
		return;
	m_shadow_node_array.emplace_back(node, shadowMode);
	if (shadowMode == ESM_RECEIVE || shadowMode == ESM_BOTH)
		node->setMaterialTexture(TEXTURE_LAYER_SHADOW, shadowMapTextureFinal);
}

void ShadowRenderer::removeNodeFromShadowList(scene::ISceneNode *node)
{
	if (!node)
		return;
	node->setMaterialTexture(TEXTURE_LAYER_SHADOW, nullptr);
	for (auto it = m_shadow_node_array.begin(); it != m_shadow_node_array.end();) {
		if (it->node == node) {
			it = m_shadow_node_array.erase(it);
			break;
		} else {
			++it;
		}
	}
}

void ShadowRenderer::updateSMTextures()
{
	if (!m_shadows_enabled || m_smgr->getActiveCamera() == nullptr) {
		return;
	}

	if (!shadowMapTextureDynamicObjects) {

		shadowMapTextureDynamicObjects = getSMTexture(
			std::string("shadow_dynamic_") + itos(m_shadow_map_texture_size),
			m_texture_format, true);
		assert(shadowMapTextureDynamicObjects != nullptr);
	}

	if (!shadowMapClientMap) {

		shadowMapClientMap = getSMTexture(
			std::string("shadow_clientmap_") + itos(m_shadow_map_texture_size),
			m_shadow_map_colored ? m_texture_format_color : m_texture_format,
			true);
		assert(shadowMapClientMap != nullptr);
	}

	if (!shadowMapClientMapFuture && m_map_shadow_update_frames > 1) {
		shadowMapClientMapFuture = getSMTexture(
			std::string("shadow_clientmap_bb_") + itos(m_shadow_map_texture_size),
			m_shadow_map_colored ? m_texture_format_color : m_texture_format,
			true);
		assert(shadowMapClientMapFuture != nullptr);
	}

	if (m_shadow_map_colored && !shadowMapTextureColors) {
		shadowMapTextureColors = getSMTexture(
			std::string("shadow_colored_") + itos(m_shadow_map_texture_size),
			m_shadow_map_colored ? m_texture_format_color : m_texture_format,
			true);
		assert(shadowMapTextureColors != nullptr);
	}

	// The merge all shadowmaps texture
	if (!shadowMapTextureFinal) {
		video::ECOLOR_FORMAT frt;
		if (m_shadow_map_texture_32bit) {
			if (m_shadow_map_colored)
				frt = video::ECOLOR_FORMAT::ECF_A32B32G32R32F;
			else
				frt = video::ECOLOR_FORMAT::ECF_R32F;
		} else {
			if (m_shadow_map_colored)
				frt = video::ECOLOR_FORMAT::ECF_A16B16G16R16F;
			else
				frt = video::ECOLOR_FORMAT::ECF_R16F;
		}
		shadowMapTextureFinal = getSMTexture(
			std::string("shadowmap_final_") + itos(m_shadow_map_texture_size),
			frt, true);
		assert(shadowMapTextureFinal != nullptr);

		for (auto &node : m_shadow_node_array)
			if (node.shadowMode == ESM_RECEIVE || node.shadowMode == ESM_BOTH)
				node.node->setMaterialTexture(TEXTURE_LAYER_SHADOW, shadowMapTextureFinal);
	}

	if (!m_shadow_node_array.empty() && !m_light_list.empty()) {
		bool reset_sm_texture = false;

		// detect if SM should be regenerated
		for (DirectionalLight &light : m_light_list) {
			if (light.should_update_map_shadow || m_force_update_shadow_map) {
				light.should_update_map_shadow = false;
				m_current_frame = 0;
				reset_sm_texture = true;
			}
		}

		video::ITexture* shadowMapTargetTexture = shadowMapClientMapFuture;
		if (shadowMapTargetTexture == nullptr)
			shadowMapTargetTexture = shadowMapClientMap;

		// Update SM incrementally:
		for (DirectionalLight &light : m_light_list) {
			// Static shader values.
			m_shadow_depth_cb->MapRes = (f32)m_shadow_map_texture_size;
			m_shadow_depth_cb->MaxFar = (f32)m_shadow_map_max_distance * BS;
			m_shadow_depth_cb->PerspectiveBiasXY = getPerspectiveBiasXY();
			m_shadow_depth_cb->PerspectiveBiasZ = getPerspectiveBiasZ();
			m_shadow_depth_cb->CameraPos = light.getFuturePlayerPos();
			
			// set the Render Target
			// right now we can only render in usual RTT, not
			// Depth texture is available in irrlicth maybe we
			// should put some gl* fn here


			if (m_current_frame < m_map_shadow_update_frames || m_force_update_shadow_map) {
				m_driver->setRenderTarget(shadowMapTargetTexture, reset_sm_texture, true,
						video::SColor(255, 255, 255, 255));
				renderShadowMap(shadowMapTargetTexture, light);

				// Render transparent part in one pass.
				// This is also handled in ClientMap.
				if (m_current_frame == m_map_shadow_update_frames - 1 || m_force_update_shadow_map) {
					if (m_shadow_map_colored) {
						m_driver->setRenderTarget(0, false, false);
						m_driver->setRenderTarget(shadowMapTextureColors,
								true, false, video::SColor(255, 255, 255, 255));
					}
					renderShadowMap(shadowMapTextureColors, light,
							scene::ESNRP_TRANSPARENT);
				}
				m_driver->setRenderTarget(0, false, false);
			}

			reset_sm_texture = false;
		} // end for lights

		// move to the next section
		if (m_current_frame <= m_map_shadow_update_frames)
			++m_current_frame;

		// pass finished, swap textures and commit light changes
		if (m_current_frame == m_map_shadow_update_frames || m_force_update_shadow_map) {
			if (shadowMapClientMapFuture != nullptr)
				std::swap(shadowMapClientMapFuture, shadowMapClientMap);

			// Let all lights know that maps are updated
			for (DirectionalLight &light : m_light_list)
				light.commitFrustum();
		}
		m_force_update_shadow_map = false;
	}
}

void ShadowRenderer::update(video::ITexture *outputTarget)
{
	if (!m_shadows_enabled || m_smgr->getActiveCamera() == nullptr) {
		return;
	}

	updateSMTextures();

	if (shadowMapTextureFinal == nullptr) {
		return;
	}


	if (!m_shadow_node_array.empty() && !m_light_list.empty()) {

		for (DirectionalLight &light : m_light_list) {
			// Static shader values for entities are set in updateSMTextures
			// SM texture for entities is not updated incrementally and 
			// must by updated using current player position.
			m_shadow_depth_cb->CameraPos = light.getPlayerPos();

			// render shadows for the n0n-map objects.
			m_driver->setRenderTarget(shadowMapTextureDynamicObjects, true,
					true, video::SColor(255, 255, 255, 255));
			renderShadowObjects(shadowMapTextureDynamicObjects, light);
			// clear the Render Target
			m_driver->setRenderTarget(0, false, false);

			// in order to avoid too many map shadow renders,
			// we should make a second pass to mix clientmap shadows and
			// entities shadows :(
			m_screen_quad->getMaterial().setTexture(0, shadowMapClientMap);
			// dynamic objs shadow texture.
			if (m_shadow_map_colored)
				m_screen_quad->getMaterial().setTexture(1, shadowMapTextureColors);
			m_screen_quad->getMaterial().setTexture(2, shadowMapTextureDynamicObjects);

			m_driver->setRenderTarget(shadowMapTextureFinal, false, false,
					video::SColor(255, 255, 255, 255));
			m_screen_quad->render(m_driver);
			m_driver->setRenderTarget(0, false, false);

		} // end for lights
	}
}

void ShadowRenderer::drawDebug()
{
	/* this code just shows shadows textures in screen and in ONLY for debugging*/
	#if 0
	// this is debug, ignore for now.
	if (shadowMapTextureFinal)
		m_driver->draw2DImage(shadowMapTextureFinal,
				core::rect<s32>(0, 50, 128, 128 + 50),
				core::rect<s32>({0, 0}, shadowMapTextureFinal->getSize()));

	if (shadowMapClientMap)
		m_driver->draw2DImage(shadowMapClientMap,
				core::rect<s32>(0, 50 + 128, 128, 128 + 50 + 128),
				core::rect<s32>({0, 0}, shadowMapTextureFinal->getSize()));
	
	if (shadowMapTextureDynamicObjects)
		m_driver->draw2DImage(shadowMapTextureDynamicObjects,
				core::rect<s32>(0, 128 + 50 + 128, 128,
						128 + 50 + 128 + 128),
				core::rect<s32>({0, 0}, shadowMapTextureDynamicObjects->getSize()));

	if (m_shadow_map_colored && shadowMapTextureColors) {

		m_driver->draw2DImage(shadowMapTextureColors,
				core::rect<s32>(128,128 + 50 + 128 + 128,
						128 + 128, 128 + 50 + 128 + 128 + 128),
				core::rect<s32>({0, 0}, shadowMapTextureColors->getSize()));
	}
	#endif
}


video::ITexture *ShadowRenderer::getSMTexture(const std::string &shadow_map_name,
		video::ECOLOR_FORMAT texture_format, bool force_creation)
{
	if (force_creation) {
		return m_driver->addRenderTargetTexture(
				core::dimension2du(m_shadow_map_texture_size,
						m_shadow_map_texture_size),
				shadow_map_name.c_str(), texture_format);
	}

	return m_driver->getTexture(shadow_map_name.c_str());
}

void ShadowRenderer::renderShadowMap(video::ITexture *target,
		DirectionalLight &light, scene::E_SCENE_NODE_RENDER_PASS pass)
{
	m_driver->setTransform(video::ETS_VIEW, light.getFutureViewMatrix());
	m_driver->setTransform(video::ETS_PROJECTION, light.getFutureProjectionMatrix());

	// Operate on the client map
	for (const auto &shadow_node : m_shadow_node_array) {
		if (strcmp(shadow_node.node->getName(), "ClientMap") != 0)
			continue;

		ClientMap *map_node = static_cast<ClientMap *>(shadow_node.node);

		video::SMaterial material;
		if (map_node->getMaterialCount() > 0) {
			// we only want the first material, which is the one with the albedo info
			material = map_node->getMaterial(0);
		}

		material.BackfaceCulling = false;
		material.FrontfaceCulling = true;

		if (m_shadow_map_colored && pass != scene::ESNRP_SOLID) {
			material.MaterialType = (video::E_MATERIAL_TYPE) depth_shader_trans;
		}
		else {
			material.MaterialType = (video::E_MATERIAL_TYPE) depth_shader;
			material.BlendOperation = video::EBO_MIN;
		}

		m_driver->setTransform(video::ETS_WORLD,
				map_node->getAbsoluteTransformation());

		int frame = m_force_update_shadow_map ? 0 : m_current_frame;
		int total_frames = m_force_update_shadow_map ? 1 : m_map_shadow_update_frames;

		map_node->renderMapShadows(m_driver, material, pass, frame, total_frames);
		break;
	}
}

void ShadowRenderer::renderShadowObjects(
		video::ITexture *target, DirectionalLight &light)
{
	m_driver->setTransform(video::ETS_VIEW, light.getViewMatrix());
	m_driver->setTransform(video::ETS_PROJECTION, light.getProjectionMatrix());

	for (const auto &shadow_node : m_shadow_node_array) {
		// we only take care of the shadow casters
		if (shadow_node.shadowMode == ESM_RECEIVE ||
				strcmp(shadow_node.node->getName(), "ClientMap") == 0)
			continue;

		// render other objects
		u32 n_node_materials = shadow_node.node->getMaterialCount();
		std::vector<s32> BufferMaterialList;
		std::vector<std::pair<bool, bool>> BufferMaterialCullingList;
		std::vector<video::E_BLEND_OPERATION> BufferBlendOperationList;
		BufferMaterialList.reserve(n_node_materials);
		BufferMaterialCullingList.reserve(n_node_materials);
		BufferBlendOperationList.reserve(n_node_materials);

		// backup materialtype for each material
		// (aka shader)
		// and replace it by our "depth" shader
		for (u32 m = 0; m < n_node_materials; m++) {
			auto &current_mat = shadow_node.node->getMaterial(m);

			BufferMaterialList.push_back(current_mat.MaterialType);
			current_mat.MaterialType =
					(video::E_MATERIAL_TYPE)depth_shader_entities;

			BufferMaterialCullingList.emplace_back(
				(bool)current_mat.BackfaceCulling, (bool)current_mat.FrontfaceCulling);
			BufferBlendOperationList.push_back(current_mat.BlendOperation);

			current_mat.BackfaceCulling = true;
			current_mat.FrontfaceCulling = false;
		}

		m_driver->setTransform(video::ETS_WORLD,
				shadow_node.node->getAbsoluteTransformation());
		shadow_node.node->render();

		// restore the material.

		for (u32 m = 0; m < n_node_materials; m++) {
			auto &current_mat = shadow_node.node->getMaterial(m);

			current_mat.MaterialType = (video::E_MATERIAL_TYPE) BufferMaterialList[m];

			current_mat.BackfaceCulling = BufferMaterialCullingList[m].first;
			current_mat.FrontfaceCulling = BufferMaterialCullingList[m].second;
			current_mat.BlendOperation = BufferBlendOperationList[m];
		}

	} // end for caster shadow nodes
}

void ShadowRenderer::mixShadowsQuad()
{
}

/*
 * @Liso's disclaimer ;) This function loads the Shadow Mapping Shaders.
 * I used a custom loader because I couldn't figure out how to use the base
 * Shaders system with custom IShaderConstantSetCallBack without messing up the
 * code too much. If anyone knows how to integrate this with the standard MT
 * shaders, please feel free to change it.
 */

void ShadowRenderer::createShaders()
{
	IWritableShaderSource *shaders = m_client->getShaderSource();


	if (depth_shader == -1) {
		m_shadow_depth_cb = new ShadowDepthShaderCB();
		shaders->addShaderConstantSetterFactory(new ShaderConstantSetterToSetCallBackFactory(m_shadow_depth_cb));
		u32 info_id = shaders->getShader("shadows_pass1_shader", TILE_MATERIAL_MAP_SHADOW, NDT_NORMAL);
		depth_shader = shaders->getShaderInfo(info_id).material;

		if (depth_shader == -1) {
			// upsi, something went wrong loading shader.
			m_shadows_enabled = false;
			m_shadows_supported = false;
			errorstream << "Error compiling shadow mapping shader." << std::endl;
			return;
		}
	}

	// This creates a clone of depth_shader with base material set to EMT_SOLID,
	// because entities won't render shadows with base material EMP_ONETEXTURE_BLEND
	if (depth_shader_entities == -1) {
		u32 info_id = shaders->getShader("shadows_pass1_shader", TILE_MATERIAL_BASIC, NDT_NORMAL);
		depth_shader_entities = shaders->getShaderInfo(info_id).material;

		if (depth_shader_entities == -1) {
			// upsi, something went wrong loading shader.
			m_shadows_enabled = false;
			m_shadows_supported = false;
			errorstream << "Error compiling shadow mapping shader (dynamic)." << std::endl;
			return;
		}
	}

	if (mixcsm_shader == -1) {

		m_shadow_mix_cb = new shadowScreenQuadCB();
		shaders->addShaderConstantSetterFactory(new ShaderConstantSetterToSetCallBackFactory(m_shadow_mix_cb));
		m_screen_quad = new shadowScreenQuad();

		u32 info_id = shaders->getShader("shadows_pass2_shader", TILE_MATERIAL_BASIC, NDT_NORMAL);
		mixcsm_shader = shaders->getShaderInfo(info_id).material;

		if (mixcsm_shader == -1) {
			// upsi, something went wrong loading shader.
			m_shadows_enabled = false;
			m_shadows_supported = false;
			errorstream << "Error compiling cascade shadow mapping shader." << std::endl;
			return;
		}

		m_screen_quad->getMaterial().MaterialType =
				(video::E_MATERIAL_TYPE)mixcsm_shader;
	}

	if (m_shadow_map_colored && depth_shader_trans == -1) {
		u32 info_id = shaders->getShader("shadows_pass1_colored_shader", TILE_MATERIAL_ALPHA, NDT_NORMAL);
		depth_shader_trans = shaders->getShaderInfo(info_id).material;

		if (depth_shader_trans == -1) {
			// upsi, something went wrong loading shader.
			m_shadow_map_colored = false;
			m_shadows_supported = false;
			errorstream << "Error compiling colored shadow mapping shader." << std::endl;
			return;
		}
	}
}
