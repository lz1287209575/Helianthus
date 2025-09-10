#!/usr/bin/env python3
import os
import re
import sys
import json
import hashlib

def scan_sources(root):
    sources = []
    for base, _, files in os.walk(root):
        for f in files:
            if f.endswith(('.h', '.hpp', '.cpp', '.cc', '.cxx')):
                sources.append(os.path.join(base, f))
    return sources

CLASS_RE = re.compile(r'\bHCLASS\s*\(([^)]*)\)\s*class\s+(\w+)|class\s+(\w+)[^{]*\{[^}]*HCLASS\s*\(([^)]*)\)')
RPC_FACTORY_RE = re.compile(r'\bHRPC_FACTORY\s*\(\s*\)')
PROP_RE  = re.compile(r'\bHPROPERTY\s*\(([^)]*)\)\s*[^;]+?\b(\w+)\s*;')
METH_RE  = re.compile(r'\bHMETHOD\s*\(([^)]*)\)\s*[^;{]*?\b(\w+)\s*\(')
METH_SIG_RE = re.compile(r'\bHMETHOD\s*\(([^)]*)\)\s*([^;{]*?)\b(\w+)\s*\(([^;{)]*)\)')
STATIC_FUNC_RE = re.compile(r'\bHFUNCTION\s*\(\s*\)\s*static\s+[^;{]*?\b(\w+)\s*\(')

def parse_file(path):
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        text = f.read()
    classes = []
    for m in CLASS_RE.finditer(text):
        if m.group(1) and m.group(2):  # HCLASS before class
            class_tags = m.group(1).strip()
            class_name = m.group(2).strip()
        elif m.group(3) and m.group(4):  # HCLASS inside class
            class_name = m.group(3).strip()
            class_tags = m.group(4).strip()
        else:
            continue
        has_rpc_factory = bool(RPC_FACTORY_RE.search(text))
        classes.append((class_name, class_tags, text, has_rpc_factory))
    return classes

_QUALIFIERS = {
    'inline', 'virtual', 'constexpr', 'consteval', 'constinit', 'static', 'friend',
    'volatile', 'noexcept', 'override', 'final'
}


def _clean_return_type(prefix: str) -> str:
    p = prefix.strip()
    # remove trailing qualifiers like 'const'
    p = p.replace('&', ' ').replace('*', ' ')
    tokens = [t for t in p.split() if t and t not in _QUALIFIERS]
    if not tokens:
        return ''
    # best effort: keep last token; but if template present, keep whole joined
    return ' '.join(tokens)


def extract_members(text):
    props = PROP_RE.findall(text)
    meths = METH_RE.findall(text)
    sfuncs = STATIC_FUNC_RE.findall(text)
    # 提取带签名的方法，用于参数名与返回类型
    param_map = {}
    return_map = {}
    for m in METH_SIG_RE.finditer(text):
        tag = (m.group(1) or '').strip()
        method_name = (m.group(3) or '').strip()
        params_raw = (m.group(4) or '').strip()
        if not method_name:
            continue
        # 返回类型在 group(2)
        ret_prefix = (m.group(2) or '').strip()
        return_map[(tag, method_name)] = _clean_return_type(ret_prefix)
        param_names = []
        if params_raw:
            for part in split_params(params_raw):
                name = extract_param_name(part)
                if name:
                    param_names.append(name)
        param_map[(tag, method_name)] = param_names
    return props, meths, sfuncs, param_map, return_map

def split_params(param_str: str):
    parts = []
    buf = []
    depth_angle = 0
    depth_paren = 0
    i = 0
    while i < len(param_str):
        ch = param_str[i]
        if ch == '<':
            depth_angle += 1
        elif ch == '>':
            if depth_angle > 0:
                depth_angle -= 1
        elif ch == '(':
            depth_paren += 1
        elif ch == ')':
            if depth_paren > 0:
                depth_paren -= 1
        elif ch == ',' and depth_angle == 0 and depth_paren == 0:
            parts.append(''.join(buf).strip())
            buf = []
            i += 1
            continue
        buf.append(ch)
        i += 1
    if buf:
        parts.append(''.join(buf).strip())
    return [p for p in parts if p]

_IDENT_RE = re.compile(r'[A-Za-z_][A-Za-z0-9_]*$')

def extract_param_name(param: str):
    # Trim default value
    if '=' in param:
        param = param.split('=')[0].strip()
    # Remove attributes and qualifiers around
    param = param.replace('&&', ' ').replace('&', ' ').replace('*', ' ')
    param = param.replace('[', ' ').replace(']', ' ')
    tokens = [t for t in param.strip().split() if t]
    if not tokens:
        return None
    # last token heuristically is the name when declaration includes a name
    cand = tokens[-1]
    if _IDENT_RE.match(cand):
        return cand
    # If looks like only type (no name provided), skip
    return None

