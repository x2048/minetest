core.register_on_joinplayer(function(player)
	if player.set_lighting then
		player:set_lighting({ brightness = 0.0 });
	end
end)

core.register_chatcommand("b", {
	params = "brightness",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		local brightness = tonumber(param)
		-- player:set_lighting({ brightness = brightness, color_tint = {r = 255, g = 255, b = 255 }})
		player:set_lighting({ brightness = brightness, color_tint = {r = 255, g = 179 * (1 + brightness), b = 128 * (1 + brightness)}})
	end
})
