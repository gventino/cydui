#!/usr/bin/env python3
"""Writes tests/test_api_surface.c: the compile-time contract of the public API (docs/11-api.md).

It reads include/cydui/*.h and emits, for every public function, a pointer of the exact type
initialised with it (so a changed parameter or return type stops the build); for every enumerator and
constant macro, its value; for every tunable macro, that it is still defined; for every public struct, its
fields (and, for the ones without pointers, its size).

The generated file is meant to be REVIEWED and COMMITTED: it is the contract. Run this script only to add
the entries of API that was added on purpose; a diff that changes or removes a line is an API change and
needs the rules of docs/11-api.md.

Usage: tools/gen_api_surface.py [--out tests/test_api_surface.c]
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
INC = os.path.join(ROOT, "include")
HEADERS = ["types", "arena", "cmd", "font", "utf8", "text", "backend", "cydui", "theme", "ui"]
PRIVATE_STRUCTS = {"cydui_ui", "cydui_region", "cydui_input", "cydui_ctx", "cydui_arena"}
GUARD = re.compile(r"^CYDUI_\w+_H$")


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def read(name):
    with open(os.path.join(INC, "cydui", name + ".h")) as f:
        return f.read()


def macros(raw):
    """(constants, tunables, function_like) names defined by #define in the raw text."""
    consts, tunables, funcs = [], [], []
    lines = strip_comments(raw).split("\n")
    for i, line in enumerate(lines):
        m = re.match(r"\s*#\s*define\s+(CYDUI_\w+)(\(?)\s*(.*)$", line)
        if not m or GUARD.match(m.group(1)):
            continue
        name, paren, rest = m.groups()
        if paren:
            funcs.append(name)
        elif re.match(r"^\(?\s*[-\d(][\w\s<>()+*|~-]*$", rest.strip()) and rest.strip() and not rest.strip().startswith('"'):
            prev = lines[i - 1] if i else ""
            (tunables if re.match(r"\s*#\s*ifndef\s+" + name + r"\s*$", prev) else consts).append(name)
    return consts, tunables, funcs


def statements(text):
    """Top-level statements with their braced bodies: [(head, body or None, tail)]."""
    text = re.sub(r"^[ \t]*#(?:[^\n]*\\\n)*[^\n]*$", "", text, flags=re.M)  # directives, with their continuation lines
    text = re.sub(r'extern\s+"C"\s*\{', "", text)
    lone = [m for m in re.finditer(r"^\}\s*$", text, flags=re.M)]
    if lone:  # the one that closes extern "C" is the last
        text = text[: lone[-1].start()] + text[lone[-1].end():]
    out, buf, body, depth, head = [], [], [], 0, None
    for ch in text:
        if ch == "{":
            if depth == 0:
                head = "".join(buf)
                buf, body = [], []
            else:
                body.append(ch)
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                if head and "(" in head and not head.startswith(("typedef", "struct", "enum")):
                    out.append((" ".join(head.split()), None, " ".join(head.split())))  # a function with a body (static inline)
                    buf, body, head = [], [], None
                continue
            body.append(ch)
        elif depth > 0:
            body.append(ch)
        elif ch == ";":
            tail = "".join(buf)
            out.append((" ".join((head or "").split()), "".join(body) if head is not None else None, " ".join(tail.split())))
            buf, body, head = [], [], None
        else:
            buf.append(ch)
    return out


def split_fields(body):
    """Statements of a struct body (nested braces kept together, then skipped)."""
    res, buf, depth = [], [], 0
    for ch in body:
        if ch == "{":
            depth += 1
        if ch == "}":
            depth -= 1
        if ch == ";" and depth == 0:
            res.append(" ".join("".join(buf).split()))
            buf = []
        else:
            buf.append(ch)
    return [r for r in res if r]


