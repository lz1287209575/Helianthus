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

def _extract_class_text(text, start_pos, class_name):
    """提取类定义的范围文本"""
    # 从 HCLASS 开始，找到对应的类定义结束位置
    lines = text[start_pos:].split('\n')
    class_lines = []
    brace_count = 0
    in_class = False
    found_class = False
    
    for i, line in enumerate(lines):
        class_lines.append(line)
        
        # 检查是否找到类定义
        if f'class {class_name}' in line and '{' in line:
            found_class = True
            in_class = True
            brace_count = line.count('{') - line.count('}')
        elif found_class and in_class:
            brace_count += line.count('{') - line.count('}')
            if brace_count <= 0 and '}' in line:
                # 类定义结束，检查下一行
                if i + 1 < len(lines):
                    next_line = lines[i + 1].strip()
                    # 如果下一行是注释、HCLASS 或另一个类定义，则停止
                    if (next_line.startswith('//') or 
                        next_line.startswith('HCLASS') or 
                        next_line.startswith('class') or
                        next_line == ''):
                        break
                else:
                    break
    
    return '\n'.join(class_lines)

def _strip_line_comments(text: str) -> str:
    """去除单行 // 注释和简单的 /* */ 单行块注释，便于宏检测避免误匹配注释内容"""
    lines = text.split('\n')
    cleaned = []
    for line in lines:
        # 去掉 // 之后内容
        if '//' in line:
            line = line.split('//', 1)[0]
        # 去掉单行块注释
        line = re.sub(r"/\*.*?\*/", "", line)
        cleaned.append(line)
    return '\n'.join(cleaned)

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
        
        # 提取类定义的范围
        class_text = _extract_class_text(text, m.start(), class_name)
        # 在检测 HRPC_FACTORY 前去除注释，避免注释中的宏被误识别
        class_text_no_comments = _strip_line_comments(class_text)
        has_rpc_factory = bool(RPC_FACTORY_RE.search(class_text_no_comments))
        classes.append((class_name, class_tags, class_text, has_rpc_factory))
    return classes

_QUALIFIERS = {
    'inline', 'virtual', 'constexpr', 'consteval', 'constinit', 'static', 'friend',
    'volatile', 'noexcept', 'override', 'final', 'register'
}


def _clean_return_type(prefix: str) -> str:
    """提取返回类型：尽量保留模板/命名空间，剔除限定词与多余空白。
    示例：
      - const std::vector<std::pair<int,std::string>>& -> std::vector<std::pair<int,std::string>>
      - inline virtual MyType&& -> MyType
    """
    p = prefix.strip()
    # 去除 C++ 属性 [[...]]
    p = re.sub(r"\[\[[^\]]*\]\]", " ", p)
    # 去除限定修饰词
    tokens = []
    buf = []
    i = 0
    depth_angle = 0
    while i < len(p):
        ch = p[i]
        if ch == '<':
            depth_angle += 1
            buf.append(ch)
        elif ch == '>':
            depth_angle = max(0, depth_angle - 1)
            buf.append(ch)
        else:
            buf.append(ch)
        i += 1
    s = ''.join(buf)
    # 移除指针/引用修饰符与多余空白
    s = s.replace('&', ' ').replace('*', ' ')
    parts = [t for t in s.split() if t and t not in _QUALIFIERS and t != 'const']
    cleaned = ' '.join(parts)
    # 规范化空白与逗号后空格
    cleaned = re.sub(r"\s*<\s*", "<", cleaned)
    cleaned = re.sub(r"\s*>\s*", ">", cleaned)
    cleaned = re.sub(r"\s*,\s*", ",", cleaned)
    cleaned = re.sub(r"\s*::\s*", "::", cleaned)
    return cleaned


def _parse_tags(tag_str: str) -> list[str]:
    """解析标签字符串，支持 | 和 , 分隔符"""
    if not tag_str:
        return []
    # 先按 | 分割，再按 , 分割，然后去重并过滤空字符串
    tags = []
    for part in tag_str.split('|'):
        for subpart in part.split(','):
            cleaned = subpart.strip()
            if cleaned and cleaned not in tags:
                tags.append(cleaned)
    return tags


