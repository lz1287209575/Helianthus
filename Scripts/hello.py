# Python脚本示例
print("Hello from Python!")

# 定义一个简单的函数
def greet(name):
    return f"Hello, {name} from Python!"

# 定义一个计算函数
def calculate(a, b, operation):
    if operation == "add":
        return a + b
    elif operation == "subtract":
        return a - b
    elif operation == "multiply":
        return a * b
    elif operation == "divide":
        if b != 0:
            return a / b
        else:
            raise ValueError("Division by zero")
    else:
        raise ValueError(f"Unknown operation: {operation}")

# 定义一个简单的类
class Calculator:
    def __init__(self):
        self.history = []
    
    def add(self, a, b):
        result = a + b
        self.history.append(f"{a} + {b} = {result}")
        return result
    
    def subtract(self, a, b):
        result = a - b
        self.history.append(f"{a} - {b} = {result}")
        return result
    
    def multiply(self, a, b):
        result = a * b
        self.history.append(f"{a} * {b} = {result}")
        return result
    
    def divide(self, a, b):
        if b == 0:
            raise ValueError("Division by zero")
        result = a / b
        self.history.append(f"{a} / {b} = {result}")
        return result
    
    def get_history(self):
        return self.history
    
    def clear_history(self):
        self.history.clear()

# 定义一个游戏相关的类
class Player:
    def __init__(self, name, health=100):
        self.name = name
        self.health = health
        self.level = 1
        self.experience = 0
    
    def take_damage(self, damage):
        self.health = max(0, self.health - damage)
        return self.health
    
    def heal(self, amount):
        self.health = min(100, self.health + amount)
        return self.health
    
    def gain_experience(self, exp):
        self.experience += exp
        # 简单的升级逻辑
        if self.experience >= self.level * 100:
            self.level += 1
            self.experience = 0
            return True
        return False
    
    def is_alive(self):
        return self.health > 0
    
    def get_status(self):
        return {
            "name": self.name,
            "health": self.health,
            "level": self.level,
            "experience": self.experience,
            "alive": self.is_alive()
        }

# 测试函数
def test_calculator():
    calc = Calculator()
    print("Testing Calculator:")
    print(f"5 + 3 = {calc.add(5, 3)}")
    print(f"10 - 4 = {calc.subtract(10, 4)}")
    print(f"6 * 7 = {calc.multiply(6, 7)}")
    print(f"15 / 3 = {calc.divide(15, 3)}")
    print("History:", calc.get_history())

def test_player():
    player = Player("Alice")
    print("Testing Player:")
    print(f"Player: {player.name}")
    print(f"Initial health: {player.health}")
    
    player.take_damage(20)
    print(f"After taking 20 damage: {player.health}")
    
    player.heal(10)
    print(f"After healing 10: {player.health}")
    
    player.gain_experience(150)
    print(f"After gaining 150 exp: Level {player.level}")
    
    print("Status:", player.get_status())

# 主函数
if __name__ == "__main__":
    print("Python script loaded successfully!")
    test_calculator()
    test_player()