def main():
    if len(sys.argv) < 3:
        print('usage: reflection_codegen.py <source_root> <out_dir>')
        sys.exit(1)
    root = sys.argv[1]
    out_dir = sys.argv[2]

    # 收集所有类信息
    all_classes = []
    includes = set()
    cache_path = os.path.join(out_dir, '.refl_cache.json')
    try:
        with open(cache_path, 'r', encoding='utf-8') as f:
            cache = json.load(f)
    except Exception:
        cache = {}
    
    for src in scan_sources(root):
        try:
            classes = parse_file(src)
        except Exception:
            continue
        for class_name, class_tags, text, has_rpc_factory in classes:
            # Sanitize tags to avoid quotes and exotic content
            clean = class_tags.replace('"', '\"')
            tags = [t.strip() for t in clean.split(',') if t.strip() and ' ' not in t]
            props, meths, sfuncs, param_map, return_map = extract_members(text)
            header_mode = os.path.splitext(src)[1] in ('.h', '.hpp', '.hh', '.hxx')
            
            if src not in includes:
                rel = os.path.relpath(src, root)
                if header_mode:
                    includes.add(rel)
            
            all_classes.append((class_name, tags, props, meths, sfuncs, has_rpc_factory, header_mode, param_map, return_map))
    
    # 生成头文件
    generate_header_file(out_dir, includes)
    
    # 生成每类的片段文件，并生成汇总文件（带增量）
    classes_dir = os.path.join(out_dir, 'classes')
    os.makedirs(classes_dir, exist_ok=True)
    class_hash_map = generate_per_class_files(classes_dir, all_classes, cache)
    
    # 生成类注册文件（通过 include 每类片段）
    generate_registrations_file(out_dir, all_classes)
    
    # 生成RPC服务文件（通过 include 每类片段）
    generate_services_file(out_dir, all_classes)
    
    # 生成自动挂载文件
    generate_auto_mount_file(out_dir)
    
    # 生成初始化文件
    generate_init_file(out_dir)
    
    # 生成聚合编译文件
    generate_classes_compilation_file(out_dir, all_classes)
    # 写回缓存
    try:
        with open(cache_path, 'w', encoding='utf-8') as f:
            json.dump(class_hash_map, f, ensure_ascii=False, indent=2, sort_keys=True)
    except Exception:
        pass

def generate_header_file(out_dir, includes):
    """生成头文件"""
    gen = []
    gen.append('#pragma once')
    gen.append('')
    gen.append('#include "Shared/Reflection/ReflectionCore.h"')
    gen.append('#include "Shared/RPC/RpcReflection.h"')
    gen.append('#include "Shared/RPC/IRpcServer.h"')
    gen.append('#include "Shared/Common/LogCategories.h"')
    gen.append('#include <cstddef>')
    gen.append('#include <fmt/format.h>')
    gen.append('#include <atomic>')
    gen.append('')
    gen.append('using namespace Helianthus::Reflection;')
    gen.append('')
    gen.append('// 全局标志，确保静态初始化只在程序启动时执行一次')
    gen.append('extern std::atomic<bool> g_ReflectionInitialized;')
    gen.append('')
    gen.append('// 包含所有反射类的头文件')
    for include in sorted(includes):
        gen.append(f'#include "{include}"')
    gen.append('')
    gen.append('// 声明所有注册函数')
    gen.append('namespace Helianthus { namespace Reflection {')
    gen.append('    void RegisterAllClasses();')
    gen.append('    void RegisterAllRpcServices();')
    gen.append('}}')
    gen.append('')
    gen.append('// 用于强制链接初始化单元的全局符号声明')
    gen.append('extern int g_ReflectionAutoInit;')
    gen.append('')
    gen.append('// 声明自动挂载函数')
    gen.append('namespace Helianthus { namespace RPC {')
    gen.append('    void RegisterReflectedServices(IRpcServer& Server);')
    gen.append('    void RegisterReflectedServices(IRpcServer& Server, const std::vector<std::string>& RequiredTags);')
    gen.append('    extern int g_ReflectionAutoInit;')
    gen.append('}}')
    
    os.makedirs(out_dir, exist_ok=True)
    _write_if_changed(os.path.join(out_dir, 'reflection_gen.h'), '\n'.join(gen))

