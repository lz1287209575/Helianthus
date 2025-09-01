-- Generated Lua binding for class: SimpleWeapon
local SimpleWeapon = {}
SimpleWeapon.__index = SimpleWeapon

function SimpleWeapon.new(...)
    local self = setmetatable({}, SimpleWeapon)
    -- TODO: Initialize properties
    return self
end

function SimpleWeapon:getName()
    return self.Name
end
function SimpleWeapon:setName(value)
    self.Name = value
end

function SimpleWeapon:getDamage()
    return self.Damage
end
function SimpleWeapon:setDamage(value)
    self.Damage = value
end

function SimpleWeapon:getType()
    return self.Type
end
function SimpleWeapon:setType(value)
    self.Type = value
end

function SimpleWeapon:Upgrade()
    -- TODO: Implement method call
    return nil
end

-- Metatable for SimpleWeapon
local mt = {
    __index = SimpleWeapon,
    __tostring = function(self)
        return "SimpleWeapon instance"
    end
}
setmetatable(SimpleWeapon, mt)

_G.SimpleWeapon = SimpleWeapon
return SimpleWeapon


-- Generated Lua binding for class: SimplePlayer
local SimplePlayer = {}
SimplePlayer.__index = SimplePlayer

function SimplePlayer.new(...)
    local self = setmetatable({}, SimplePlayer)
    -- TODO: Initialize properties
    return self
end

function SimplePlayer:getName()
    return self.Name
end
function SimplePlayer:setName(value)
    self.Name = value
end

function SimplePlayer:getHealth()
    return self.Health
end
function SimplePlayer:setHealth(value)
    self.Health = value
end

function SimplePlayer:getSpeed()
    return self.Speed
end
function SimplePlayer:setSpeed(value)
    self.Speed = value
end

function SimplePlayer:TakeDamage()
    -- TODO: Implement method call
    return nil
end

function SimplePlayer:Heal()
    -- TODO: Implement method call
    return nil
end

-- Metatable for SimplePlayer
local mt = {
    __index = SimplePlayer,
    __tostring = function(self)
        return "SimplePlayer instance"
    end
}
setmetatable(SimplePlayer, mt)

_G.SimplePlayer = SimplePlayer
return SimplePlayer


