# Creates an interface implementation that forwards all calls to a member.
import argparse
import re

FUNCS_REGEX = re.compile(r'\s+([\w\s*&:]+\s+\**)(\w+)\(([\w\s*&,=:]*)\)')
ARG_REGEX = re.compile(r'.*[\s*&]([\w*&]+)')

# https://stackoverflow.com/a/241506
def comment_remover(text):
    def replacer(match):
        s = match.group(0)
        if s.startswith('/'):
            return " " # note: a space and not an empty string
        else:
            return s
    pattern = re.compile(
        r'//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
        re.DOTALL | re.MULTILINE
    )
    return re.sub(pattern, replacer, text)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('header_file')

    args = parser.parse_args()

    with open(args.header_file, 'r', encoding='utf-8', errors='replace') as f:
        header_text = f.read()

    # Remove comments
    header_text = comment_remover(header_text)

    # Remove access modifiers
    header_text = header_text.replace('public:', '')
    header_text = header_text.replace('protected:', '')
    header_text = header_text.replace('private:', '')

    # print(header_text)

    # Find all functions
    for return_type, name, args_str in re.findall(FUNCS_REGEX, header_text):
        return_type: str
        name: str
        args_str: str

        return_type = return_type.removeprefix('virtual ')

        wrap_func = f'virtual {return_type}{name}({args_str}) override {{ '

        if return_type.strip() != 'void':
            # Has return value
            wrap_func += 'return '

        # Parse arguments
        if len(args_str.strip()) > 0:
            arg_names = [ARG_REGEX.findall(x)[0] for x in args_str.split(',')]
        else:
            arg_names = []

        # Call the wrapper
        wrap_func += f'm_pBase->{name}('
        wrap_func += ', '.join(arg_names)
        wrap_func += '); }'

        print(wrap_func)

main()