def generate_registrations_file(out_dir, all_classes):
    """生成类注册文件"""
    gen = []
    gen.append('#include "reflection_gen.h"')
    gen.append('')
    gen.append('// 全局标志定义')
    gen.append('std::atomic<bool> g_ReflectionInitialized{false};')
    gen.append('')
    gen.append('namespace Helianthus { namespace Reflection {')
    # 前置声明每类注册函数
    for class_name, *_ in all_classes:
        gen.append(f'void RegisterClass_{class_name}();')
    gen.append('')
    gen.append('void RegisterAllClasses()')
    gen.append('{')
    gen.append('    // 检查是否已经初始化，避免在程序退出时重复执行')
    gen.append('    if (g_ReflectionInitialized.load(std::memory_order_acquire)) {')
    gen.append('        return;')
    gen.append('    }')
    gen.append('    g_ReflectionInitialized.store(true, std::memory_order_release);')
    gen.append('')
    # 逐类调用
    for class_name, *_ in all_classes:
        gen.append(f'    RegisterClass_{class_name}();')
    
    gen.append('}')
    gen.append('')
    gen.append('}} // namespace Helianthus::Reflection')
    
    _write_if_changed(os.path.join(out_dir, 'reflection_registrations.cpp'), '\n'.join(gen))

def generate_services_file(out_dir, all_classes):
    """生成RPC服务文件"""
    gen = []
    gen.append('#include "reflection_gen.h"')
    gen.append('')
    gen.append('namespace Helianthus { namespace Reflection {')
    # 前置声明每类服务注册函数
    for class_name, *_ in all_classes:
        gen.append(f'void RegisterRpc_{class_name}();')
    gen.append('')
    gen.append('void RegisterAllRpcServices()')
    gen.append('{')
    
    # 逐类调用
    for class_name, *_ in all_classes:
        gen.append(f'    RegisterRpc_{class_name}();')
    
    gen.append('}')
    gen.append('')
    gen.append('}} // namespace Helianthus::Reflection')
    
    _write_if_changed(os.path.join(out_dir, 'reflection_services.cpp'), '\n'.join(gen))

def _compute_class_hash(class_name, tags, props, meths, sfuncs, param_map, return_map, has_rpc_factory, header_mode):
    payload = {
        'name': class_name,
        'tags': list(tags),
        'props': [(t, m) for (t, m) in props],
        'meths': [
            {
                'tag': t,
                'name': m,
                'ret': return_map.get((t, m), ''),
                'params': param_map.get((t, m), []),
            }
            for (t, m) in meths
        ],
        'sfuncs': list(sfuncs),
        'rpc_factory': bool(has_rpc_factory),
        'header_mode': bool(header_mode),
    }
    data = json.dumps(payload, ensure_ascii=False, sort_keys=True).encode('utf-8')
    return hashlib.sha256(data).hexdigest()


def _write_if_changed(path, content):
    try:
        with open(path, 'r', encoding='utf-8') as f:
            old = f.read()
        if old == content:
            return False
    except Exception:
        pass
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    return True


