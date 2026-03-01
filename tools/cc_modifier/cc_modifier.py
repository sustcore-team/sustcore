#!/usr/bin/env python3
import json
import shlex
import sys
from pathlib import Path
from abc import ABC, abstractmethod

# --- 统一接口设计 ---

class ArgOperation(ABC):
    """表示对编译选项操作的抽象基类"""
    @abstractmethod
    def process(self, args: list) -> list:
        pass

class RemoveFlags(ArgOperation):
    """删除指定的 flags"""
    def __init__(self, flags_to_remove):
        self.flags = set(flags_to_remove)
    
    def process(self, args: list) -> list:
        return [a for a in args if a not in self.flags]

class RegexTransform(ArgOperation):
    """使用正则表达式对 flags 进行模式转换"""
    def __init__(self, pattern: str, replacement: str):
        import re
        self.regex = re.compile(pattern)
        self.replacement = replacement

    def process(self, args: list) -> list:
        return [self.regex.sub(self.replacement, a) for a in args]

class AddFlags(ArgOperation):
    """在末尾添加指定的 flags"""
    def __init__(self, flags_to_add):
        self.flags = flags_to_add
    
    def process(self, args: list) -> list:
        return args + self.flags

# --- 处理逻辑 ---

def process_compile_entry(entry, operations):
    """处理单个 compile_commands 实体"""
    # 优先处理 arguments 列表，如果不存在则处理 command 字符串
    if 'arguments' in entry and isinstance(entry['arguments'], list):
        args = entry['arguments']
        for op in operations:
            args = op.process(args)
        entry['arguments'] = args
    elif 'command' in entry and isinstance(entry['command'], str):
        try:
            args = shlex.split(entry['command'])
            for op in operations:
                args = op.process(args)
            entry['command'] = ' '.join(shlex.quote(p) for p in args)
        except ValueError:
            # 如果 shlex 失败，回退到简单的字符串替换（仅支持移除）
            for op in operations:
                if isinstance(op, RemoveFlags):
                    for flag in op.flags:
                        entry['command'] = entry['command'].replace(f' {flag} ', ' ')
                        entry['command'] = entry['command'].replace(f' {flag}', '')
    return entry

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <path_to_compile_commands.json>")
        sys.exit(1)

    path = Path(sys.argv[1])
    if not path.exists():
        print(f"Error: {path} does not exist.")
        sys.exit(1)

    # 1. 定义操作流水线
    operations = [
        RemoveFlags(['-fno-toplevel-reorder', '-fno-tree-scev-cprop']),
        # 使用正则表达式转换: 将 ^-I 替换为 -isystem
        RegexTransform(pattern=r'^-I', replacement='-isystem'),
        RegexTransform(pattern=r'^-Wno-literal-suffix', replacement='-Wno-user-defined-literals'),
    ]

    # 2. 读取并处理数据
    with path.open('r', encoding='utf-8') as f:
        data = json.load(f)

    for entry in data:
        process_compile_entry(entry, operations)

    # 3. 写回文件
    with path.open('w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write('\n')

if __name__ == "__main__":
    main()

if __name__ == "__main__":
    main()
