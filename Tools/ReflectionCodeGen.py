#!/usr/bin/env python3
"""
Helianthus Reflection Code Generator
生成类似UE的反射代码
"""

import os
import re
import sys
from typing import List, Dict, Tuple

class ReflectionParser:
    def __init__(self):
        self.classes = []
        self.current_class = None
        
    def parse_file(self, filepath: str) -> List[Dict]:
        """解析C++文件中的反射标记"""
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # 正则表达式匹配HCLASS, HPROPERTY, HFUNCTION
        class_pattern = r'HCLASS\(([^)]*)\)\s*class\s+(\w+)\s*:\s*public\s+(\w+)'
        property_pattern = r'HPROPERTY\(([^)]*)\)\s*([\w<>\s]+)\s+(\w+)(?:\s*=\s*([^;\n]+))?'
        function_pattern = r'HFUNCTION\(([^)]*)\)\s*([\w<>\s]+)\s+(\w+)\s*\(([^)]*)\)'
        
        classes = []
        
        # 查找类定义
        for class_match in re.finditer(class_pattern, content):
            flags, class_name, super_class = class_match.groups()
            
            class_info = {
                'name': class_name,
                'super_class': super_class,
                'flags': [f.strip() for f in flags.split(',')],
                'properties': [],
                'functions': []
            }
            
            # 查找类体内的属性和函数
            class_body_start = content.find('{', class_match.end())
            if class_body_start != -1:
                brace_count = 1
                class_body_end = class_body_start + 1
                while brace_count > 0 and class_body_end < len(content):
                    if content[class_body_end] == '{':
                        brace_count += 1
                    elif content[class_body_end] == '}':
                        brace_count -= 1
                    class_body_end += 1
                
                class_body = content[class_body_start:class_body_end]
                
                # 解析属性
                for prop_match in re.finditer(property_pattern, class_body):
                    prop_flags, prop_type, prop_name, default_value = prop_match.groups()
                    class_info['properties'].append({
                        'type': prop_type.strip(),
                        'name': prop_name,
                        'flags': [f.strip() for f in prop_flags.split(',')],
                        'default_value': default_value.strip() if default_value else ''
                    })
                
                # 解析函数
                for func_match in re.finditer(function_pattern, class_body):
                    func_flags, return_type, func_name, params = func_match.groups()
                    class_info['functions'].append({
                        'return_type': return_type.strip(),
                        'name': func_name,
                        'params': params.strip(),
                        'flags': [f.strip() for f in func_flags.split(',')]
                    })
            
            classes.append(class_info)
        
        return classes

class CodeGenerator:
    def __init__(self):
        self.parser = ReflectionParser()
    
    def generate_header(self, class_info: Dict, output_dir: str) -> str:
        """生成.GEN.h文件"""
        header_name = f"{class_info['name']}.GEN.h"
        header_path = os.path.join(output_dir, header_name)
        
        with open(header_path, 'w', encoding='utf-8') as f:
            f.write(self._generate_header_content(class_info))
        
        return header_path
    
    def _generate_header_content(self, class_info: Dict) -> str:
        """生成头文件内容"""
        content = f"""#pragma once

#include "{class_info['name']}.h"
#include "Shared/Reflection/HObject.h"
#include <cstddef>

namespace Helianthus::Reflection
{{
    template<>
    struct ClassInfo<{class_info['name']}>
    {{
        static constexpr const char* Name = "{class_info['name']}";
        static constexpr const char* SuperClassName = "{class_info['super_class']}";
        
        struct PropertyInfo
        {{
            const char* Name;
            const char* Type;
            size_t Offset;
            const char* Flags;
        }};
        
        struct FunctionInfo
        {{
            const char* Name;
            const char* ReturnType;
            const char* Flags;
        }};
        
        static constexpr PropertyInfo Properties[] = {{
"""
        
        # 生成属性信息
        for prop in class_info['properties']:
            flags_str = '|'.join(prop['flags']) if prop['flags'] else "None"
            content += f"            {{ \"{prop['name']}\", \"{prop['type']}\", offsetof({class_info['name']}, {prop['name']}), \"{flags_str}\" }},\n"
        
        if not class_info['properties']:
            content += "            { nullptr, nullptr, 0, nullptr }\n"
        
        content += f"        }};\n\n        static constexpr FunctionInfo Functions[] = {{\n"
        
        # 生成函数信息
        for func in class_info['functions']:
            flags_str = '|'.join(func['flags']) if func['flags'] else "None"
            content += f"            {{ \"{func['name']}\", \"{func['return_type']}\", \"{flags_str}\" }},\n"
        
        if not class_info['functions']:
            content += "            { nullptr, nullptr, nullptr }\n"
        
        content += f"        }};\n        \n        static constexpr size_t PropertyCount = {len(class_info['properties'])};\n        static constexpr size_t FunctionCount = {len(class_info['functions'])};\n    }};\n}}\n\n// 自动注册\nstatic struct {class_info['name']}Registration\n{{\n    {class_info['name']}Registration()\n    {{\n        Helianthus::Reflection::RegisterClass<{class_info['name']}>();\n    }}\n}} {class_info['name']}Reg;\n"
        
        return content
    
    def generate_build_file(self, classes: List[Dict], output_dir: str) -> str:
        """生成BUILD.bazel文件"""
        build_content = """load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

# 自动生成的反射代码
cc_library(\n    name = "reflection_gen",\n    hdrs = ["""
"""
        
        for class_info in classes:
            build_content += f"        \"{class_info['name']}.GEN.h\",\n"
        
        build_content += """    ],\n    deps = [\n        "//Shared/Reflection:reflection",\n    ],\n)\n"""
        
        build_path = os.path.join(output_dir, "BUILD.bazel")
        with open(build_path, 'w', encoding='utf-8') as f:
            f.write(build_content)
        
        return build_path

def main():
    if len(sys.argv) < 3:
        print("Usage: python ReflectionCodeGen.py <input_dir> <output_dir>")
        sys.exit(1)
    
    input_dir = sys.argv[1]
    output_dir = sys.argv[2]
    
    os.makedirs(output_dir, exist_ok=True)
    
    generator = CodeGenerator()
    all_classes = []
    
    # 扫描所有C++文件
    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file.endswith(('.h', '.hpp', '.cpp')):
                filepath = os.path.join(root, file)
                try:
                    classes = generator.parser.parse_file(filepath)
                    all_classes.extend(classes)
                    
                    # 为每个类生成.GEN.h文件
                    for class_info in classes:
                        header_path = generator.generate_header(class_info, output_dir)
                        print(f"Generated: {header_path}")
                        
                except Exception as e:
                    print(f"Error processing {filepath}: {e}")
    
    # 生成BUILD文件
    if all_classes:
        build_path = generator.generate_build_file(all_classes, output_dir)
        print(f"Generated: {build_path}")
    
    print(f"Code generation complete. Generated {len(all_classes)} classes.")

if __name__ == "__main__":
    main()