def generate_per_class_files(classes_dir, all_classes, cache):
    """为每个类生成 registration/services 片段文件（带增量与参数名）"""
    class_hash_map = {}
    alive_files = set()
    for class_name, tags, props, meths, sfuncs, has_rpc_factory, header_mode, param_map, return_map in all_classes:
        class_hash = _compute_class_hash(class_name, tags, props, meths, sfuncs, param_map, return_map, has_rpc_factory, header_mode)
        class_hash_map[class_name] = class_hash
        # registration 独立CPP
        reg = []
        reg.append('#include "reflection_gen.h"')
        reg.append('namespace Helianthus { namespace Reflection {')
        reg.append(f'void RegisterClass_{class_name}() {{')
        reg.append(f'// {class_name} 类注册')
        reg.append(f'ClassRegistry::Get().RegisterClass("{class_name}", {{}}, nullptr);')
        for t in tags:
            reg.append(f'ClassRegistry::Get().AddClassTag("{class_name}", "{t}");')
        for tag, member in props:
            clean_tag = tag.replace('"', '').replace("'", '').strip()
            if ' ' in clean_tag or not clean_tag:
                continue
            if header_mode:
                reg.append(f'ClassRegistry::Get().RegisterProperty("{class_name}", "{member}", "{clean_tag}", offsetof({class_name}, {member}), sizeof((({class_name}*)0)->{member}));')
            else:
                reg.append(f'ClassRegistry::Get().RegisterProperty("{class_name}", "{member}", "{clean_tag}", 0, 0);')
        for tag, method in meths:
            params = param_map.get((tag, method), [])
            ret = return_map.get((tag, method), '')
            params_literal = ', '.join([f'"{p}"' for p in params])
            # 单标签当前转为单元素数组，后续可扩展解析多个标签
            reg.append(f'ClassRegistry::Get().RegisterMethodEx("{class_name}", "{method}", {{"{tag}"}}, "{ret}", "Public", "", false, {{{params_literal}}});')
        for func in sfuncs:
            reg.append(f'ClassRegistry::Get().RegisterMethodEx("{class_name}", "{func}", {{"Function"}}, "", "Public", "", true, {{}});')
        reg.append('}')
        reg.append('}}')
        reg_content = '\n'.join(reg)
        reg_path = os.path.join(classes_dir, f'{class_name}_registration.cpp')
        alive_files.add(reg_path)

        # services 独立CPP
        svc = []
        svc.append('#include "reflection_gen.h"')
        svc.append('namespace Helianthus { namespace Reflection {')
        svc.append(f'void RegisterRpc_{class_name}() {{')
        svc.append(f'// {class_name} RPC服务注册')
        svc.append(f'if (!Helianthus::RPC::RpcServiceRegistry::Get().HasService("{class_name}")) {{')
        if has_rpc_factory:
            svc.append(f'    Helianthus::RPC::RpcServiceRegistry::Get().RegisterService("{class_name}", "1.0.0", [](){{ return std::static_pointer_cast<Helianthus::RPC::IRpcService>(std::make_shared<{class_name}>()); }});')
        else:
            svc.append(f'    Helianthus::RPC::RpcServiceRegistry::Get().RegisterService("{class_name}", "1.0.0", [](){{ return std::shared_ptr<Helianthus::RPC::IRpcService>(); }});')
        svc.append('}')
        # 方法去重（同名方法按首个标签入表）
        seen_methods = set()
        for tag, method in meths:
            key = method.strip()
            if key in seen_methods:
                continue
            seen_methods.add(key)
            svc.append(f'Helianthus::RPC::RpcServiceRegistry::Get().RegisterMethod("{class_name}", Helianthus::RPC::RpcMethodMeta{{"{method}", "ReflectedRpc", "", "", {{"{tag.strip()}"}}, "", 100}});')
        svc.append('}')
        svc.append('}}')
        svc_content = '\n'.join(svc)
        svc_path = os.path.join(classes_dir, f'{class_name}_services.cpp')
        alive_files.add(svc_path)

        cached_hash = cache.get(class_name)
        _write_if_changed(reg_path, reg_content)
        _write_if_changed(svc_path, svc_content)

    # 清理已不存在类的过期文件
    try:
        for f in os.listdir(classes_dir):
            if not f.endswith(('_registration.cpp', '_services.cpp')):
                continue
            full = os.path.join(classes_dir, f)
            if full not in alive_files:
                try:
                    os.remove(full)
                except Exception:
                    pass
    except Exception:
        pass

    return class_hash_map

def generate_classes_compilation_file(out_dir, all_classes):
    gen = []
    gen.append('#include "reflection_gen.h"')
    gen.append('// 聚合按类生成的CPP，确保被编译')
    for class_name, *_ in all_classes:
        gen.append(f'#include "classes/{class_name}_registration.cpp"')
        gen.append(f'#include "classes/{class_name}_services.cpp"')
    _write_if_changed(os.path.join(out_dir, 'reflection_classes_compilation.cpp'), '\n'.join(gen))