def parse_fields(body):
    fields = []  # (name, type or None, kind) kind: scalar|array|func
    for st in split_fields(body):
        if "{" in st:
            m = re.search(r"\}\s*(\w+)\s*$", st)
            if m:
                fields.append((m.group(1), None, "array"))
            continue
        fp = re.search(r"\(\s*\*\s*(\w+)\s*\)", st)
        if fp:
            fields.append((fp.group(1), None, "func"))
            continue
        st = re.sub(r":\s*\d+\s*$", "", st)
        parts = [p.strip() for p in st.split(",")]
        first = re.match(r"^(?P<type>.*?[\s\*])(?P<name>\w+)(?P<arr>(\s*\[[^\]]*\])*)$", parts[0])
        if not first:
            continue
        base = first.group("type").rstrip()
        stars = re.search(r"(\**)\s*$", base).group(1)
        base_no_stars = base[: len(base) - len(stars)].rstrip()
        fields.append((first.group("name"), base, "array" if first.group("arr") else "scalar"))
        for p in parts[1:]:
            m = re.match(r"^(?P<ptr>\**)\s*(?P<name>\w+)(?P<arr>(\s*\[[^\]]*\])*)$", p)
            if m:
                fields.append((m.group("name"), (base_no_stars + " " + m.group("ptr")).rstrip(),
                               "array" if m.group("arr") else "scalar"))
    return fields