def _extract_method_comment(text: str, method_start: int) -> str:
    """提取方法前的注释作为描述"""
    # 从方法开始位置向前查找注释
    lines = text[:method_start].split('\n')
    comment_lines = []
    
    # 从最后一行开始向前查找
    for line in reversed(lines):
        line = line.strip()
        if not line:
            continue
        if line.startswith('//'):
            # 提取注释内容，去掉 // 前缀
            comment = line[2:].strip()
            if comment:
                comment_lines.insert(0, comment)
        elif line.startswith('/*') and line.endswith('*/'):
            # 单行块注释
            comment = line[2:-2].strip()
            if comment:
                comment_lines.insert(0, comment)
        else:
            # 遇到非注释行，停止查找
            break
    
    return ' '.join(comment_lines)


def _parse_qualifiers_from_tags(tag_str: str, text: str, method_start: int) -> dict:
    """通过标签解析方法的限定符和属性"""
    tags = _parse_tags(tag_str)
    
    qualifiers = {
        'is_pure_function': 'PureFunction' in tags,
        # C++ 语义限定符仅从函数签名推断，不依赖标签
        'is_virtual': False,
        'is_inline': False,
        'is_deprecated': 'Deprecated' in tags,
        'is_const': False,
        'is_noexcept': False,
        'is_override': False,
        'is_final': False,
        'is_static': False,
        'access_modifier': 'Public',
        'other_qualifiers': []
    }
    
    # 从标签中提取访问修饰符（如有）
    for tag in tags:
        if tag in ['Public', 'Protected', 'Private', 'Friend']:
            qualifiers['access_modifier'] = tag
        elif tag in ['Explicit', 'Friend']:
            qualifiers['other_qualifiers'].append(tag.lower())
    
    # 从方法声明中提取 C++ 语义限定符
    lines = text[:method_start].split('\n')
    for line in reversed(lines):
        line = line.strip()
        if not line or line.startswith('//') or line.startswith('/*'):
            continue
        
        # 检查方法声明行中的限定符
        if ' const' in f' {line}':
            qualifiers['is_const'] = True
        if 'noexcept' in line:
            qualifiers['is_noexcept'] = True
        if 'override' in line:
            qualifiers['is_override'] = True
        if 'final' in line:
            qualifiers['is_final'] = True
        if 'virtual' in line:
            qualifiers['is_virtual'] = True
        if 'inline' in line:
            qualifiers['is_inline'] = True
        if 'static' in line:
            qualifiers['is_static'] = True
        if 'explicit' in line:
            qualifiers['other_qualifiers'].append('explicit')
        
        # 如果找到方法声明，停止查找
        if '(' in line and ')' in line:
            break
    
    return qualifiers


def extract_members(text):
    props = PROP_RE.findall(text)
    meths = METH_RE.findall(text)
    sfuncs = STATIC_FUNC_RE.findall(text)
    
    # 提取带签名的方法，用于参数名、返回类型、多标签与注释
    param_map = {}
    return_map = {}
    tags_map = {}
    comment_map = {}
    qualifier_map = {}
    
    # 处理所有方法
    for m in METH_SIG_RE.finditer(text):
        tag = (m.group(1) or '').strip()
        method_name = (m.group(3) or '').strip()
        params_raw = (m.group(4) or '').strip()
        if not method_name:
            continue
        # 返回类型在 group(2)
        ret_prefix = (m.group(2) or '').strip()
        return_map[(tag, method_name)] = _clean_return_type(ret_prefix)
        # 解析多标签
        tags_map[(tag, method_name)] = _parse_tags(tag)
        # 提取方法前的注释
        comment_map[(tag, method_name)] = _extract_method_comment(text, m.start())
        # 通过标签解析限定符
        qualifier_map[(tag, method_name)] = _parse_qualifiers_from_tags(tag, text, m.start())
        param_names = []
        if params_raw:
            for part in split_params(params_raw):
                name = extract_param_name(part)
                if name:
                    param_names.append(name)
        param_map[(tag, method_name)] = param_names
    
    return props, meths, sfuncs, param_map, return_map, tags_map, comment_map, qualifier_map

