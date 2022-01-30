core.register_on_joinplayer(function(player)
	if player.set_lighting then
		player:set_lighting({ brightness = 0.0 });
	end
end)

core.register_chatcommand("b", {
	params = "brightness",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		player:set_lighting({ brightness = tonumber(param) })
	end
})

core.register_chatcommand("t", {
	params = "color",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		player:set_lighting({ color_tint = param })
	end
})