def generate_auto_mount_file(out_dir):
    """生成自动挂载文件"""
    gen = []
    gen.append('#include "reflection_gen.h"')
    gen.append('')
    gen.append('namespace Helianthus { namespace RPC {')
    gen.append('// 引用全局符号以确保链接器不会丢弃初始化单元')
    gen.append('extern int g_ReflectionAutoInit;')
    gen.append('static int* volatile __force_link_init_ref = &g_ReflectionAutoInit;')
    gen.append('')
    gen.append('')
    gen.append('void RegisterReflectedServices(IRpcServer& Server) {')
    gen.append('    // 检查是否正在关闭，避免在程序退出时记录日志')
    gen.append('    if (Helianthus::Common::Logger::IsShuttingDown()) {')
    gen.append('        return;')
    gen.append('    }')
    gen.append('    auto Names = RpcServiceRegistry::Get().ListServices();')
    gen.append('    std::cout << "[DEBUG] RegisterReflectedServices called with " << Names.size() << " services" << std::endl;')
    gen.append('    std::cout << "[RPC] Auto-mounting " << Names.size() << " reflected services" << std::endl;')
    gen.append('    for (const auto& Name : Names) {')
    gen.append('        auto Service = RpcServiceRegistry::Get().Create(Name);')
    gen.append('        if (Service) {')
    gen.append('            (void)Server.RegisterService(Service);')
    gen.append('            auto Meta = RpcServiceRegistry::Get().GetMeta(Name);')
    gen.append('            std::cout << "[RPC] Mounted service: " << Name << " (version: " << Meta.Version << ", methods: " << Meta.Methods.size() << ")" << std::endl;')
    gen.append('            for (const auto& Method : Meta.Methods) {')
    gen.append('                std::cout << "[RPC]   - Method: " << Method.MethodName << " (tags: " << fmt::format("{}", fmt::join(Method.Tags, ", ")) << ")" << std::endl;')
    gen.append('            }')
    gen.append('        } else {')
    gen.append('            std::cout << "[RPC] Failed to create service: " << Name << std::endl;')
    gen.append('        }')
    gen.append('    }')
    gen.append('}')
    gen.append('')
    gen.append('void RegisterReflectedServices(IRpcServer& Server, const std::vector<std::string>& RequiredTags) {')
    gen.append('    // 检查是否正在关闭，避免在程序退出时记录日志')
    gen.append('    if (Helianthus::Common::Logger::IsShuttingDown()) {')
    gen.append('        return;')
    gen.append('    }')
    gen.append('    auto Names = RpcServiceRegistry::Get().ListServices();')
    gen.append('    std::cout << "[RPC] Auto-mounting reflected services with tags: " << fmt::format("{}", fmt::join(RequiredTags, ", ")) << std::endl;')
    gen.append('    int MountedCount = 0;')
    gen.append('    for (const auto& Name : Names) {')
    gen.append('        auto Meta = RpcServiceRegistry::Get().GetMeta(Name);')
    gen.append('        bool HasRequiredTag = false;')
    gen.append('        std::vector<std::string> MatchingMethods;')
    gen.append('        for (const auto& Method : Meta.Methods) {')
    gen.append('            bool MethodMatches = false;')
    gen.append('            for (const auto& Tag : Method.Tags) {')
    gen.append('                for (const auto& Required : RequiredTags) {')
    gen.append('                    if (Tag == Required) {')
    gen.append('                        MethodMatches = true;')
    gen.append('                        break;')
    gen.append('                    }')
    gen.append('                }')
    gen.append('                if (MethodMatches) break;')
    gen.append('            }')
    gen.append('            if (MethodMatches) {')
    gen.append('                HasRequiredTag = true;')
    gen.append('                MatchingMethods.push_back(Method.MethodName);')
    gen.append('            }')
    gen.append('        }')
    gen.append('        if (HasRequiredTag) {')
    gen.append('            auto Service = RpcServiceRegistry::Get().Create(Name);')
    gen.append('            if (Service) {')
    gen.append('                (void)Server.RegisterService(Service);')
    gen.append('                MountedCount++;')
    gen.append('                std::cout << "[RPC] Mounted service: " << Name << " (matching methods: " << fmt::format("{}", fmt::join(MatchingMethods, ", ")) << ")" << std::endl;')
    gen.append('            } else {')
    gen.append('                std::cout << "[RPC] Failed to create service: " << Name << std::endl;')
    gen.append('            }')
    gen.append('        } else {')
    gen.append('            std::cout << "[RPC] Skipped service: " << Name << " (no matching tags)" << std::endl;')
    gen.append('        }')
    gen.append('    }')
    gen.append('    std::cout << "[RPC] Auto-mount completed: " << MountedCount << " services mounted" << std::endl;')
    gen.append('}')
    gen.append('')
    gen.append('}} // namespace Helianthus::RPC')
    
    _write_if_changed(os.path.join(out_dir, 'reflection_auto_mount.cpp'), '\n'.join(gen))

def generate_init_file(out_dir):
    """生成初始化文件"""
    gen = []
    gen.append('#include "reflection_gen.h"')
    gen.append('')
    gen.append('// 定义全局符号以强制链接该翻译单元（放在 Helianthus::RPC 命名空间）')
    gen.append('namespace Helianthus { namespace RPC {')
    gen.append('int g_ReflectionAutoInit = 0;')
    gen.append('}}')
    gen.append('')
    gen.append('// 静态初始化函数，确保所有反射代码在程序启动时执行')
    gen.append('static int __reflection_init = []() -> int {')
    gen.append('    Helianthus::Reflection::RegisterAllClasses();')
    gen.append('    Helianthus::Reflection::RegisterAllRpcServices();')
    gen.append('    return 0;')
    gen.append('}();')
    
    _write_if_changed(os.path.join(out_dir, 'reflection_init.cpp'), '\n'.join(gen))

if __name__ == '__main__':
    main()


