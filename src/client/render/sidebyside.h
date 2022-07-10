/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

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
#include "stereo.h"

class DrawImageStep : public RenderStep
{
public:
	DrawImageStep(u8 texture_index, v2s32 *pos);

	void setRenderSource(RenderSource *_source) override;
	void setRenderTarget(RenderTarget *_target) override;

	void reset(PipelineContext *context) override {}
	void run(PipelineContext *context) override;
private:
	u8 texture_index;
	v2s32 *pos;
	RenderSource *source;
	RenderTarget *target;
};

class RenderingCoreSideBySide : public RenderingCoreStereo
{
protected:
	static const u8 TEXTURE_LEFT = 0;
	static const u8 TEXTURE_RIGHT = 1;

	bool horizontal = false;
	bool flipped = false;
	v2s32 rpos;

	void initTextures() override;
	void createPipeline() override;

public:
	RenderingCoreSideBySide(IrrlichtDevice *_device, Client *_client, Hud *_hud,
			bool _horizontal = false, bool _flipped = false);
};
