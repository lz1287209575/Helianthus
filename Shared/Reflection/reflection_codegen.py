#!/usr/bin/env python3
import os
import re
import sys

def scan_sources(root):
    sources = []
    for base, _, files in os.walk(root):
        for f in files:
            if f.endswith(('.h', '.hpp', '.cpp', '.cc', '.cxx')):
                sources.append(os.path.join(base, f))
    return sources

CLASS_RE = re.compile(r'\bHCLASS\s*\(([^)]*)\)\s*class\s+(\w+)')
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
        class_tags = m.group(1).strip()
        class_name = m.group(2).strip()
        has_rpc_factory = bool(RPC_FACTORY_RE.search(text))
        classes.append((class_name, class_tags, text, has_rpc_factory))
    return classes

def extract_members(text):
    props = PROP_RE.findall(text)
    meths = METH_RE.findall(text)
    sfuncs = STATIC_FUNC_RE.findall(text)
    return props, meths, sfuncs

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
        print('usage: reflection_codegen.py <source_root> <out_cpp>')
        sys.exit(1)
    root = sys.argv[1]
    out_cpp = sys.argv[2]

    gen = []
    gen.append('#include "Shared/Reflection/ReflectionCore.h"')
    gen.append('#include "Shared/RPC/RpcReflection.h"')
    gen.append('#include "Shared/RPC/IRpcServer.h"')
    gen.append('#include <cstddef>')
    gen.append('using namespace Helianthus::Reflection;')
    includes = set()

    idx = 0
    seen_services = set()
    for src in scan_sources(root):
        try:
            classes = parse_file(src)
        except Exception:
            continue
        for class_name, class_tags, text, has_rpc_factory in classes:
            # Sanitize tags to avoid quotes and exotic content
            clean = class_tags.replace('"', '\"')
            tags = [t.strip() for t in clean.split(',') if t.strip() and ' ' not in t]
            props, meths, sfuncs = extract_members(text)
            # if class is defined in a header, we can include and compute offsetof/sizeof
            header_mode = os.path.splitext(src)[1] in ('.h', '.hpp', '.hh', '.hxx')
            if header_mode and src not in includes and '/Examples/' not in src:
                rel = os.path.relpath(src, root)
                gen.append(f'#include "{rel}"')
                includes.add(src)
            gen.append(f'static int __refl_reg_{idx} = []() -> int {{')
            # class factory as nullptr; users can supply later if needed
            gen.append(f'    ClassRegistry::Get().RegisterClass("{class_name}", {{}}, nullptr);')
            for t in tags:
                gen.append(f'    ClassRegistry::Get().AddClassTag("{class_name}", "{t}");')
            for tag, member in props:
                tag = tag.replace('"', '\"')
                if header_mode:
                    gen.append(f'    ClassRegistry::Get().RegisterProperty("{class_name}", "{member}", "{tag}", offsetof({class_name}, {member}), sizeof((({class_name}*)0)->{member}));')
                else:
                    gen.append(f'    ClassRegistry::Get().RegisterProperty("{class_name}", "{member}", "{tag}", 0, 0);')
            for tag, method in meths:
                # best-effort: try to find signature to extract param names
                params = []
                # search with the method name to match correct signature
                for m2 in METH_SIG_RE.finditer(text):
                    name2 = m2.group(3).strip()
                    if name2 != method:
                        continue
                    raw_params = m2.group(4).strip()
                    if raw_params and raw_params != 'void':
                        for p in split_params(raw_params):
                            name = extract_param_name(p)
                            if name:
                                params.append(name)
                    break
                if params:
                    vec = ', '.join(f'"{p}"' for p in params)
                    gen.append(f'    ClassRegistry::Get().RegisterMethodEx("{class_name}", "{method}", "{tag}", false, {{{vec}}});')
                else:
                    gen.append(f'    ClassRegistry::Get().RegisterMethod("{class_name}", "{method}", "{tag}");')
            for func in sfuncs:
                gen.append(f'    ClassRegistry::Get().RegisterMethodEx("{class_name}", "{func}", "Function", true, {{}});')

            # auto-bridge Rpc-tagged methods to RPC registry (ensure service, register method meta)
            for tag, method in meths:
                if tag.strip() == 'Rpc':
                    gen.append(f'    if (!Helianthus::RPC::RpcServiceRegistry::Get().HasService("{class_name}")) {{')
                    if has_rpc_factory:
                        gen.append(f'        Helianthus::RPC::RpcServiceRegistry::Get().RegisterService("{class_name}", "1.0.0", [](){{ return std::static_pointer_cast<Helianthus::RPC::IRpcService>(std::make_shared<{class_name}>()); }});')
                    else:
                        gen.append(f'        Helianthus::RPC::RpcServiceRegistry::Get().RegisterService("{class_name}", "1.0.0", [](){{ return std::shared_ptr<Helianthus::RPC::IRpcService>(); }});')
                    gen.append('    }')
                    gen.append(f'    Helianthus::RPC::RpcServiceRegistry::Get().RegisterMethod("{class_name}", Helianthus::RPC::RpcMethodMeta{{"{method}", "ReflectedRpc", "", ""}});')
                    gen.append(f'    (void)0;')
                    seen_services.add(class_name)
            gen.append('    return 0;')
            gen.append('}();')
            idx += 1

    # Emit auto-mount function: iterate registry and attach to server
    gen.append('namespace Helianthus { namespace RPC {')
    gen.append('void RegisterReflectedServices(IRpcServer& Server) {')
    gen.append('    auto Names = RpcServiceRegistry::Get().ListServices();')
    gen.append('    for (const auto& Name : Names) {')
    gen.append('        auto Service = RpcServiceRegistry::Get().Create(Name);')
    gen.append('        if (Service) { (void)Server.RegisterService(Service); }')
    gen.append('    }')
    gen.append('}')
    gen.append('}}')


    os.makedirs(os.path.dirname(out_cpp), exist_ok=True)
    with open(out_cpp, 'w', encoding='utf-8') as f:
        f.write('\n'.join(gen))

if __name__ == '__main__':
    main()