def main():
    out_path = os.path.join(ROOT, "tests", "test_api_surface.c")
    if "--out" in sys.argv:
        out_path = sys.argv[sys.argv.index("--out") + 1]

    funcs, enums, structs, consts, tunables, fmacros, enum_typedefs = [], [], {}, [], [], [], []
    for h in HEADERS:
        raw = read(h)
        c, t, f = macros(raw)
        consts += c
        tunables += t
        fmacros += f
        for head, body, tail in statements(strip_comments(raw)):
            if body is not None:
                if "enum" in head:
                    if head.startswith("typedef"):
                        enum_typedefs.append(tail.split()[-1])
                    for item in body.split(","):
                        m = re.match(r"\s*(CYDUI_\w+)", item)
                        if m:
                            enums.append(m.group(1))
                    continue
                if head.startswith("typedef struct") or head.startswith("struct"):
                    name = tail.split()[-1] if head.startswith("typedef") else head.split()[1]
                    structs[name] = parse_fields(body)
                continue
            if tail.startswith("typedef"):
                continue
            m = re.match(r"^(?P<ret>[\w\s\*]+?)\s*\b(?P<name>cydui_\w+)\s*\((?P<params>.*)\)$", tail)
            if m and "=" not in tail:
                ret = " ".join(m.group("ret").replace("extern", "").replace("static", "").replace("inline", "").split())
                funcs.append((ret, m.group("name"), m.group("params").strip()))
    enums = sorted(set(e for e in enums if not e.endswith("_COUNT") or True))
    consts = sorted(set(consts))
    tunables = sorted(set(tunables))
    stable = {k: v for k, v in structs.items() if k not in PRIVATE_STRUCTS}

    simple = re.compile(r"^(u?int(8|16|32)_t|bool|cydui_color)$")
    enum_types = set(enum_typedefs)

    def pointer_free(name, seen=()):
        """True when the size does not depend on the target: only fixed-width integers, bool, enums and
        structs like that (a pointer or a size_t changes between the PC and the ESP32)."""
        for fname, ftype, kind in stable[name]:
            t = (ftype or "").replace("const", "").strip()
            if kind == "func" or ftype is None or "*" in t:
                return False
            if simple.match(t) or t in enum_types:
                continue
            if t in stable and t not in seen and pointer_free(t, seen + (name,)):
                continue
            return False
        return True

    plain = sorted(n for n in stable if pointer_free(n))

    # ask the compiler for the values
    probe = "#include <stdio.h>\n" + "".join('#include "cydui/%s.h"\n' % h for h in HEADERS) + "int main(void){\n"
    for n in enums + consts:
        probe += '    printf("%s %%lld\\n", (long long)(%s));\n' % (n, n)
    for n in plain:
        probe += '    printf("sizeof(%s) %%lld\\n", (long long)sizeof(%s));\n' % (n, n)
    probe += "    return 0;\n}\n"
    with tempfile.TemporaryDirectory() as d:
        src, exe = os.path.join(d, "p.c"), os.path.join(d, "p")
        open(src, "w").write(probe)
        subprocess.run(["gcc", "-std=c99", "-I" + INC, src, "-o", exe], check=True)
        values = dict(line.rsplit(" ", 1) for line in subprocess.run([exe], capture_output=True, text=True, check=True).stdout.splitlines())

    o = []
    w = o.append
    w("/*\n * The compile-time contract of the public API (docs/11-api.md).\n *\n"
      " * Generated once by tools/gen_api_surface.py and then REVIEWED AND KEPT BY HAND: a change to a line\n"
      " * here is a change to the API, and needs a CHANGELOG entry, docs/11-api.md and the version bumped in\n"
      " * the same commit. Adding API means adding lines (run the script and keep only the new ones).\n *\n"
      " *   - every function is bound to a pointer of its exact type: a changed parameter or result stops the build\n"
      " *   - every enumerator and constant macro has the value it had when the API was frozen\n"
      " *   - every tunable macro is still there (its default value may change)\n"
      " *   - every field of every stable struct is still there with its type; structs without pointers keep their size\n */\n")
    w("#include <stddef.h>\n#include <stdio.h>\n#include <string.h>\n")
    for h in HEADERS:
        w('#include "cydui/%s.h"' % h)
    w('#include "unity.h"\n\nvoid setUp(void) {}\nvoid tearDown(void) {}\n')
    w("#define CT_ASSERT(name, cond) typedef char ct_##name[(cond) ? 1 : -1]\n")
    w("/* ---- functions ---- */\n")
    w("#define SURFACE(ret, name, params) static ret(*const surface_##name) params = name;\n")
    for ret, name, params in sorted(funcs, key=lambda f: f[1]):
        w("SURFACE(%s, %s, (%s))" % (ret, name, params))
    w("\n/* ---- enumerators and constants ---- */\n")
    for n in enums + consts:
        w("CT_ASSERT(%s, (long long)(%s) == %sLL);" % (n, n, values[n]))
    w("\n/* ---- tunable macros: the name is stable, the default may change ---- */\n")
    for n in tunables:
        w("#ifndef %s\n#error \"%s is part of the API\"\n#endif" % (n, n))
    w("\n/* ---- function-like macros ---- */\n")
    for n in sorted(set(fmacros)):
        w("#ifndef %s\n#error \"%s is part of the API\"\n#endif" % (n, n))
    w("\n/* ---- structs: fields and types ---- */\n")
    w("static void structs_have_their_fields(void)\n{")
    for name in sorted(stable):
        w("    {\n        %s v;\n        (void)v;" % name)
        for fname, ftype, kind in stable[name]:
            if kind == "scalar" and ftype:
                w("        { %s *p = &v.%s; (void)p; }" % (ftype.strip(), fname))
            else:
                w("        (void)sizeof(v.%s);" % fname)
        w("    }")
    w("}\n")
    for n in plain:
        w("CT_ASSERT(sizeof_%s, sizeof(%s) == %s);" % (n, n, values["sizeof(%s)" % n]))  # only types whose size is the same on the PC and the ESP32
    w("\n/* ---- run ---- */\n")
    w("static void test_every_function_is_there(void)\n{")
    for ret, name, params in sorted(funcs, key=lambda f: f[1]):
        w("    TEST_ASSERT_NOT_NULL(surface_%s);" % name)
    w("}\n")
    w("static void test_the_version_agrees_with_itself(void)\n{")
    w("    char buf[32];\n    snprintf(buf, sizeof(buf), \"%d.%d.%d\", CYDUI_VERSION_MAJOR, CYDUI_VERSION_MINOR, CYDUI_VERSION_PATCH);")
    w("    TEST_ASSERT_EQUAL_STRING(CYDUI_VERSION_STRING, buf);\n    TEST_ASSERT_EQUAL_STRING(CYDUI_VERSION_STRING, cydui_version());\n}\n")
    w("static void test_the_structs_are_still_what_they_were(void)\n{\n    structs_have_their_fields();\n}\n")
    w("int main(void)\n{\n    UNITY_BEGIN();\n    RUN_TEST(test_every_function_is_there);\n    RUN_TEST(test_the_version_agrees_with_itself);\n    RUN_TEST(test_the_structs_are_still_what_they_were);\n    return UNITY_END();\n}")
    with open(out_path, "w") as f:
        f.write("\n".join(o) + "\n")
    print("%d functions, %d enumerators, %d constants, %d tunables, %d function-like macros, %d structs (%d without pointers) -> %s" %
          (len(funcs), len(enums), len(consts), len(tunables), len(set(fmacros)), len(stable), len(plain), out_path))


main()
