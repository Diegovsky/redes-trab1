
PRECMD := ""

client *args:
    @just compile 2>&1
    {{PRECMD}} build/client {{args}}

server *args:
    @just compile 2>&1
    {{PRECMD}} build/server {{args}}

gendb:
    just compile -t compdb > compile_commands.json

compile *args:
    @ninja --quiet -C build {{args}}
