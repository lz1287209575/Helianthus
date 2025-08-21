-- 游戏逻辑脚本示例
-- 这个脚本可以在运行时被热更新

local GameLogic = {}

-- 游戏状态
GameLogic.PlayerCount = 0
GameLogic.GameTime = 0
GameLogic.IsRunning = false

-- 初始化游戏逻辑
function GameLogic.Initialize()
    print("GameLogic: Initializing...")
    GameLogic.PlayerCount = 0
    GameLogic.GameTime = 0
    GameLogic.IsRunning = true
    print("GameLogic: Initialized successfully")
end

-- 更新游戏逻辑
function GameLogic.Update(DeltaTime)
    if not GameLogic.IsRunning then
        return
    end
    
    GameLogic.GameTime = GameLogic.GameTime + DeltaTime
    
    -- 每5秒输出一次游戏状态
    if math.floor(GameLogic.GameTime) % 5 == 0 then
        print(string.format("GameLogic: Game running for %.1f seconds, Players: %d", 
                           GameLogic.GameTime, GameLogic.PlayerCount))
    end
end

-- 添加玩家
function GameLogic.AddPlayer(PlayerName)
    GameLogic.PlayerCount = GameLogic.PlayerCount + 1
    print(string.format("GameLogic: Player '%s' joined. Total players: %d", 
                       PlayerName, GameLogic.PlayerCount))
    return GameLogic.PlayerCount
end

-- 移除玩家
function GameLogic.RemovePlayer(PlayerName)
    if GameLogic.PlayerCount > 0 then
        GameLogic.PlayerCount = GameLogic.PlayerCount - 1
        print(string.format("GameLogic: Player '%s' left. Total players: %d", 
                           PlayerName, GameLogic.PlayerCount))
    end
    return GameLogic.PlayerCount
end

-- 获取游戏状态
function GameLogic.GetStatus()
    return {
        PlayerCount = GameLogic.PlayerCount,
        GameTime = GameLogic.GameTime,
        IsRunning = GameLogic.IsRunning
    }
end

-- 停止游戏
function GameLogic.Stop()
    print("GameLogic: Stopping game...")
    GameLogic.IsRunning = false
end

-- 重新开始游戏
function GameLogic.Restart()
    print("GameLogic: Restarting game...")
    GameLogic.Initialize()
end

-- 导出到全局
_G.GameLogic = GameLogic

print("GameLogic: Script loaded successfully!")

-- 返回模块
return GameLogic
