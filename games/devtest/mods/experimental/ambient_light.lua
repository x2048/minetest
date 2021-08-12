core.register_chatcommand("set_ambient_light", {
    params = "<brightness> [color]",
    description = "Set ambient light parameters. Brightness is a number from 0 to 1, color is an #RRGGBB color",
    func = function(player_name, param)
        local brightness, color_tint = string.match(param, "([^ ]+)(.*)$");
        if not brightness then
            return false, "Invalid input, see /help set_ambient_light"
        end
        color_tint = string.gsub(color_tint, " ", "")
        if color_tint == "" then color_tint = nil end
        minetest.get_player_by_name(player_name):set_ambient_light({brightness=tonumber(brightness), color_tint = color_tint})
    end
})