def split_params(param_str: str):
    parts = []
    buf = []
    depth_angle = 0
    depth_paren = 0
    depth_brace = 0
    in_single = False
    in_double = False
    escape = False
    i = 0
    while i < len(param_str):
        ch = param_str[i]
        if in_single:
            buf.append(ch)
            if not escape and ch == '\\':
                escape = True
            elif escape:
                escape = False
            elif ch == '\'':
                in_single = False
            i += 1
            continue
        if in_double:
            buf.append(ch)
            if not escape and ch == '\\':
                escape = True
            elif escape:
                escape = False
            elif ch == '"':
                in_double = False
            i += 1
            continue
        if ch == '\'':
            in_single = True
            buf.append(ch)
            i += 1
            continue
        if ch == '"':
            in_double = True
            buf.append(ch)
            i += 1
            continue
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
        elif ch == '{':
            depth_brace += 1
        elif ch == '}':
            if depth_brace > 0:
                depth_brace -= 1
        elif ch == ',' and depth_angle == 0 and depth_paren == 0 and depth_brace == 0:
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
    """尽力从参数声明中提取名称，支持函数指针、数组、引用、默认值与属性。"""
    original = param
    # 去除默认值
    if '=' in param:
        param = param.split('=')[0].strip()
    # 去除 C++ 属性 [[...]] 与 __attribute__(...)
    param = re.sub(r"\[\[[^\]]*\]\]", " ", param)
    param = re.sub(r"__attribute__\s*\(.*?\)", " ", param)
    # 函数指针/成员函数指针: int (*Func)(int), void (Cls:: *cb)(int)
    m = re.search(r"\(\s*(?:[A-Za-z_][A-Za-z0-9_]*\s*::\s*)?[*&]\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)", param)
    if m:
        return m.group(1)
    # 普通指针/引用或数组: type& Name[] / type * Name
    # 先移除修饰符以便提取末尾标识符
    tmp = param
    tmp = tmp.replace('&&', ' ').replace('&', ' ').replace('*', ' ')
    tmp = tmp.replace('[', ' ').replace(']', ' ')
    tokens = [t for t in tmp.strip().split() if t]
    if tokens:
        cand = tokens[-1]
        if _IDENT_RE.match(cand):
            return cand
    # 尝试从原串末尾回溯找到标识符
    m2 = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*(\[|$|\)|,)", param[::-1])
    if m2:
        # 由于反转了字符串，这里不使用该方式，回退到正向通用匹配
        pass
    # 通用匹配：抓取最后一个标识符
    m3 = list(re.finditer(r"[A-Za-z_][A-Za-z0-9_]*", param))
    if m3:
        return m3[-1].group(0)
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
        for class_name, class_tags, class_text, has_rpc_factory in classes:
            # Sanitize tags to avoid quotes and exotic content
            clean = class_tags.replace('"', '\"')
            tags = [t.strip() for t in clean.split(',') if t.strip() and ' ' not in t]
            props, meths, sfuncs, param_map, return_map, tags_map, comment_map, qualifier_map = extract_members(class_text)
            header_mode = os.path.splitext(src)[1] in ('.h', '.hpp', '.hh', '.hxx')
            
            if src not in includes:
                rel = os.path.relpath(src, root)
                if header_mode:
                    includes.add(rel)
            # 记录相对目录用于分层输出
            rel_dir = os.path.dirname(os.path.relpath(src, root))
            all_classes.append((class_name, tags, props, meths, sfuncs, has_rpc_factory, header_mode, param_map, return_map, tags_map, comment_map, qualifier_map, rel_dir))
    
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
    # 过滤 Tests/ 路径下的类，不参与聚合入口
    filtered = [item for item in all_classes if not (item[-1].startswith('Tests/'))]
    # 前置声明每类注册函数
    for class_name, *_ in filtered:
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
    # 逐类调用（不包含 Tests/ 下的类）
    for class_name, *_ in filtered:
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
    # 过滤 Tests/ 路径下的类，不参与聚合入口
    filtered = [item for item in all_classes if not (item[-1].startswith('Tests/'))]
    # 前置声明每类服务注册函数
    for class_name, *_ in filtered:
        gen.append(f'void RegisterRpc_{class_name}();')
    gen.append('')
    gen.append('void RegisterAllRpcServices()')
    gen.append('{')
    
    # 逐类调用（不包含 Tests/ 下的类）
    for class_name, *_ in filtered:
        gen.append(f'    RegisterRpc_{class_name}();')
    
    gen.append('}')
    gen.append('')
    gen.append('}} // namespace Helianthus::Reflection')
    
    _write_if_changed(os.path.join(out_dir, 'reflection_services.cpp'), '\n'.join(gen))

