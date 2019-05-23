def override_check(src):
    from pycparser import c_ast
    from pycparserext.ext_c_parser import GnuCParser, FuncDeclExt

    p = GnuCParser()
    ast = p.parse(src)

    # Create a set of override/overridable/override_default values
    entries = {}
    for ext in ast.ext:
        # Decl is in a different place depending on attribute before or after function name
        if type(ext) == c_ast.Decl:
            decl = ext
        elif type(ext) == c_ast.FuncDef:
            decl = ext.decl
        else:
            continue

        # Only working with Functions and not types
        if isinstance(decl.type, FuncDeclExt):
            # if the name has not been seen, create it
            if decl.name not in entries:
                entries[decl.name] = dict(override=0, overridable=0, override_default=0, static=0)

            # attributes can be in one of two places... so check both
            for funcspec in decl.funcspec:
                if funcspec.exprlist is not None:
                    for expr in funcspec.exprlist.exprs:
                        if expr.name in ('override', 'overridable', 'override_default'):
                            entries[decl.name][expr.name] = 1
            for expr in decl.type.attributes.exprs:
                if expr.name in ('override', 'overridable', 'override_default'):
                    entries[decl.name][expr.name] = 1

            # static overrides are not allowed
            for val in decl.storage:
                if val == 'static':
                    entries[decl.name]['static'] = 1

    # Override/Override_default without Overridable is a problem, as is static overrides
    invalid_override = 0
    for name in entries.keys():
        if entries[name]['override'] == 1 or entries[name]['override_default'] == 1:
            # Static is not allowed
            if entries[name]['static'] == 1:
                invalid_override = 1
                print('> %s: static detected on an override function' % name)

            # This is the problem case
            if entries[name]['overridable'] == 0:
                invalid_override = 1
                print('> %s: override function detected that is not overridable' % name)
    return invalid_override

def main():
    import sys
    src = sys.stdin.read()

    invalid_override = override_check(src)
    if invalid_override:
        sys.exit(1)


if __name__ == "__main__":
    main()