def _compute_class_hash(class_name, tags, props, meths, sfuncs, param_map, return_map, tags_map, comment_map, qualifier_map, has_rpc_factory, header_mode, rel_dir):
    payload = {
        'name': class_name,
        'tags': list(tags),
        'props': [(t, m) for (t, m) in props],
        'meths': [
            {
                'tag': t,
                'name': m,
                'ret': return_map.get((t, m), ''),
                'tags': tags_map.get((t, m), [t]),  # 默认回退到单标签
                'params': param_map.get((t, m), []),
                'comment': comment_map.get((t, m), ''),
                'qualifiers': qualifier_map.get((t, m), {}),
            }
            for (t, m) in meths
        ],
        'sfuncs': list(sfuncs),
        'rpc_factory': bool(has_rpc_factory),
        'header_mode': bool(header_mode),
        'rel_dir': rel_dir,
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
    for class_name, tags, props, meths, sfuncs, has_rpc_factory, header_mode, param_map, return_map, tags_map, comment_map, qualifier_map, rel_dir in all_classes:
        class_hash = _compute_class_hash(class_name, tags, props, meths, sfuncs, param_map, return_map, tags_map, comment_map, qualifier_map, has_rpc_factory, header_mode, rel_dir)
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
            # 仅保留业务语义标签：优先使用签名解析得到的 tags_map
            raw_tags = tags_map.get((tag, method))
            if raw_tags is None:
                # 回退：按方法名匹配任一记录
                for (tkey, mkey), v in tags_map.items():
                    if mkey == method and v:
                        raw_tags = v
                        break
            if raw_tags is None:
                raw_tags = _parse_tags(tag)
            method_tags = [t for t in raw_tags if t not in ['Static','Virtual','Const','Noexcept','Override','Final','Inline']]
            comment = comment_map.get((tag, method), '')
            qualifiers = qualifier_map.get((tag, method), {})
            
            # 提取限定符信息
            is_pure = qualifiers.get('is_pure_function', False)
            is_const = qualifiers.get('is_const', False)
            is_noexcept = qualifiers.get('is_noexcept', False)
            is_virtual = qualifiers.get('is_virtual', False)
            is_override = qualifiers.get('is_override', False)
            is_final = qualifiers.get('is_final', False)
            is_inline = qualifiers.get('is_inline', False)
            is_deprecated = qualifiers.get('is_deprecated', False)
            is_static = qualifiers.get('is_static', False)
            access_modifier = qualifiers.get('access_modifier', 'Public')
            other_qualifiers = qualifiers.get('other_qualifiers', [])
            
            params_literal = ', '.join([f'"{p}"' for p in params])
            tags_literal = ', '.join([f'"{t}"' for t in method_tags])
            qualifiers_literal = ', '.join([f'"{q}"' for q in other_qualifiers])
            
            # 使用新的完整元数据注册函数
            reg.append(f'ClassRegistry::Get().RegisterMethodEx("{class_name}", "{method}", {{{tags_literal}}}, "{ret}", "{access_modifier}", "{comment}", {str(is_static).lower()}, {{{params_literal}}}, {str(is_pure).lower()}, {str(is_const).lower()}, {str(is_noexcept).lower()}, {str(is_virtual).lower()}, {str(is_override).lower()}, {str(is_final).lower()}, {str(is_inline).lower()}, {str(is_deprecated).lower()}, "{access_modifier}", {{{qualifiers_literal}}});')
        for func in sfuncs:
            reg.append(f'ClassRegistry::Get().RegisterMethodEx("{class_name}", "{func}", {{"Function"}}, "", "Public", "", true, {{}});')
        reg.append('}')
        reg.append('}}')
        reg_content = '\n'.join(reg)
        target_dir = os.path.join(classes_dir, rel_dir)
        os.makedirs(target_dir, exist_ok=True)
        reg_path = os.path.join(target_dir, f'{class_name}_registration.cpp')
        alive_files.add(reg_path)

        # services 独立CPP
        svc = []
        svc.append('#include "reflection_gen.h"')
        svc.append('namespace Helianthus { namespace Reflection {')
        svc.append(f'void RegisterRpc_{class_name}() {{')
        svc.append(f'// {class_name} RPC服务注册')
        # 如果类含 NoAutoRegister 标签，则仅跳过工厂注册，仍写入方法元信息
        no_auto = ('NoAutoRegister' in tags)
        if not no_auto and not os.environ.get('HELIANTHUS_REFLECTION_SKIP_FACTORY_AUTO_REGISTER'):
            svc.append(f'if (!Helianthus::RPC::RpcServiceRegistry::Get().HasService("{class_name}")) {{')
            if has_rpc_factory:
                svc.append(f'    Helianthus::RPC::RpcServiceRegistry::Get().RegisterService("{class_name}", "1.0.0", [](){{ return std::static_pointer_cast<Helianthus::RPC::IRpcService>(std::make_shared<{class_name}>()); }});')
            else:
                svc.append(f'    Helianthus::RPC::RpcServiceRegistry::Get().RegisterService("{class_name}", "1.0.0", [](){{ return std::shared_ptr<Helianthus::RPC::IRpcService>(); }});')
            svc.append('}')
        # 方法去重（同名方法按首个出现入表），并写入全部标签
        seen_methods = set()
        for tag, method in meths:
            key = method.strip()
            if key in seen_methods:
                continue
            seen_methods.add(key)
            # 仅保留业务语义标签：优先使用签名解析得到的 tags_map
            raw_tags = tags_map.get((tag, method))
            if raw_tags is None:
                for (tkey, mkey), v in tags_map.items():
                    if mkey == method and v:
                        raw_tags = v
                        break
            if raw_tags is None:
                raw_tags = _parse_tags(tag)
            method_tags = [t for t in raw_tags if t not in ['Static','Virtual','Const','Noexcept','Override','Final','Inline']]
            tags_literal = ', '.join([f'"{t}"' for t in method_tags])
            svc.append(f'Helianthus::RPC::RpcServiceRegistry::Get().RegisterMethod("{class_name}", Helianthus::RPC::RpcMethodMeta{{"{method}", "ReflectedRpc", "", "", {{{tags_literal}}}, "", 100}});')
        svc.append('}')
        svc.append('}}')
        svc_content = '\n'.join(svc)
        svc_path = os.path.join(target_dir, f'{class_name}_services.cpp')
        alive_files.add(svc_path)

        cached_hash = cache.get(class_name)
        _write_if_changed(reg_path, reg_content)
        _write_if_changed(svc_path, svc_content)

    # 清理已不存在类的过期文件
    try:
        for root_dir, _, files in os.walk(classes_dir):
            for f in files:
                if not f.endswith(('_registration.cpp', '_services.cpp')):
                    continue
                full = os.path.join(root_dir, f)
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
    # 过滤 Tests/ 路径下的类，不把它们包含进聚合编译单元
    for item in [it for it in all_classes if not (it[-1].startswith('Tests/'))]:
        class_name = item[0]
        rel_dir = item[-1]
        include_base = f'classes/{rel_dir}/{class_name}' if rel_dir else f'classes/{class_name}'
        gen.append(f'#include "{include_base}_registration.cpp"')
        gen.append(f'#include "{include_base}_services.cpp"')
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


